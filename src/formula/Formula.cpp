/**
 *
 * Copyright (c) 2001, Frank Buß
 *
 * project: Formula
 * version: $Revision: 1.3 $ $Name:  $
 *
 * The Formula class is the facade class for accessing the fomula system.
 */

#include "Formula.h"
#include "Parser.h"

Formula::Formula()
{
	m_parser = new Parser("");
}

Formula::Formula(std::string expression)
{
	m_parser = new Parser(expression);
}

Formula::~Formula()
{
	delete m_parser;
}

void Formula::setExpression(std::string expression)
{
	m_parser->setExpression(expression);
}


void Formula::setVariable(std::string name, float_4* value)
{
	m_parser->setVariable(name, value);
}


float_4* Formula::getVariableAddress(std::string name)
{
	return m_parser->getVariableAddress(name);
}


void Formula::setFunction(std::string name, float_4(*function)())
{
	m_parser->setFunction(name, function);
}



void Formula::setFunction(std::string name, float_4(*function)(float_4))
{
	m_parser->setFunction(name, function);
}



void Formula::setFunction(std::string name, float_4(*function)(float_4, float_4))
{
	m_parser->setFunction(name, function);
}

float_4 Formula::eval()
{
	return m_parser->eval();
}
