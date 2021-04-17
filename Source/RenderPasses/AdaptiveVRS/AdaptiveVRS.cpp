#include "Yang/YangVRS.h"
#include "Debug/VRSDebug.h"
#include "Jaliborc/JaliVRS.h"

extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerClass("YangVRS", "Basic Lie Yang 2019 content adaptive shading", YangVRS::create);
    lib.registerClass("JaliVRS", "Our deep learning based VRS implementation", JaliVRS::create);
    lib.registerClass("VRSDebug", "Shows a VRS texture as color", VRSDebug::create);
}
