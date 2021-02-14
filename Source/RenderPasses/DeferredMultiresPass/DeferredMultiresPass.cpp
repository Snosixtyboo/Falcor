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

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("DeferredMultiresPass", "Uses gbuffer data to compute a rasterized shading result.", DeferredMultiresPass::create);
}

DeferredMultiresPass::DeferredMultiresPass()
{
    mpPass = FullScreenPass::create(kShaderFilename);
    mpFbo = Fbo::create();
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPass["gSampler"] = Sampler::create(samplerDesc);

    mpVars = GraphicsVars::create(mpPass->getProgram()->getReflector());
    ParameterBlockReflection::SharedConstPtr pReflection = mpPass->getProgram()->getReflector()->getParameterBlock("tScene");
    mpSceneBlock = ParameterBlock::create(pReflection);
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
    reflector.addInputOutput("color", "the color texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addInput("visibility", "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addOutput("normalsOut", "World-space normal, [0,1] range.").format(ResourceFormat::RGBA8Unorm).texture2D(0, 0, 1);
    reflector.addOutput("viewNormalsOut", "View-space normals").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("extraOut", "Roughness, opacity and visibility in dedicated texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("motionOut", "Motion vectors").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    return reflector;
}

void DeferredMultiresPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    mpFbo->attachColorTarget(renderData["color"]->asTexture(), 0);
    mpFbo->attachColorTarget(renderData["normalsOut"]->asTexture(), 1);
    mpFbo->attachColorTarget(renderData["viewNormalsOut"]->asTexture(), 2);
    mpFbo->attachColorTarget(renderData["extraOut"]->asTexture(), 3);
    mpFbo->attachColorTarget(renderData["motionOut"]->asTexture(), 4);

    for (int i = 1; i < 5; i++) {
        pRenderContext->clearRtv(mpFbo->getRenderTargetView(i).get(), float4(0,0,0,1));
    }

    if (mpScene) {
        mpPass["gPosW"] = renderData["posW"]->asTexture();
        mpPass["gNormW"] = renderData["normW"]->asTexture();;
        mpPass["gDiffuse"] = renderData["diffuseOpacity"]->asTexture();;
        mpPass["gSpecRough"] = renderData["specRough"]->asTexture();
        mpPass["gEmissive"] = renderData["emissive"]->asTexture();
        mpPass["gVisibility"] = renderData["visibility"]->asTexture();
        mpPass["lights"] = mpLightsBuffer;

        auto currVP = mpScene->getCamera()->getViewProjMatrix();
        auto pCB = mpPass["PerFrameCB"];

        pCB["lightCount"] = numLights;
        pCB["cameraPosition"] = mpScene->getCamera()->getPosition();
        pCB["world2View"] = mpScene->getCamera()->getViewMatrix();
        pCB["currVP"] = currVP;
        pCB["prevVP"] = prevVP;

        mpPass->execute(pRenderContext, mpFbo);
        prevVP = currVP;
    }
}
