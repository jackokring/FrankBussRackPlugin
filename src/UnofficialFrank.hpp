#include "rack.hpp"

using namespace rack;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin *pluginInstance;

// Forward-declare each Model, defined in each module source file
extern Model *modelFrankBussFormula;

// make those precise in hp and u rack units
float hp(float w, bool mhp = false) {
	return (mhp ? w - 1 : w) * RACK_GRID_WIDTH;
}

float u(float h, bool mhp = false) {
	return h * RACK_GRID_HEIGHT / 3.0f - (mhp ? RACK_GRID_WIDTH : 0);
}

Vec hpu(float w, float h, bool mhpw = false, bool mhph = false) {
	return Vec(hp(w, mhpw), u(h, mhph));
}

void screws(ModuleWidget* widget, float hp) {
    widget->addChild(createWidget<ScrewSilver>(hpu(1, 0))));
	widget->addChild(createWidget<ScrewSilver>(hpu(hp, 0, true)));
	widget->addChild(createWidget<ScrewSilver>(hpu(1, 3, false, true)));
	widget->addChild(createWidget<ScrewSilver>(hpu(hp, 3, true, true)));
}
