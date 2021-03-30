#include "AdaptiveVRS.h"
#include <filesystem>
#include <string>
#include <cstdlib>

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("AdaptiveVRS", "Sets shading rate based on rendering result.", AdaptiveVRS::create);
}

AdaptiveVRS::AdaptiveVRS()
{
    shader = ComputePass::create("RenderPasses/AdaptiveVRS/AdaptiveVRS.slang", "main");
}

RenderPassReflection AdaptiveVRS::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInput("color", "Input").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addOutput("rate", "Rate").bindFlags(ResourceBindFlags::UnorderedAccess).format(ResourceFormat::RGBA32Float); // todo change output format
    return reflector;
}

void AdaptiveVRS::compile(RenderContext* context, const CompileData& data)
{
    resolution = data.defaultTexDims;
    shader["constant"]["resolution"] = resolution;
}


void AdaptiveVRS::execute(RenderContext* context, const RenderData& data)
{
    shader["rate"] = data["rate"]->asTexture();
    shader["color"] = data["color"]->asTexture();
    shader->execute(context, resolution.x, resolution.y);
}
