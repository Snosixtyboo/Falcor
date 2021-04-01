#include "DeferredMultiresPass.h"
#include <filesystem>
#include <string>
#include <cstdlib>

const ChannelList GBufferChannels =
{
    { "depth",          "gDepth",           "depth buffer",                 false, ResourceFormat::D32Float},
    { "posW",           "gPosW",            "world space position",         true},
    { "normW",          "gNormW",           "world space normal",           true},
    { "diffuseOpacity", "gDiffuseOpacity",  "diffuse color",                true},
    { "specRough",      "gSpecRough",       "specular color",               true},
    { "emissive",       "gEmissive",        "emissive color",               true},
};

const DeferredMultiresPass::ShadingRate ShadingRates[7] =
{
    {D3D12_SHADING_RATE_4X4, "color4x4"},
    {D3D12_SHADING_RATE_4X2, "color4x2"},
    {D3D12_SHADING_RATE_2X4, "color2x4"},
    {D3D12_SHADING_RATE_2X2, "color2x2"},
    {D3D12_SHADING_RATE_2X1, "color2x1"},
    {D3D12_SHADING_RATE_1X2, "color1x2"},
    {D3D12_SHADING_RATE_1X1, "color1x1"}
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("DeferredMultiresPass", "Deferred rasterization at multiple shading rates.", DeferredMultiresPass::create);
}

DeferredMultiresPass::DeferredMultiresPass()
{
    framebuffers = Fbo::create();
    pass = FullScreenPass::create("RenderPasses/DeferredMultiresPass/DeferredMultiresPass.slang");
    pass["gSampler"] = Sampler::create(Sampler::Desc().setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point));

    sceneBlock = ParameterBlock::create(pass->getProgram()->getReflector()->getParameterBlock("tScene"));
    vars = GraphicsVars::create(pass->getProgram()->getReflector());
}


void DeferredMultiresPass::setScene(RenderContext* context, const Scene::SharedPtr& scene)
{
    if (scene) {
        this->scene = scene;
        numLights = scene->getLightCount();

        if (numLights)  {
            lightsBuffer = Buffer::createStructured(sizeof(LightData), (uint32_t)numLights, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
            lightsBuffer->setName("lightsBuffer");

            for (int l = 0; l < numLights; l++) {
                auto light = scene->getLight(l);
                if (!light->isActive()) continue;
                lightsBuffer->setElement(l, light->getData());
            }

            lightProbe = scene->getLightProbe();
            if (lightProbe) {
                lightProbe->setShaderData(sceneBlock["lightProbe"]);
                vars->setParameterBlock("tScene", sceneBlock);
                pass["tScene"] = sceneBlock;
            }
        }
    }
}

DeferredMultiresPass::SharedPtr DeferredMultiresPass::create(RenderContext* context, const Dictionary& dict)
{
    return SharedPtr(new DeferredMultiresPass);
}

RenderPassReflection DeferredMultiresPass::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    addRenderPassInputs(reflector, GBufferChannels);

    for (const auto& rate : ShadingRates)
        reflector.addInputOutput(rate.name, "Shading color").format(ResourceFormat::RGBA32Float).flags(RenderPassReflection::Field::Flags::Optional);

    reflector.addInput("visibility", "Visibility buffer used for shadowing").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addOutput("normalsOut", "Normalized world-space normal").format(ResourceFormat::RGBA8Unorm);
    reflector.addOutput("viewNormalsOut", "View-space normals").format(ResourceFormat::RGBA32Float);
    return reflector;
}

void DeferredMultiresPass::execute(RenderContext* context, const RenderData& data)
{
    if (scene) {
        framebuffers->attachColorTarget(data["viewNormalsOut"]->asTexture(), 1);

        pass["gPosW"] = data["posW"]->asTexture();
        pass["gNormW"] = data["normW"]->asTexture();;
        pass["gDiffuse"] = data["diffuseOpacity"]->asTexture();;
        pass["gSpecRough"] = data["specRough"]->asTexture();
        pass["gEmissive"] = data["emissive"]->asTexture();
        pass["gVisibility"] = data["visibility"]->asTexture();
        pass["lights"] = lightsBuffer;

        auto constants = pass["constants"];
        constants["cameraPosition"] = scene->getCamera()->getPosition();
        constants["world2View"] = scene->getCamera()->getViewMatrix();
        constants["lightCount"] = numLights;

        d3d_call(context->getLowLevelData()->getCommandList()->QueryInterface(IID_PPV_ARGS(&directX)));
        for (const auto& rate : ShadingRates) {
            for (int i = 1; i <= 1; i++)
                context->clearRtv(framebuffers->getRenderTargetView(i).get(), float4(0, 0, 0, 1));

            directX->RSSetShadingRate(rate.id, nullptr);
            framebuffers->attachColorTarget(data[rate.name]->asTexture(), 0);
            pass->execute(context, framebuffers);
        }
    }
}
