#include "rack.hpp"

using namespace rack;
using namespace simd;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin *pluginInstance;

// Forward-declare each Model, defined in each module source file
extern Model *modelFrankBussFormula;

// make those precise in hp and u rack units
float hp(float w, bool mhp = false);
float u2(float h, bool mhp = false);
Vec hpu2(float w, float h, bool mhpw = false, bool mhph = false);
Vec alignedUtil();

void screws(ModuleWidget* widget, float hp);

void port(ModuleWidget* w, Module* m, Vec pos, int portId, bool isInput, const char* name);
void button(ModuleWidget* w, Module* m, Vec pos, int buttId, int lightId, const char* name);
void okNo(ModuleWidget* w, Module* m, Vec pos, int portId, const char* name);
void knob(ModuleWidget* w, Module* m, Vec pos, int paramId, const char* name);
void knobSmall(ModuleWidget* w, Module* m, Vec pos, int paramId, const char* name);

void setFilter(float_4 fc, float fs, float_4* f1, float_4* f2);
float_4 processFilter(float_4 in, float_4* buff, float_4 f1, float_4 f2);
float_4 fast_tan_pade55(float_4 x);
float_4 fast_sin_pade55(float_4 x);
float_4 fast_cos_pade55(float_4 x);
