#include "UnofficialFrank.hpp"


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

// align control knobs to centralize layout
Vec alignCtl = align + hpu2(-0.25f, 0);

void port(ModuleWidget* w, Module* m, Vec pos, int portId, bool isInput, const char* name) {
    auto offset = hpu2(0.0f, 0.0f);
	if(isInput) {
		w->addInput(createInput<PJ301MPort>(pos + offset + alignCtl, m, portId));
	} else {
		w->addOutput(createOutput<PJ301MPort>(pos + offset + alignCtl, m, portId));
	}
}

void button(ModuleWidget* w, Module* m, Vec pos, int buttId, int lightId, const char* name) {
    auto offset = hpu2(0.2f, 0.5f / 8 - 0.5f / 32);
	w->addParam(createParam<LEDButton>(pos + offset + alignCtl, m, buttId));
	// offset light from top corner
	w->addChild(createLight<MediumLight<GreenLight>>(pos + offset + alignCtl + Vec(4.4f, 4.4f), m, lightId));
}

void okNo(ModuleWidget* w, Module* m, Vec pos, int portId, const char* name) {
    auto offset = hpu2(0.5f, 0.5f / 4);
	w->addChild(createLight<MediumLight<GreenRedLight>>(pos + offset + alignCtl, m, portId));
}

void knob(ModuleWidget* w, Module* m, Vec pos, int paramId, const char* name) {
    auto offset = hpu2(-0.15f, -0.5f / 16);
	w->addParam(createParam<RoundBlackKnob>(pos + offset + alignCtl, m, paramId));
}
