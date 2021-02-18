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

const DeferredMultiresPass::ShadingRate ShadingRates[3] =
{
    {D3D12_SHADING_RATE_4X4, "color4x4"},
    {D3D12_SHADING_RATE_2X2, "color2x2"},
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
    mpFbo = Fbo::create();
    mpPass = FullScreenPass::create("RenderPasses/DeferredMultiresPass/DeferredMultiresPass.slang");
    mpPass["gSampler"] = Sampler::create(Sampler::Desc().setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point));

    mpSceneBlock = ParameterBlock::create(mpPass->getProgram()->getReflector()->getParameterBlock("tScene"));
    mpVars = GraphicsVars::create(mpPass->getProgram()->getReflector());
}


void DeferredMultiresPass::setScene(RenderContext* context, const Scene::SharedPtr& scene)
{
    if (scene) {
        mpScene = scene;
        numLights = scene->getLightCount();

        if (numLights)
        {
            mpLightsBuffer = Buffer::createStructured(sizeof(LightData), (uint32_t)numLights, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
            mpLightsBuffer->setName("mpLightsBuffer");

            for (int l = 0; l < numLights; l++)
            {
                auto light = mpScene->getLight(l);
                if (!light->isActive()) continue;
                mpLightsBuffer->setElement(l, light->getData());
            }

            lightProbe = scene->getLightProbe();
            if (lightProbe)
            {
                lightProbe->setShaderData(mpSceneBlock["lightProbe"]);
                mpVars->setParameterBlock("tScene", mpSceneBlock);
                mpPass["tScene"] = mpSceneBlock;
            }
        }
    }
}

DeferredMultiresPass::SharedPtr DeferredMultiresPass::create(RenderContext* context, const Dictionary& dict)
{
    return SharedPtr(new DeferredMultiresPass);
}

Dictionary DeferredMultiresPass::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection DeferredMultiresPass::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    addRenderPassInputs(reflector, GBufferChannels);

    for (const auto& rate : ShadingRates)
        reflector.addInputOutput(rate.name, "Shading color").format(ResourceFormat::RGBA32Float).flags(RenderPassReflection::Field::Flags::Optional).texture2D(0, 0, 1);

    reflector.addInput("visibility", "Visibility buffer used for shadowing").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addOutput("normalsOut", "Normalized world-space normal").format(ResourceFormat::RGBA8Unorm).texture2D(0, 0, 1);
    reflector.addOutput("viewNormalsOut", "View-space normals").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("extraOut", "Roughness, opacity and visibility in dedicated texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("motionOut", "Motion vectors").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    return reflector;
}

void DeferredMultiresPass::execute(RenderContext* context, const RenderData& data)
{
    if (mpScene) {
        mpFbo->attachColorTarget(data["normalsOut"]->asTexture(), 1);
        mpFbo->attachColorTarget(data["viewNormalsOut"]->asTexture(), 2);
        mpFbo->attachColorTarget(data["extraOut"]->asTexture(), 3);
        mpFbo->attachColorTarget(data["motionOut"]->asTexture(), 4);

        mpPass["gPosW"] = data["posW"]->asTexture();
        mpPass["gNormW"] = data["normW"]->asTexture();;
        mpPass["gDiffuse"] = data["diffuseOpacity"]->asTexture();;
        mpPass["gSpecRough"] = data["specRough"]->asTexture();
        mpPass["gEmissive"] = data["emissive"]->asTexture();
        mpPass["gVisibility"] = data["visibility"]->asTexture();
        mpPass["lights"] = mpLightsBuffer;

        const auto& currVP = mpScene->getCamera()->getViewProjMatrix();
        auto pCB = mpPass["PerFrameCB"];

        pCB["lightCount"] = numLights;
        pCB["cameraPosition"] = mpScene->getCamera()->getPosition();
        pCB["world2View"] = mpScene->getCamera()->getViewMatrix();
        pCB["currVP"] = currVP;
        pCB["prevVP"] = prevVP;

        d3d_call(context->getLowLevelData()->getCommandList()->QueryInterface(IID_PPV_ARGS(&directX)));
        for (const auto& rate : ShadingRates) {
            for (int i = 1; i < 5; i++)
                context->clearRtv(mpFbo->getRenderTargetView(i).get(), float4(0, 0, 0, 1));

            directX->RSSetShadingRate(rate.id, nullptr);
            mpFbo->attachColorTarget(data[rate.name]->asTexture(), 0);
            mpPass->execute(context, mpFbo);
        }

        prevVP = currVP;
    }
}
