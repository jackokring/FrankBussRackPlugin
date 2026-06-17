#include "UnofficialFrank.hpp"
#include "app/ModuleWidget.hpp"
#include "engine/Port.hpp"
#include "simd/Vector.hpp"
#include "widget/Widget.hpp"
#include <cmath>
#include <cstdio>
#include "formula/Formula.h"
#include "formula/Exception.h"

using namespace simd;

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
		CHANNELS_PARAM,
		LOWPASS_PARAM,
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
	float_4 phase[PORT_MAX_CHANNELS / 4];

	dsp::SchmittTrigger clampTrigger;
	dsp::SchmittTrigger bMinus1Trigger;
	dsp::SchmittTrigger b0Trigger;
	dsp::SchmittTrigger b1Trigger;
	int mux;

	//============================================
	// variable pointers
	//============================================
	float_4 formulaP;
	float_4 formulaK;
	float_4 formulaB;
	float_4 formulaW;
	float_4 formulaX;
	float_4 formulaY;
	float_4 formulaZ;

	//new
	float_4 formulaC;
	float_4 formulaF;
	float_4 formulaM;
	float_4 formulaL;
	float_4 formulaS;
	float_4 formulaR;

	// locals but time delayed for breaking loops of reference
	float_4 freqLast[PORT_MAX_CHANNELS / 4] = { 0.0f };

	// impulse filter based on glitch of parse error?
	float_4 filters[PORT_MAX_CHANNELS / 4] = { 0.0f };
	float_4 filterF1 = 0.0f;
	float_4 filterF2 = 0.0f;

	FrankBussFormulaModule() : compiled(false) {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configButton(B_MINUS_1_PARAM, "Variable 'b': -1");
		configButton(B_0_PARAM, "Variable 'b': 0");
		configButton(B_1_PARAM, "Variable 'b': 1");
		configParam(KNOB_PARAM, -1.0f, 1.0f, 0.0f, "Variable 'k'");
		// save a few cycles for the area and the miners
		configParam(CHANNELS_PARAM, 0.51f, PORT_MAX_CHANNELS + 0.49f, 1.0f, "Channels 'm'");
		configParam(LOWPASS_PARAM, -4.0f, 4.0f, 0.0f, "Lowpass 'l'");
		configButton(CLAMP_PARAM, "Clamp to -5V/+5V");

		configLight(ERROR_LIGHT, "Status:\n  green light: ok\n  red blinking light: error\n  -------------------------------\n ");

		configInput(X_INPUT, "Variable 'x'");
		configInput(Y_INPUT, "Variable 'y'");
		configInput(Z_INPUT, "Variable 'z'");
		configInput(W_INPUT, "Variable 'w'");
		configOutput(FORMULA_OUTPUT, "Result");
	}

	void process(const ProcessArgs &args) override {
	    //====================================================
	    // DSP critical section, lower dynamic if count
	    //====================================================
	    // lower branch predict loading
	    if((mux++ & 0xff) == 0) {
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
		}

		float deltaTime = args.sampleTime;

		// evaluate channels
		float chan = params[CHANNELS_PARAM].getValue();
		// already have channels from formulaM integer part
		int channels = (int)(chan + 0.5f);// in range [1, 16]
		// bug precesion completed level C
		int blocks = (channels + 3) / 4;

		int channelsW = max(inputs[W_INPUT].getChannels(), 1);
		int channelsX = max(inputs[X_INPUT].getChannels(), 1);
		int channelsY = max(inputs[Y_INPUT].getChannels(), 1);
		int channelsZ = max(inputs[Z_INPUT].getChannels(), 1);

		// evaluate lowpass filter multiplier of frequency
		float lowpass = powf(2.0f, params[LOWPASS_PARAM].getValue());

		if (compiled && compiling.try_lock()) {
// sort of as if using que(x) for crosstalk then ...
#pragma GCC ivdep
			for (int b = 0; b < blocks; blocks++) {
				try {
				    // set variables
					float_4 sub;
					formulaM = chan;
					formulaP = float_4(modff(phase[b][0], &sub[0]),
					    modff(phase[b][1], &sub[1]),
					    modff(phase[b][2], &sub[2]),
					    modff(phase[b][3], &sub[3]));
					formulaS = 2.0f * (sub - 0.5f);
					formulaK = params[KNOB_PARAM].getValue();
					formulaB = radiobutton;
					formulaW = float_4(inputs[W_INPUT].getVoltage((4 * b) % channelsW),
					    inputs[W_INPUT].getVoltage((4 * b + 1) % channelsW),
					    inputs[W_INPUT].getVoltage((4 * b + 2) % channelsW),
					    inputs[W_INPUT].getVoltage((4 * b + 3) % channelsW));
					formulaX = float_4(inputs[X_INPUT].getVoltage((4 * b) % channelsX),
					    inputs[X_INPUT].getVoltage((4 * b + 1) % channelsX),
					    inputs[X_INPUT].getVoltage((4 * b + 2) % channelsX),
					    inputs[X_INPUT].getVoltage((4 * b + 3) % channelsX));
					formulaY = float_4(inputs[Y_INPUT].getVoltage((4 * b) % channelsY),
					    inputs[Y_INPUT].getVoltage((4 * b + 1) % channelsY),
					    inputs[Y_INPUT].getVoltage((4 * b + 2) % channelsY),
					    inputs[Y_INPUT].getVoltage((4 * b + 3) % channelsY));
					formulaZ = float_4(inputs[Z_INPUT].getVoltage((4 * b) % channelsZ),
					    inputs[Z_INPUT].getVoltage((4 * b + 1) % channelsZ),
					    inputs[Z_INPUT].getVoltage((4 * b + 2) % channelsZ),
					    inputs[Z_INPUT].getVoltage((4 * b + 3) % channelsZ));

					// new
					formulaC = float_4(4 * b + 1, 4 * b + 2, 4 * b + 3, 4 * b + 4); // assign channel index * something
					formulaF = freqLast[b]; // frquency
					formulaL = freqLast[b] * lowpass;
					formulaR = args.sampleRate;

					if (freqFormulaEnabled) {
						auto freq = evalFormula(freqFormula);
						freqLast[b] = freq;// delay for next cycle
						phase[b] += freq * deltaTime;
						// branch predict worse case fail?
						// there's an instruction bit mask for that
						phase[b] = 2.0f * float_4(modff(phase[b][0] * 0.5f, &sub[0]),
							modff(phase[b][1] * 0.5f, &sub[1]),
							modff(phase[b][2] * 0.5f, &sub[2]),
							modff(phase[b][3] * 0.5f, &sub[3]));
					}

					float_4 val = evalFormula(formula);
					setFilter(formulaL, args.sampleRate, &filterF1, &filterF2);
					val = processFilter(val, &filters[b], filterF1, filterF2);
					// reduce preictor load by predicates
					val = doclamp * clamp(val, -5.0f, 5.0f) + val * !doclamp;
					outputs[FORMULA_OUTPUT].setVoltage(val[0], 4 * b);
					outputs[FORMULA_OUTPUT].setVoltage(val[1], 4 * b + 1);
					outputs[FORMULA_OUTPUT].setVoltage(val[2], 4 * b + 2);
					outputs[FORMULA_OUTPUT].setVoltage(val[3], 4 * b + 3);
					/* } catch (MathError&) {
					// ignore math errors, e.g. division by zero
					float_4 val = 0.0f;
					// good for impulse glitch control
					setFilter(freqLast[b] * lowpass, args.sampleRate, &filterF1, &filterF2);
					val = processFilter(val, &filters[b], filterF1, filterF2);
					outputs[FORMULA_OUTPUT].setVoltage(val[0], 4 * b);
					outputs[FORMULA_OUTPUT].setVoltage(val[1], 4 * b + 1);
					outputs[FORMULA_OUTPUT].setVoltage(val[2], 4 * b + 2);
					outputs[FORMULA_OUTPUT].setVoltage(val[3], 4 * b + 3); */
				} catch (exception&) {
					// for all other exceptions, set compiled to false, e.g. VariableNotFound
					compiled = false;
				}
			}
			compiling.unlock();
		} else {
#pragma GCC ivdep
			for (int b = 0; b < blocks; b++) {
				float_4 val = 0.0f;
				// good for impulse glitch control
    			setFilter(freqLast[b] * lowpass, args.sampleRate, &filterF1, &filterF2);
    			val = processFilter(val, &filters[b], filterF1, filterF2);
				outputs[FORMULA_OUTPUT].setVoltage(val[0], 4 * b);
				outputs[FORMULA_OUTPUT].setVoltage(val[1], 4 * b + 1);
				outputs[FORMULA_OUTPUT].setVoltage(val[2], 4 * b + 2);
				outputs[FORMULA_OUTPUT].setVoltage(val[3], 4 * b + 3);
			}
		}
		outputs[FORMULA_OUTPUT].setChannels(channels);


		// Blink light at 1Hz
		blinkPhase += deltaTime;
		// reduce branch prediction overhead
		if(((mux + 0x7f) & 0xff) == 0) {
    		if (blinkPhase >= 1.0f)
    			blinkPhase -= 1.0f;
    		if (compiled) {
    			lights[OK_LIGHT].value = 0;
    			lights[ERROR_LIGHT].value = 1;
    		} else {
    			lights[OK_LIGHT].value = (blinkPhase < 0.5f) ? 1.0f : 0.0f;
    			lights[ERROR_LIGHT].value = 0;
    		}
		}

		lights[CLAMP_LIGHT].value = (doclamp);
		lights[B_MINUS_1_LIGHT].value = (radiobutton == -1.0f);
		lights[B_0_LIGHT].value = (radiobutton == -0.0f);
		lights[B_1_LIGHT].value = (radiobutton == 1.0f);
	}

	float_4 pi = M_PI;
	float_4 e = M_E;

	void parseFormula(Formula& formula, std::string expr) {
		//============================================
		// variable definition defaults
		//============================================
		formula.setVariable("pi", &pi);
		formula.setVariable("e", &e);

		formula.setVariable("p", &formulaP);
		formula.setVariable("k", &formulaK);
		formula.setVariable("b", &formulaB);
		formula.setVariable("w", &formulaW);
		formula.setVariable("x", &formulaX);
		formula.setVariable("y", &formulaY);
		formula.setVariable("z", &formulaZ);

		// new
		formula.setVariable("c", &formulaC);// channel index
		formula.setVariable("f", &formulaF);// frequency (delayed by a sample)
		formula.setVariable("m", &formulaM);// number of channels-ish
		formula.setVariable("l", &formulaL);// LFO
		formula.setVariable("s", &formulaS);// sub oscillation bipolar
		formula.setVariable("r", &formulaR);// sample rate

		formula.setExpression(expr);
	}

	float_4 evalFormula(Formula& formula) {
		// eval
		float_4 val = formula.eval();
		// it appears to have less if statements than the original
		val = val * float_4(isfinite(val[0]), isfinite(val[1]), isfinite(val[2]), isfinite(val[3]));
		// now finite or NaN and jump[1] is 0.0f
		return ifelse(float_4(isnan(val[0]), isnan(val[1]), isnan(val[2]), isnan(val[3])), 0.0f, val);
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

		lcd(this, module, hpu2(0.5f, 1.0f), hpu2(10.0f, 2.57f), textField, TEXT, true);
		lcd(this, module, hpu2(1.7f, 3.7f), hpu2(8.8f, 0.5f), freqField, FREQ, false);

		button(this, module, hpu2(1.0f, 4.9f), FrankBussFormulaModule::B_MINUS_1_PARAM
			, FrankBussFormulaModule::B_MINUS_1_LIGHT, "-1");
		button(this, module, hpu2(2.0f, 4.5f), FrankBussFormulaModule::B_0_PARAM
			, FrankBussFormulaModule::B_0_LIGHT, "0");
		button(this, module, hpu2(3.0f, 4.9f), FrankBussFormulaModule::B_1_PARAM
			, FrankBussFormulaModule::B_1_LIGHT, "1");

		knob(this, module, hpu2(6.0f, 4.85f), FrankBussFormulaModule::KNOB_PARAM, "KNOB");

		knobSmall(this, module, hpu2(4.2f, 4.4f), FrankBussFormulaModule::CHANNELS_PARAM, "POLY");
		knobSmall(this, module, hpu2(7.7f, 4.4f), FrankBussFormulaModule::LOWPASS_PARAM, "LPF");

		okNo(this, module, hpu2(0.3f, 3.8f), FrankBussFormulaModule::ERROR_LIGHT, "E");

		button(this, module, hpu2(9.0f, 4.9f), FrankBussFormulaModule::CLAMP_PARAM
			, FrankBussFormulaModule::CLAMP_LIGHT, "CLMP");

		// bottomn row of ports
		port(this, module, hpu2(1.0f, 5.5f), FrankBussFormulaModule::W_INPUT, true, "W");
		port(this, module, hpu2(3.0f, 5.5f), FrankBussFormulaModule::X_INPUT, true, "X");
		port(this, module, hpu2(5.0f, 5.5f), FrankBussFormulaModule::Y_INPUT, true, "Y");
		port(this, module, hpu2(7.0f, 5.5f), FrankBussFormulaModule::Z_INPUT, true, "Z");
		port(this, module, hpu2(9.0f, 5.5f), FrankBussFormulaModule::FORMULA_OUTPUT, false, "OUT");
	}
};

Model *modelFrankBussFormula = createModel<FrankBussFormulaModule, FrankBussFormulaWidget>("Formula");
