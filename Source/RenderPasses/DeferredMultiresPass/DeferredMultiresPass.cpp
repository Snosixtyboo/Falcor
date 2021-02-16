#include "DeferredMultiresPass.h"
#include <filesystem>
#include <string>
#include <cstdlib>

const std::string kShaderFilename = "RenderPasses/DeferredMultiresPass/DeferredMultiresPass.slang";
const ChannelList kGBufferChannels =
{
    { "depth",          "gDepth",           "depth buffer",                 false, ResourceFormat::D32Float},
    { "posW",           "gPosW",            "world space position",         true},
    { "normW",          "gNormW",           "world space normal",           true},
    { "diffuseOpacity", "gDiffuseOpacity",  "diffuse color",                true},
    { "specRough",      "gSpecRough",       "specular color",               true},
    { "emissive",       "gEmissive",        "emissive color",               true},
};

const DeferredMultiresPass::ShadingRate kRates[3] =
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
    Sampler::Desc sampling;
    sampling.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);

    mpFbo = Fbo::create();
    mpPass = FullScreenPass::create(kShaderFilename);
    mpPass["gSampler"] = Sampler::create(sampling);

    ParameterBlockReflection::SharedConstPtr pReflection = mpPass->getProgram()->getReflector()->getParameterBlock("tScene");
    mpSceneBlock = ParameterBlock::create(pReflection);
    mpVars = GraphicsVars::create(mpPass->getProgram()->getReflector());
}


void DeferredMultiresPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    if (pScene) {
        mpScene = pScene;
        numLights = pScene->getLightCount();

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

            lightProbe = pScene->getLightProbe();
            if (lightProbe)
            {
                lightProbe->setShaderData(mpSceneBlock["lightProbe"]);
                mpVars->setParameterBlock("tScene", mpSceneBlock);
                mpPass["tScene"] = mpSceneBlock;
            }
        }
    }
}

DeferredMultiresPass::SharedPtr DeferredMultiresPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new DeferredMultiresPass);
    return pPass;
}

Dictionary DeferredMultiresPass::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection DeferredMultiresPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    addRenderPassInputs(reflector, kGBufferChannels);

    for (const auto& rate : kRates) {
      reflector.addInputOutput(rate.name, "Shading color").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    }

    reflector.addInput("visibility", "Visibility buffer used for shadowing").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addOutput("normalsOut", "Normalized world-space normal").format(ResourceFormat::RGBA8Unorm).texture2D(0, 0, 1);
    reflector.addOutput("viewNormalsOut", "View-space normals").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("extraOut", "Roughness, opacity and visibility in dedicated texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("motionOut", "Motion vectors").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    return reflector;
}

void DeferredMultiresPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (mpScene) {
        mpFbo->attachColorTarget(renderData["normalsOut"]->asTexture(), 1);
        mpFbo->attachColorTarget(renderData["viewNormalsOut"]->asTexture(), 2);
        mpFbo->attachColorTarget(renderData["extraOut"]->asTexture(), 3);
        mpFbo->attachColorTarget(renderData["motionOut"]->asTexture(), 4);

        for (int i = 1; i < 5; i++) {
            pRenderContext->clearRtv(mpFbo->getRenderTargetView(i).get(), float4(0,0,0,1));
        }

        mpPass["gPosW"] = renderData["posW"]->asTexture();
        mpPass["gNormW"] = renderData["normW"]->asTexture();;
        mpPass["gDiffuse"] = renderData["diffuseOpacity"]->asTexture();;
        mpPass["gSpecRough"] = renderData["specRough"]->asTexture();
        mpPass["gEmissive"] = renderData["emissive"]->asTexture();
        mpPass["gVisibility"] = renderData["visibility"]->asTexture();
        mpPass["lights"] = mpLightsBuffer;

        const auto& currVP = mpScene->getCamera()->getViewProjMatrix();
        auto pCB = mpPass["PerFrameCB"];

        pCB["lightCount"] = numLights;
        pCB["cameraPosition"] = mpScene->getCamera()->getPosition();
        pCB["world2View"] = mpScene->getCamera()->getViewMatrix();
        pCB["currVP"] = currVP;
        pCB["prevVP"] = prevVP;

        d3d_call(pRenderContext->getLowLevelData()->getCommandList()->QueryInterface(IID_PPV_ARGS(&directX)));
        for (const auto& rate : kRates) {
          directX->RSSetShadingRate(rate.id, nullptr);
          mpFbo->attachColorTarget(renderData[rate.name]->asTexture(), 0);
          mpPass->execute(pRenderContext, mpFbo);
        }

        prevVP = currVP;
    }
}
