#include "Yang/YangVRS.h"
#include "Debug/VRSDebug.h"

extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerClass("YangVRS", "Basic Lie Yang 2019 content adaptive shading", YangVRS::create);
    lib.registerClass("VRSDebug", "Shows a VRS texture as color", VRSDebug::create);
}
