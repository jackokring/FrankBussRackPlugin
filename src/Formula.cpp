#include "UnofficialFrank.hpp"
#include "app/ModuleWidget.hpp"
#include "engine/Port.hpp"
#include "widget/Widget.hpp"
#include <cmath>
#include <cstdio>
#include "formula/Formula.h"

//============================================
// Enum reporting front and centre
//============================================
enum TextFieldType {
	TEXT,
	FREQ,
	MAX_TEXT_COUNT
};

struct FrankBussFormulaModule : Module {
	enum ParamIds {
		KNOB_PARAM,
		CLAMP_PARAM,
		B_MINUS_1_PARAM,
		B_0_PARAM,
		B_1_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,
		Y_INPUT,
		Z_INPUT,
		W_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		FORMULA_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ERROR_LIGHT,
		OK_LIGHT,
		CLAMP_LIGHT,
		B_MINUS_1_LIGHT,
		B_0_LIGHT,
		B_1_LIGHT,
		NUM_LIGHTS
	};

	std::string textField;
	std::string freqField;
	float blinkPhase = 0.0f;

	Formula formula;
	Formula freqFormula;
	std::atomic<bool> compiled;
	// mutex for DSP try_lock()
	// so compilation not pulled by other threads
	std::mutex compiling;
	bool textDirty = false;
	bool freqDirty = false;
	bool doclamp = true;
	bool freqFormulaEnabled = false;
	float radiobutton = 0.0f;
	float phase[PORT_MAX_CHANNELS];

	dsp::SchmittTrigger clampTrigger;
	dsp::SchmittTrigger bMinus1Trigger;
	dsp::SchmittTrigger b0Trigger;
	dsp::SchmittTrigger b1Trigger;

	//============================================
	// variable pointers
	//============================================
	float* formulaP[MAX_TEXT_COUNT] = { NULL };
	float* formulaK[MAX_TEXT_COUNT] = { NULL };
	float* formulaB[MAX_TEXT_COUNT] = { NULL };
	float* formulaW[MAX_TEXT_COUNT] = { NULL };
	float* formulaX[MAX_TEXT_COUNT] = { NULL };
	float* formulaY[MAX_TEXT_COUNT] = { NULL };
	float* formulaZ[MAX_TEXT_COUNT] = { NULL };

	// new
	float* formulaC[MAX_TEXT_COUNT] = { NULL }; // channel index
	float* formulaF[MAX_TEXT_COUNT] = { NULL }; // frequency

	// locals but time delayed for breaking loops of reference
	float freqLast[PORT_MAX_CHANNELS] = { 0.0f };

	FrankBussFormulaModule() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configButton(B_MINUS_1_PARAM, "Variable 'b': -1");
		configButton(B_0_PARAM, "Variable 'b': 0");
		configButton(B_1_PARAM, "Variable 'b': 1");
		configParam(KNOB_PARAM, -1.0f, 1.0f, 0.0f, "Variable 'k'");
		configButton(CLAMP_PARAM, "Clamp to -5V/+5V");

		configLight(ERROR_LIGHT, "Status:\n  green light: ok\n  red blinking light: error\n  -------------------------------\n ");

		configInput(X_INPUT, "Variable 'x'");
		configInput(Y_INPUT, "Variable 'y'");
		configInput(Z_INPUT, "Variable 'z'");
		configInput(W_INPUT, "Variable 'w'");
		configOutput(FORMULA_OUTPUT, "Result");
	}

	int getMaxChannels() {
		int channels = inputs[W_INPUT].getChannels();
		channels = max(channels, inputs[X_INPUT].getChannels());
		channels = max(channels, inputs[Y_INPUT].getChannels());
		channels = max(channels, inputs[Z_INPUT].getChannels());
		if (channels == 0) channels = 1;
		return channels;
	}

	void processVariables(TextFieldType idx,
		int c, float k, float radiobutton, float w, float x, float y, float z, float freq) {
		// set all variables
		*formulaP[idx] = phase[c];
		*formulaK[idx] = k;
		*formulaB[idx] = radiobutton;
		*formulaW[idx] = w;
		*formulaX[idx] = x;
		*formulaY[idx] = y;
		*formulaZ[idx] = z;

		// new
		*formulaC[idx] = (float)c; // assign channel index to formulaC
		*formulaF[idx] = freq; // frquency
	}

	void process(const ProcessArgs &args) override {
		if (clampTrigger.process(params[CLAMP_PARAM].getValue())) {
			doclamp = !doclamp;
		}
		if (bMinus1Trigger.process(params[B_MINUS_1_PARAM].getValue())) {
			radiobutton = -1.0f;
		}
		if (b0Trigger.process(params[B_0_PARAM].getValue())) {
			radiobutton = 0.0f;
		}
		if (b1Trigger.process(params[B_1_PARAM].getValue())) {
			radiobutton = 1.0f;
		}

		float deltaTime = args.sampleTime;

		// evaluate frequency and output formula
		int channels = getMaxChannels();
		if (compiled && compiling.try_lock()) {
			for (int c = 0; c < channels; c++) {
				try {
				    float freq = freqLast[c];
					// get inputs
					float w = inputs[W_INPUT].getVoltage(c);
					float x = inputs[X_INPUT].getVoltage(c);
					float y = inputs[Y_INPUT].getVoltage(c);
					float z = inputs[Z_INPUT].getVoltage(c);

					// knob
					float k = params[KNOB_PARAM].getValue();

					processVariables(TEXT, c, k, radiobutton, w, x, y, z, freq);

					if (freqFormulaEnabled) {
						processVariables(FREQ, c, k, radiobutton, w, x, y, z, freq);
						// SO ...
						freq = evalFormula(freqFormula);
						freqLast[c] = freq;// delay for next cycle
						phase[c] += freq * deltaTime;
						if (phase[c] > 1.0f) phase[c] = fmodf(phase[c], 1.0f);
					}

					// OR ...
					float val = 0.0f;//evalFormula(formula);
					if (doclamp) val = clamp(val, -5.0f, 5.0f);
					outputs[FORMULA_OUTPUT].setVoltage(val, c);
				} catch (MathError&) {
					// ignore math errors, e.g. division by zero
					outputs[FORMULA_OUTPUT].setVoltage(0, c);
				} catch (exception&) {
					// for all other exceptions, set compiled to false, e.g. VariableNotFound
					compiled = false;
				}
			}
			compiling.unlock();
		} else {
			for (int c = 0; c < channels; c++) {
				outputs[FORMULA_OUTPUT].setVoltage(0, c);
			}
		}
		outputs[FORMULA_OUTPUT].setChannels(channels);


		// Blink light at 1Hz
		blinkPhase += deltaTime;
		if (blinkPhase >= 1.0f)
			blinkPhase -= 1.0f;
		if (compiled) {
			lights[OK_LIGHT].value = 0;
			lights[ERROR_LIGHT].value = 1;
		} else {
			lights[OK_LIGHT].value = (blinkPhase < 0.5f) ? 1.0f : 0.0f;
			lights[ERROR_LIGHT].value = 0;
		}

		lights[CLAMP_LIGHT].value = (doclamp);
		lights[B_MINUS_1_LIGHT].value = (radiobutton == -1.0f);
		lights[B_0_LIGHT].value = (radiobutton == -0.0f);
		lights[B_1_LIGHT].value = (radiobutton == 1.0f);
	}

	void parseFormula(Formula& formula, std::string expr) {
		//============================================
		// variable definition defaults
		//============================================
		formula.setVariable("pi", M_PI);
		formula.setVariable("e", M_E);

		formula.setVariable("p", 0);
		formula.setVariable("k", 0);
		formula.setVariable("b", 0);
		formula.setVariable("w", 0);
		formula.setVariable("x", 0);
		formula.setVariable("y", 0);
		formula.setVariable("z", 0);

		// new
		formula.setVariable("c", 0);// channel index
		formula.setVariable("f", 0);// frequency (delayed by a sample)

		formula.setExpression(expr);
	}

	float evalFormula(Formula& formula) {
		// eval
		float val = formula.eval();
		if (!isfinite(val) || isnan(val)) val = 0.0f;
		return val;
	}

	void compileVariables(TextFieldType idx, Formula formula) {
		formulaP[idx] = formula.getVariableAddress("p");
		formulaK[idx] = formula.getVariableAddress("k");
		formulaB[idx] = formula.getVariableAddress("b");
		formulaW[idx] = formula.getVariableAddress("w");
		formulaX[idx] = formula.getVariableAddress("x");
		formulaY[idx] = formula.getVariableAddress("y");
		formulaZ[idx] = formula.getVariableAddress("z");

		// new
		formulaC[idx] = formula.getVariableAddress("c");
		formulaF[idx] = formula.getVariableAddress("f");
	}

	void compile()
	{
		compiling.lock();
		compiled = false;
		for (int c = 0; c < PORT_MAX_CHANNELS; c++) phase[c] = 0;
		if (textField.size() > 0) {
			try {
				parseFormula(formula, textField);
				freqFormulaEnabled = false;
				if (freqField.size() > 0) {
					parseFormula(freqFormula, freqField);
					freqFormulaEnabled = true;
				}
				compileVariables(TEXT, formula);
				if(freqFormulaEnabled) compileVariables(FREQ, freqFormula);
				compiled = true;
			} catch (exception& e) {
				printf("formula exception: %s\n", e.what());
			}
		}
		compiling.unlock();
	}

	void onReset (const ResetEvent &e) override
	{
		textField = "";
		freqField = "";
		compile();
		textDirty = true;
		freqDirty = true;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		json_object_set_new(rootJ, "text", json_string(textField.c_str()));
		json_object_set_new(rootJ, "freq", json_string(freqField.c_str()));
		json_object_set_new(rootJ, "clamp", json_boolean(doclamp));
		json_object_set_new(rootJ, "button", json_real(radiobutton));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		json_t *textJ = json_object_get(rootJ, "text");
		if (textJ) textField = json_string_value(textJ);

		json_t *freqJ = json_object_get(rootJ, "freq");
		if (freqJ) freqField = json_string_value(freqJ);

		json_t *clampJ = json_object_get(rootJ, "clamp");
		if (clampJ) doclamp = json_is_true(clampJ);

		json_t *buttonJ = json_object_get(rootJ, "button");
		if (buttonJ) radiobutton = json_real_value(buttonJ);

		compile();
		textDirty = true;
		freqDirty = true;
	}

};

struct FormulaTextField : LedDisplayTextField {
	FrankBussFormulaModule* module;
	TextFieldType type;

	FormulaTextField() : LedDisplayTextField() {
		bgColor = nvgRGB(0x00, 0x00, 0x00);
	}

	void setModule(FrankBussFormulaModule* module) {
		this->module = module;
	}

	void setTextFieldType(TextFieldType type) {
		this->type = type;
	}

    void onChange(const event::Change &e) override {
		if (module) {
			if (type == TEXT) {
				module->textField = getText();
			} else if (type == FREQ) {
				module->freqField = getText();
			}
			module->compile();
		}
	}

	void step() override {
		LedDisplayTextField::step();
		if (module && (module->textDirty || module->freqDirty)) {
			if (type == TEXT) {
				setText(module->textField);
				module->textDirty = false;
			} else if (type == FREQ) {
				setText(module->freqField);
				module->freqDirty = false;
			}
		}
	}
};

void lcd(Widget* w, FrankBussFormulaModule* m, Vec pos, Vec size,
    FormulaTextField* field, TextFieldType type, bool multiline) {
        LedDisplay* textDisplay = createWidget<LedDisplay>(pos + alignedUtil());
		textDisplay->box.size = size;
		w->addChild(textDisplay);
		field = createWidget<FormulaTextField>(pos + alignedUtil());
		field->setModule(m);
		field->setTextFieldType(type);
		field->box.size = size;
		field->multiline = multiline;
		w->addChild(field);
}

#define WIDTH_MODULE 11

struct FrankBussFormulaWidget : ModuleWidget {
	FormulaTextField* textField;
	FormulaTextField* freqField;

	FrankBussFormulaWidget(FrankBussFormulaModule *module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/formula.svg")));
		screws(this, WIDTH_MODULE);

		lcd(this, module, hpu2(0.5f, 1.0f), hpu2(10.0f, 2.5f), textField, TEXT, true);
		lcd(this, module, hpu2(1.7f, 3.7f), hpu2(8.8f, 0.5f), freqField, FREQ, false);

		button(this, module, hpu2(1.0f, 5.0f), FrankBussFormulaModule::B_MINUS_1_PARAM
			, FrankBussFormulaModule::B_MINUS_1_LIGHT, "-1");
		button(this, module, hpu2(2.0f, 4.5f), FrankBussFormulaModule::B_0_PARAM
			, FrankBussFormulaModule::B_0_LIGHT, "0");
		button(this, module, hpu2(3.0f, 5.0f), FrankBussFormulaModule::B_1_PARAM
			, FrankBussFormulaModule::B_1_LIGHT, "1");

		knob(this, module, hpu2(6.0f, 4.75f), FrankBussFormulaModule::KNOB_PARAM, "KNOB");

		okNo(this, module, hpu2(0.3f, 3.75f), FrankBussFormulaModule::ERROR_LIGHT, "ERROR");

		button(this, module, hpu2(9.0f, 5.0f), FrankBussFormulaModule::CLAMP_PARAM
			, FrankBussFormulaModule::CLAMP_LIGHT, "CLAMP");

		// bottomn row of ports
		port(this, module, hpu2(1.0f, 5.5f), FrankBussFormulaModule::W_INPUT, true, "W");
		port(this, module, hpu2(3.0f, 5.5f), FrankBussFormulaModule::X_INPUT, true, "X");
		port(this, module, hpu2(5.0f, 5.5f), FrankBussFormulaModule::Y_INPUT, true, "Y");
		port(this, module, hpu2(7.0f, 5.5f), FrankBussFormulaModule::Z_INPUT, true, "Z");
		port(this, module, hpu2(9.0f, 5.5f), FrankBussFormulaModule::FORMULA_OUTPUT, false, "OUT");
	}
};

Model *modelFrankBussFormula = createModel<FrankBussFormulaModule, FrankBussFormulaWidget>("Formula");
