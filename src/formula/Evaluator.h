/**
 *
 * Copyright (c) 2001, Frank Buﬂ
 *
 * project: Formula
 * version: $Revision: 1.3 $ $Name:  $
 *
 * Evaluator class and all Action classes.
 */

#ifndef EVALUATOR_H
#define EVALUATOR_H

#include <string>
#include <vector>
#include <map>
#include <math.h>
#include <float.h>
#include <rack.hpp>

using namespace std;
using namespace rack::simd;

typedef float_4(*NoArgumentFunction)();
typedef float_4(*OneArgumentFunction)(float_4);
typedef float_4(*TwoArgumentsFunction)(float_4, float_4);

class NumberStack : public vector<float_4>
{
public:
	NumberStack() : m_size(0) {}
	float_4 top();
	float_4 pop();
	void push(float_4 value);
	size_t size() { return m_size; }
private:
	vector<float_4> m_values;
	size_t m_size;
};


class Action
{
public:
	virtual ~Action() {};
	virtual void run(NumberStack& numberStack) {};
};


class NumberAction : public Action
{
public:
	NumberAction(float value) : m_value(value) {}
	NumberAction(std::string value);

	void run(NumberStack& numberStack) override;

private:
	float m_value;
};


class MulAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};


class DivAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};


class AddAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class LessAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class GreaterAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class LessEqualAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class GreaterEqualAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class EqualAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class NotEqualAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class AndAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class OrAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class NotAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class SubAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class NegAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class InvAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class PowerAction : public Action
{
public:
	void run(NumberStack& numberStack) override;
};

class Evaluator
{
public:
	~Evaluator();
	void addAction(Action* action);
	float_4 eval();
	void removeAllActions();
	void setVariable(std::string name, float_4* value);
	float_4 getVariable(std::string name);
	float_4* getVariableAddress(std::string name);

private:
	NumberStack m_numberStack;

	void deleteActions();

	vector<Action*> m_actions;
	map<std::string, float_4*> m_variables;
};


class VariableAction : public Action
{
public:
	VariableAction(Evaluator* evaluator, std::string name) : m_evaluator(evaluator), m_name(name), m_variableAddress(NULL) {}
	void run(NumberStack& numberStack) override;

private:
	Evaluator* m_evaluator;
	std::string m_name;
	float_4* m_variableAddress;
};


class NoArgumentFunctionAction : public Action
{
public:
	NoArgumentFunctionAction(Evaluator* evaluator, NoArgumentFunction function) : m_evaluator(evaluator), m_function(function) {}
	void run(NumberStack& numberStack) override;

private:
	Evaluator* m_evaluator;
	NoArgumentFunction m_function;
};

class OneArgumentFunctionAction : public Action
{
public:
	OneArgumentFunctionAction(Evaluator* evaluator, OneArgumentFunction function) : m_evaluator(evaluator), m_function(function) {}
	void run(NumberStack& numberStack) override;

private:
	Evaluator* m_evaluator;
	OneArgumentFunction m_function;
};

class TwoArgumentsFunctionAction : public Action
{
public:
	TwoArgumentsFunctionAction(Evaluator* evaluator, TwoArgumentsFunction function) : m_evaluator(evaluator), m_function(function) {}
	void run(NumberStack& numberStack) override;

private:
	Evaluator* m_evaluator;
	TwoArgumentsFunction m_function;
};


#endif
