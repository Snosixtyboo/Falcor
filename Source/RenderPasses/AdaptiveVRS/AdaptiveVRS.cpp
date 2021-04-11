#include "Lie/LieVRS.h"
#include "Debug/VRSDebug.h"

extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerClass("LieVRS", "Basic Lie 2019 content adaptive shading", LieVRS::create);
    lib.registerClass("VRSDebug", "Shows a VRS texture as color", VRSDebug::create);
}
