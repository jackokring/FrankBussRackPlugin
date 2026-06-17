/**
 *
 * Copyright (c) 2001, Frank Buﬂ
 *
 * project: Formula
 * version: $Revision: 1.3 $ $Name:  $
 *
 * The Formula class is the facade class for accessing the fomula system.
 */

#ifndef FORMULA_H
#define FORMULA_H

#include <string>
#include <rack.hpp>

using namespace std;
using namespace rack::simd;

class Parser;

class Formula
{
public:
	Formula();
	Formula(std::string formula);
	~Formula();
	void setExpression(std::string expression);
	void setVariable(std::string name, float_4* value);
	float_4* getVariableAddress(std::string name);
	void setFunction(std::string name, float_4(*function)());
	void setFunction(std::string name, float_4(*function)(float_4));
	void setFunction(std::string name, float_4(*function)(float_4, float_4));
	float_4 eval();

private:
	Parser* m_parser;
};


#endif
