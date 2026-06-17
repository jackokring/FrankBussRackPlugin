#include "UnofficialFrank.hpp"
#include "componentlibrary.hpp"
#include "helpers.hpp"
#include <cstring>

Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	// Add all Models defined throughout the plugin
	p->addModel(modelFrankBussFormula);

}

// make those precise in hp and u rack units
float hp(float w, bool mhp) {
	return (mhp ? w - 2 : w) * RACK_GRID_WIDTH;
}

float u2(float h, bool mhp) {
	return h * RACK_GRID_HEIGHT / 6.0f - (mhp ? RACK_GRID_WIDTH : 0);
}

Vec hpu2(float w, float h, bool mhpw, bool mhph) {
	return Vec(hp(w, mhpw), u2(h, mhph));
}

void screws(ModuleWidget* widget, float hp) {
    widget->addChild(createWidget<ScrewSilver>(hpu2(1, 0)));
	widget->addChild(createWidget<ScrewSilver>(hpu2(hp, 0, true)));
	widget->addChild(createWidget<ScrewSilver>(hpu2(1, 6, false, true)));
	widget->addChild(createWidget<ScrewSilver>(hpu2(hp, 6, true, true)));
}

// master alignment
Vec align = hpu2(0, -0.47f);

Vec alignedUtil() {
	return align;
}

float fontPad = 2.0f;

// align control knobs to centralize layout
Vec alignCtl = align + hpu2(0.55f, 0.25f);

struct LabelWidget : LightWidget {//TransparentWidget {
	const char *what;
	int kind;
	Vec textPos;
	float textSize;
	//const std::string fontPath = asset::system("res/fonts/DSEG7ClassicMini-Regular.ttf");

	LabelWidget(const char *p, Vec pos, const int k, const float size = 6) {
		what = p;
		kind = k;
		textPos = pos;
		textSize = size;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 /* || layer == 0 */) {
			drew(args);// lights on
		}
		Widget::drawLayer(args, layer);
	}

	void drew(const DrawArgs &args) {//foreground
		/* std::shared_ptr<Font> font;
		if (!(font = APP->window->loadFont(fontPath))) {
			return;
			} */
		NVGcolor textColor;
		switch(kind) {
			case -1:// IN
				textColor = nvgRGB(0, 255, 0);
				break;
			case 0: default: // NORMAL
				textColor = nvgRGB(0, 0, 255);
				break;
			case 1: // OUT
				textColor = nvgRGB(255, 0, 0);
				break;
		}
		//nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, textSize);
		nvgBeginPath(args.vg);
		nvgStrokeWidth(args.vg, 1.0f);
		nvgStrokeColor(args.vg, textColor);
		nvgFillColor(args.vg, textColor);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		float b[4];
		nvgTextBounds(args.vg, 0, 0, what, NULL, b);
		nvgRoundedRect(args.vg, b[0] - fontPad + textPos.x,
		    b[1] - fontPad + textPos.y,
		    b[2] - b[0] + 2 * fontPad,
		    b[3] - b[1] + 2 * fontPad,
		    2.0f);
		nvgStroke(args.vg);
		nvgStrokeWidth(args.vg, 2.0f);
		nvgText(args.vg, textPos.x, textPos.y, what, NULL);
	}
};

void port(ModuleWidget* w, Module* m, Vec pos, int portId, bool isInput, const char* name) {
	if(isInput) {
		w->addInput(createInputCentered<PJ301MPort>(pos + alignCtl, m, portId));
		w->addChild(new LabelWidget(name, pos + alignCtl + Vec(0, -20.0f), -1));
	} else {
		w->addOutput(createOutputCentered<PJ301MPort>(pos + alignCtl, m, portId));
		w->addChild(new LabelWidget(name, pos + alignCtl + Vec(0, -20.0f), 1));
	}
}

void button(ModuleWidget* w, Module* m, Vec pos, int buttId, int lightId, const char* name) {
	w->addParam(createParamCentered<LEDButton>(pos + alignCtl, m, buttId));
	w->addChild(createLightCentered<MediumLight<GreenLight>>(pos + alignCtl, m, lightId));
	w->addChild(new LabelWidget(name, pos + alignCtl + Vec(0, -17.0f), 0));
}

void okNo(ModuleWidget* w, Module* m, Vec pos, int portId, const char* name) {
	w->addChild(createLightCentered<MediumLight<GreenRedLight>>(pos + alignCtl, m, portId));
	w->addChild(new LabelWidget(name, pos + alignCtl + Vec(0, -14.0f), 0));
}

void knob(ModuleWidget* w, Module* m, Vec pos, int paramId, const char* name) {
	w->addParam(createParamCentered<RoundBlackKnob>(pos + alignCtl, m, paramId));
	w->addChild(new LabelWidget(name, pos + alignCtl + Vec(0, -23.0f), 0));
}

void knobSmall(ModuleWidget* w, Module* m, Vec pos, int paramId, const char* name) {
	w->addParam(createParamCentered<RoundSmallBlackKnob>(pos + alignCtl, m, paramId));
	w->addChild(new LabelWidget(name, pos + alignCtl + Vec(0, -18.0f), 0));
}

// Optimized [5/5] Padé Approximant for tan(x)
// High precision within the standard digital filter warping range (-1.5 < x < 1.5)
inline float fast_tan_pade55(float x) {
    float x2 = x * x;
    float num = x * (945.0f + x2 * (-105.0f + x2));
    float den = 945.0f + x2 * (-420.0f + 15.0f * x2);
    return num / den;
}

inline float fast_sin_pade55(float x) {
    float x2 = x * x;
    float num = x * (166320.0f + x2 * (-22260.0f + 551.0f * x2));
    float den = 166320.0f + x2 * (5460.0f + 75.0f * x2);
    return num / den;
}

inline float fast_cos_pade55(float x) {
    float x2 = x * x;
    float num = 1560.0f + x2 * (-610.0f + 22.0f * x2);
    float den = 1560.0f + x2 * (170.0f + x2);
    return num / den;
}

void setFilter(float fc, float fs, float* f1, float* f2) {
    fc = fmin(fc, fs / 2);
	*f1	 = fast_tan_pade55(M_PI * fc / fs);
	*f2   = 1 / (1 + *f1);
}

float processFilter(float in, float* buff, float f1, float f2) {
	float out = (f1 * in + *buff) * f2;
	*buff = f1 * (in - out) + out;
	return out;//lpf default
}
