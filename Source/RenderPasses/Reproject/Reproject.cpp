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
    reprojectPass = FullScreenPass::create("RenderPasses/Reproject/Reproject.slang");
    reprojectPass["sampler"] = Sampler::create(Sampler::Desc().setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border));
    copyPass = FullScreenPass::create("RenderPasses/Reproject/Copy.slang"); // shitty way to save previous frame
    copyPass["sampler"] = Sampler::create(Sampler::Desc().setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point));
}

RenderPassReflection Reproject::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addInput("input", "Texture to be reprojected with 1 frame delay.").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addInput("motion", "Screen-space motion between current and previous frame.").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("reproject", "Reprojected delayed input.").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("previous", "Previous frame storage.").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    return reflector;
}

void Reproject::execute(RenderContext* context, const RenderData& data)
{
  framebuffers->attachColorTarget(data["reproject"]->asTexture(), 0);
  reprojectPass["input2D"] = data["previous"]->asTexture();
  reprojectPass["motion2D"] = data["motion"]->asTexture();
  reprojectPass->execute(context, framebuffers);

  framebuffers->attachColorTarget(data["previous"]->asTexture(), 0);
  copyPass["input2D"] = data["input"]->asTexture();
  copyPass->execute(context, framebuffers);
}
