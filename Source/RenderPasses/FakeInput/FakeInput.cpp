#include "FakeInput.h"

const char* FakeInput::desc = "Provides an empty input texture.";
extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerClass("FakeInput", FakeInput::desc, FakeInput::create);
}

extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

RenderPassReflection FakeInput::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    reflector.addOutput("dst", "Empty texture.");
    return reflector;
}
