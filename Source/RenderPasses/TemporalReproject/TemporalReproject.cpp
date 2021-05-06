#include "TemporalReproject.h"

const char* TemporalReproject::desc = "Temporal reprojection of previous render using screen-space motion.";
extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("TemporalReproject", "Temporal reprojection of previous render using screen-space motion.", TemporalReproject::create);
}

extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

TemporalReproject::TemporalReproject()
{
    framebuffer = Fbo::create();
    shader = FullScreenPass::create("RenderPasses/TemporalReproject/TemporalReproject.slang");
    shader["sampler"] = Sampler::create(Sampler::Desc().setAddressingMode(Sampler::AddressMode::Border, Sampler::AddressMode::Border, Sampler::AddressMode::Border));
}

RenderPassReflection TemporalReproject::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    reflector.addInput("motion", "Screen-space motion vectors.");
    reflector.addOutput("target", "Data to be reprojected to next frame.");
    reflector.addOutput("dst", "Temporal reprojected texture.");
    return reflector;
}

void TemporalReproject::execute(RenderContext* context, const RenderData& data)
{
    framebuffer->attachColorTarget(data["dst"]->asTexture(), 0);
    shader["motion"] = data["motion"]->asTexture();
    shader["source"] = data["target"]->asTexture();
    shader->execute(context, framebuffer);
}
