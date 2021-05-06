#include "Yang/YangVRS.h"
#include "Debug/VRSDebug.h"
#include "Jaliborc/JaliVRS.h"

extern "C" __declspec(dllexport) const char* getProjDir() { return PROJECT_DIR; }
extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerClass("YangVRS", YangVRS::desc, YangVRS::create);
    lib.registerClass("JaliVRS", JaliVRS::desc, JaliVRS::create);
    lib.registerClass("VRSDebug", VRSDebug::desc, VRSDebug::create);
}

const char* YangVRS::desc = "Basic Lie Yang 2019 content adaptive shading";
const char* JaliVRS::desc = "Our deep learning based adaptive shading implementation";
const char* VRSDebug::desc = "Shows a VRS texture overlay";
