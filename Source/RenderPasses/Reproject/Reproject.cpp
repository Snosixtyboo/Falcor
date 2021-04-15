#include "Reproject.h"

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("Reproject", "Reprojects previous render using screen-space motion.", Reproject::create);
}

Reproject::Reproject()
{
    framebuffer = Fbo::create();
    shader = FullScreenPass::create("RenderPasses/Reproject/Reproject.slang");
    shader["sampler"] = Sampler::create(Sampler::Desc().setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border));
}

RenderPassReflection Reproject::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    reflector.addInput("motion", "Screen-space motion vectors.");
    reflector.addOutput("buffer", "Texture to be reprojected next frame.");
    reflector.addOutput("output", "Reprojected texture.");
    return reflector;
}

void Reproject::execute(RenderContext* context, const RenderData& data)
{
    framebuffer->attachColorTarget(data["output"]->asTexture(), 0);
    shader["buffer"] = data["buffer"]->asTexture();
    shader["motion"] = data["motion"]->asTexture();
    shader->execute(context, framebuffer);
}
