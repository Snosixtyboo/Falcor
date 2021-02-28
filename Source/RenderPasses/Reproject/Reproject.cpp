#include "Reproject.h"
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
    lib.registerClass("Reproject", "Reprojects previous render using screen-space motion.", Reproject::create);
}

Reproject::Reproject()
{
    framebuffers = Fbo::create();
    shader = FullScreenPass::create("RenderPasses/Reproject/Reproject.slang");
    shader["sampler"] = Sampler::create(Sampler::Desc().setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border));
}

RenderPassReflection Reproject::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInput("input", "Texture to be reprojected with 1 frame delay.");
    reflector.addInput("motion", "Screen-space motion between current and previous frame.");
    reflector.addInternal("previous", "Previous frame storage.").format(ResourceFormat::RGB32Float);
    reflector.addOutput("output", "Reprojected delayed input.");
    return reflector;
}

void Reproject::execute(RenderContext* context, const RenderData& data)
{
    framebuffers->attachColorTarget(data["output"]->asTexture(), 0);
    shader["input2D"] = data["previous"]->asTexture();
    shader["motion2D"] = data["motion"]->asTexture();
    shader->execute(context, framebuffers);

    context->blit(data["input"]->asTexture()->getSRV(), data["previous"]->asTexture()->getRTV(), uint4(-1), uint4(-1), Sampler::Filter::Point); // save frame for next render
}
