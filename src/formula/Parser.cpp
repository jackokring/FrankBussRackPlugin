/**
 *
 * Copyright (c) 2001, Frank Buﬂ
 *
 * project: Formula
 * version: $Revision: 1.3 $ $Name:  $
 *
 * Parser class.
 */

#include "Token.h"
#include "Parser.h"
#include "Exception.h"
#include "../UnofficialFrank.hpp"
#include "engine/Port.hpp"
#include "simd/Vector.hpp"

#include <climits>
#include <cmath>
#include <math.h>
#include <random>

float_4 ParserTanp(float_4 x)
{
	return fast_tan_pade55(x * M_PI);
}

float_4 ParserSinp(float_4 x)
{
	return fast_sin_pade55(x * M_PI);
}

float_4 ParserCosp(float_4 x)
{
	return fast_cos_pade55(x * M_PI);
}


// randoms
std::normal_distribution<float> noise;
std::exponential_distribution<float> expo;
std::poisson_distribution<int> poisson(1.0f);
std::linear_congruential_engine<unsigned int, 3, 561, 0> noiseEngine;

float_4 ParserNoise(float_4 gain)
{
	return noise(noiseEngine) * gain;
}

float_4 ParserExpo(float_4 gain)
{
	return expo(noiseEngine) * gain;
}

float_4 ParserPoisson(float_4 gain)
{
	return poisson(noiseEngine) * gain;
}

float_4 ParserUni(float_4 max)
{
	return ((float)noiseEngine() / UINT_MAX) * max;
}

float_4 ParserPar(float_4 phase)
{
	return 4.0f * phase * (1.0f - phase);
}

// ring buffer
float_4 ring[PORT_MAX_CHANNELS / 4] = { 0.0f };
int head = 0;
int tail = 0;

float_4 ParserRing(float_4 value)
{
	ring[head] = value;
	head = (head + 1) % (PORT_MAX_CHANNELS / 4);
	return value;
}

float_4 ParserResolve(float_4 gain)
{
	return ring[tail++] * gain;
}

Parser::Parser(std::string expression)
{
   	//============================================
	// functions (ggod enough for audio work)
	//============================================
	setFunction("acos", [](float_4 x) {
	    return atan2(sqrt(fmax(1.0f - x * x, 0.0f)), x);
	});
	setFunction("asin", [](float_4 x) {
	    return atan2(x, sqrt(fmax(1.0f - x * x, 0.0f)));
	});
	setFunction("atan", atan);
	setFunction("atan2", atan2);
	setFunction("cos", cos);
	setFunction("cosh", [](float_4 x) {
	    auto y = exp(x);
		return 0.5f * (y + rcp(y));
	});
	setFunction("exp", exp);
	setFunction("abs", fabs);
	setFunction("mod", fmod);
	setFunction("log", log);
	setFunction("log2", log2);
	setFunction("log10", log10);
	setFunction("pow", pow);
	setFunction("sin", sin);
	setFunction("sinh", [](float_4 x) {
	    auto y = exp(x);
		return 0.5f * (y - rcp(y));
	});
	setFunction("tan", tan);
	setFunction("tanh", [](float_4 x) {
	    auto y = exp(x);
	    auto z = rcp(y);
		return (y - z) / (y + z);
	});
	setFunction("sqrt", sqrt);
	setFunction("ceil", ceil);
	setFunction("floor", floor);
	// let's keep it float people!
	setFunction("max", fmax);
	setFunction("min", fmin);

	// oldies
	setFunction("expm1", [](float_4 x) {
	    return float_4(expm1(x[0]), expm1(x[1]), expm1(x[2]), expm1(x[3]));
	});
	setFunction("log1p", [](float_4 x) {
	    return float_4(log1p(x[0]), log1p(x[1]), log1p(x[2]), log1p(x[3]));
	});

	// new
	setFunction("par", ParserPar);// Parabolic
	setFunction("que", ParserRing);// Queue
	setFunction("unq", ParserResolve);// Unqueue
	setFunction("nor", ParserNoise);// Normal distribution
	setFunction("uni", ParserUni);// Uniform distribution
	setFunction("exd", ParserExpo);// Exponential distribution
	setFunction("poi", ParserPoisson);// Poisson distribution

	setFunction("tanp", ParserTanp);// Tan pade 5/5
	setFunction("sinp", ParserSinp);// Sin pade 5/5
	setFunction("cosp", ParserCosp);// Cos pade 5/5
	setFunction("cbrt", [](float_4 x) {
	    return float_4(cbrt(x[0]), cbrt(x[1]), cbrt(x[2]), cbrt(x[3]));
	});// Cube root

	setExpression(expression);
}


Parser::~Parser()
{
	deleteTokens();
}


char Parser::peekChar()
{
	if (m_currentIndex < (int) m_expression.size()) return m_expression[m_currentIndex];
	return 0;
}


void Parser::skipChar()
{
	m_currentIndex++;
}


char Parser::skipAndPeekChar()
{
	skipChar();
	return peekChar();
}


Token* Parser::peekToken()
{
	if (m_currentTokenIndex < (int) m_tokens.size()) return m_tokens[m_currentTokenIndex];
	return NULL;
}


Token* Parser::peekLastToken()
{
	Token* lastToken = NULL;
	if (m_currentTokenIndex > 0) lastToken = m_tokens[m_currentTokenIndex - 1];
	return lastToken;
}


Token* Parser::peekNextToken()
{
	Token* nextToken = NULL;
	if (m_currentTokenIndex + 1 < (int) m_tokens.size()) nextToken = m_tokens[m_currentTokenIndex + 1];
	return nextToken;
}


void Parser::skipToken()
{
	m_currentTokenIndex++;
}


std::string Parser::parseNumber(char c)
{
	std::string number;
	if (c != '.')
	{
		// parse before '.'
		while (c != 0 && c >= '0' && c <= '9' && c != '.' && c != 'e' && c != 'E') {
			number += c;
			c = skipAndPeekChar();
		}
	}
	if (c == '.')
	{
		// parse after '.'
		number += c;
		c = skipAndPeekChar();
		if (c != 0 && c >= '0' && c <= '9') {
			while (c != 0 && c >= '0' && c <= '9' && c != 'e' && c != 'E') {
				number += c;
				c = skipAndPeekChar();
			}
		} else {
			throw SyntaxError("Expected digit after '.', number: " + number);
		}
	}
	if (c == 'e' || c == 'E')
	{
		// parse after 'e' or 'E'
		number += c;
		c = skipAndPeekChar();
		if (c == '+' || c == '-') {
			number += c;
			c = skipAndPeekChar();
		}
		while (c != 0 && c >= '0' && c <= '9') {
			number += c;
			c = skipAndPeekChar();
		}
	}
	return number;
}


std::string Parser::parseIdentifier(char c)
{
	std::string identifier;
	identifier += c;
	c = skipAndPeekChar();
	while (c != 0 && ((c >= 'a' && c <= 'z')
	                  || (c >= 'A' && c <= 'Z')
	                  || (c >= '0' && c <= '9')
	                  || c == '_'))
	{
		identifier += c;
		c = skipAndPeekChar();
	}
	return identifier;
}


void Parser::deleteTokens()
{
	for (int i = 0; i < (int) m_tokens.size(); i++) delete m_tokens[i];
	m_tokens.clear();
}


void Parser::setExpression(std::string expression)
{
	m_expression = std::string("(") + expression + ")";

	m_postfix = "";
	m_evaluator.removeAllActions();
	m_functionArgumentCountStack = stack<int>();
	m_operators = stack<Token*>();
	deleteTokens();

	m_currentIndex = 0;
	char c;
	Token* token;
	while ((c = peekChar())) {
		token = NULL;
		switch (c) {
		case '&':
			token = new AndToken();
			skipChar();
			break;
		case '|':
			token = new OrToken();
			skipChar();
			break;
		case '=':
			token = new EqualToken();
			skipChar();
			break;
		case '!':
			skipChar();
			if (peekChar() == '=') {
				skipChar();
				token = new NotEqualToken();
			} else {
				token = new NotToken();
			}
			break;
		case '<':
			skipChar();
			if (peekChar() == '=') {
				skipChar();
				token = new LessEqualToken();
			} else {
				token = new LessToken();
			}
			break;
		case '>':
			skipChar();
			if (peekChar() == '=') {
				skipChar();
				token = new GreaterEqualToken();
			} else {
				token = new GreaterToken();
			}
			break;
		case '+':
			token = new AddToken();
			skipChar();
			break;
		case '-':
			token = new SubToken();
			skipChar();
			break;
		case '*':
			token = new MulToken();
			skipChar();
			break;
		case '/':
			token = new DivToken();
			skipChar();
			break;
		case '^':
			token = new PowerToken();
			skipChar();
			break;
		case '(':
			token = new OpenBracketToken();
			skipChar();
			break;
		case ')':
			token = new CloseBracketToken();
			skipChar();
			break;
		case ',':
			token = new CommaToken();
			skipChar();
			break;
		default:
			if ((c >= '0' && c <= '9') || c == '.') {
				token = new NumberToken(parseNumber(c));
			} else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
				token = new IdentifierToken(parseIdentifier(c));
			} else if (c == 9 || c == 10 || c == 13 || c == 32) {
				skipChar();
				continue;
			} else {
				skipChar();
				throw SyntaxError(std::string("Invalid character: ") + c);
			}
		}
		if (token) m_tokens.push_back(token);
	}

	m_currentTokenIndex = 0;
	while ((token = peekToken())) token->eval(*this);
	if (m_operators.size() > 0) throw SyntaxError("Missing ')'.");
	if (m_postfix.size() > 0) m_postfix = m_postfix.substr(1);
}

void Parser::setFunction(std::string name, float_4(*function)())
{
	m_noArgumentFunctions[name] = function;
}

void Parser::setFunction(std::string name, float_4(*function)(float_4))
{
	m_oneArgumentFunctions[name] = function;
}

void Parser::setFunction(std::string name, float_4(*function)(float_4, float_4))
{
	m_twoArgumentsFunctions[name] = function;
}

NoArgumentFunction Parser::getNoArgumentFunction(std::string name)
{
	NoArgumentFunction function = m_noArgumentFunctions[name];
	if (function) {
		return function;
	} else {
		throw FunctionNotFound(name);
	}
}

OneArgumentFunction Parser::getOneArgumentFunction(std::string name)
{
	OneArgumentFunction function = m_oneArgumentFunctions[name];
	if (function) {
		return function;
	} else {
		throw FunctionNotFound(name);
	}
}

TwoArgumentsFunction Parser::getTwoArgumentsFunction(std::string name)
{
	TwoArgumentsFunction function = m_twoArgumentsFunctions[name];
	if (function) {
		return function;
	} else {
		throw FunctionNotFound(name);
	}
}
