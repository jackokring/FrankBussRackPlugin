/**
 *
 * Copyright (c) 2001, Frank Buﬂ
 *
 * project: Formula
 * version: $Revision: 1.3 $ $Name:  $
 *
 * Evaluator class and all Action classes.
 */

#include "Evaluator.h"
#include "Exception.h"
#include <cstdio>

using namespace std;

float NumberStack::top()
{
	if (size() == 0) throw StackUnderflow();
	return m_values[m_size - 1];
}

float NumberStack::pop()
{
	if (size() == 0) throw StackUnderflow();
	return m_values[--m_size];
}

void NumberStack::push(float value)
{
	m_size++;
	if (m_size > m_values.size()) {
		m_values.push_back(value);
	} else {
		m_values[m_size - 1] = value;
	}
}


void Action::checkTopStackElement(NumberStack& numberStack)
{
	if (!isfinite(numberStack.top()) || isnan(numberStack.top())) throw MathError();
}

NumberAction::NumberAction(std::string value)
{
	m_value = atof(value.c_str());
}

void NumberAction::run(NumberStack& numberStack)
{
	numberStack.push(m_value);
	checkTopStackElement(numberStack);
}



void MulAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(op1 * op2);
	checkTopStackElement(numberStack);
}


void DivAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	if (op2 == 0.0f) {
		throw MathError();
	}
	numberStack.push(op1 / op2);
	checkTopStackElement(numberStack);
}


void AddAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(op1 + op2);
	checkTopStackElement(numberStack);
}

void LessAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(op1 < op2);
	checkTopStackElement(numberStack);
}

void GreaterAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(op1 > op2);
	checkTopStackElement(numberStack);
}

void LessEqualAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(op1 <= op2);
	checkTopStackElement(numberStack);
}

void GreaterEqualAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(op1 >= op2);
	checkTopStackElement(numberStack);
}

void EqualAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	//============================================
	// equality overlap of floats
	//============================================
	numberStack.push(max(1.0f - abs(op1 - op2), 0.0f));// a flexible float equality test
	checkTopStackElement(numberStack);
}

void NotEqualAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	//============================================
	// inequality overlap of floats
	//============================================
	numberStack.push(min(abs(op1 - op2), 1.0f));// a flexible float inequality test
	checkTopStackElement(numberStack);
}

void AndAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(op1 && op2);
	checkTopStackElement(numberStack);
}

void OrAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(op1 || op2);
	checkTopStackElement(numberStack);
}

void NotAction::run(NumberStack& numberStack)
{
	numberStack.push(!numberStack.pop());
	checkTopStackElement(numberStack);
}

void SubAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(op1 - op2);
	checkTopStackElement(numberStack);
}

void NegAction::run(NumberStack& numberStack)
{
	numberStack.push(-numberStack.pop());
	checkTopStackElement(numberStack);
}

void PowerAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(pow(op1, op2));
	checkTopStackElement(numberStack);
}

Evaluator::~Evaluator()
{
	deleteActions();
	for (auto it = m_variables.begin(); it != m_variables.end(); it++) {
	    // why pointer to float as value better and faster and no heap allocation
		//delete it->second;
	}
}

void Evaluator::addAction(Action* action)
{
	m_actions.push_back(action);
}

float Evaluator::eval()
{
	if (m_actions.size() == 0) return 0;
	m_numberStack.clear();
	printf("que: %d", (int) m_actions.size());
	for (int i = 0; i < (int) m_actions.size(); i++) {
	    // GDB say this line fails SEGFAULT
		/*
malloc(): unaligned tcache chunk detected

Thread 1 "Rack" received signal SIGABRT, Aborted.
0x00007ffff6aa095c in ?? () from /lib/x86_64-linux-gnu/libc.so.6
		 */
		// tried p 1 7 all single alphanumeric, so not variable specific
		// even commented the line that supposed gave an error
		m_actions[i]->run(m_numberStack);
	}
	return m_numberStack.pop();
}

void Evaluator::removeAllActions()
{
	deleteActions();
	m_actions.clear();
}

void Evaluator::setVariable(std::string name, float value)
{
	auto i = m_variables.find(name);
	if (i == m_variables.end()) {
		m_variables[name] = 0.0f; //new float;
	}
	*getVariableAddress(name) = value;
}

float Evaluator::getVariable(std::string name)
{
	return *getVariableAddress(name);
}

float* Evaluator::getVariableAddress(std::string name)
{
	auto i = m_variables.find(name);
	if (i != m_variables.end()) {
		return &i->second;
	} else {
		throw VariableNotFound(name);
	}
}

void Evaluator::deleteActions()
{
	for (int i = 0; i < (int) m_actions.size(); i++) delete m_actions[i];
}


void VariableAction::run(NumberStack& numberStack)
{
	if (!m_variableAddress) m_variableAddress = m_evaluator->getVariableAddress(m_name);
	numberStack.push(*m_variableAddress);
	checkTopStackElement(numberStack);
}


void NoArgumentFunctionAction::run(NumberStack& numberStack)
{
	numberStack.push(m_function());
	checkTopStackElement(numberStack);
}

void OneArgumentFunctionAction::run(NumberStack& numberStack)
{
	numberStack.push(m_function(numberStack.pop()));
	checkTopStackElement(numberStack);
}

void TwoArgumentsFunctionAction::run(NumberStack& numberStack)
{
	float op2 = numberStack.pop();
	float op1 = numberStack.pop();
	numberStack.push(m_function(op1, op2));
	checkTopStackElement(numberStack);
}
