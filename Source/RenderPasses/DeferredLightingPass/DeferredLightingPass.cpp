#include "DeferredLightingPass.h"
#include <filesystem>
#include <string>
#include <cstdlib>

const std::string kShaderFilename = "RenderPasses/DeferredLightingPass/DeferredLightingPass.slang";
const ChannelList DeferredLightingPass::kGBufferChannels =
{
    { "depth",          "gDepth",           "depth buffer",                 false               , ResourceFormat::D32Float},
    { "posW",           "gPosW",            "world space position",         true /* optional */, ResourceFormat::RGBA32Float },
    { "normW",          "gNormW",           "world space normal",           true /* optional */, ResourceFormat::RGBA32Float },
    { "tangentW",       "gTangentW",        "world space tangent",          true /* optional */, ResourceFormat::RGBA32Float },
    { "texC",           "gTexC",            "texture coordinates",          true /* optional */, ResourceFormat::RGBA32Float },
    { "diffuseOpacity", "gDiffuseOpacity",  "diffuse color",                true /* optional */, ResourceFormat::RGBA32Float },
    { "specRough",      "gSpecRough",       "specular color",               true /* optional */, ResourceFormat::RGBA32Float },
    { "emissive",       "gEmissive",        "emissive color",               true /* optional */, ResourceFormat::RGBA32Float },
    { "matlExtra",      "gMatlExtra",       "additional material data",     true /* optional */, ResourceFormat::RGBA32Float },
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("DeferredLightingPass", "Uses gbuffer data to compute a rasterized shading result.", DeferredLightingPass::create);
}

DeferredLightingPass::DeferredLightingPass()
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


void DeferredLightingPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
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

DeferredLightingPass::SharedPtr DeferredLightingPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new DeferredLightingPass);
    return pPass;
}

Dictionary DeferredLightingPass::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection DeferredLightingPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    addRenderPassInputs(reflector, kGBufferChannels);
    reflector.addInputOutput("color", "the color texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addInput("visibility", "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addOutput("normalsOut", "World-space normal, [0,1] range.").format(ResourceFormat::RGBA8Unorm).texture2D(0, 0, 1);
    reflector.addOutput("viewDirsOut", "View directions").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("viewNormalsOut", "View-space normals").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("roughOpacVisOut", "Roughness and opacity and visibility in dedicated texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("motionVecsOut", "Motion vectors").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    return reflector;
}

void DeferredLightingPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    const auto& output = renderData["color"]->asTexture();
    const auto& normalsOut = renderData["normalsOut"]->asTexture();
    const auto& viewDirsOut = renderData["viewDirsOut"]->asTexture();
    const auto& viewNormalsOut = renderData["viewNormalsOut"]->asTexture();
    const auto& roughOpacVisOut = renderData["roughOpacVisOut"]->asTexture();
    const auto& motionVecsOut = renderData["motionVecsOut"]->asTexture();

    mpFbo->attachColorTarget(output, 0);
    mpFbo->attachColorTarget(normalsOut, 1);
    mpFbo->attachColorTarget(viewDirsOut, 2);
    mpFbo->attachColorTarget(viewNormalsOut, 3);
    mpFbo->attachColorTarget(roughOpacVisOut, 4);
    mpFbo->attachColorTarget(motionVecsOut, 5);

    for (int i = 1; i < 6; i++) {
        pRenderContext->clearRtv(mpFbo->getRenderTargetView(i).get(), float4(0,0,0,1));
    }

    if (mpScene) {
        const auto& posW = renderData["posW"]->asTexture();
        const auto& normW = renderData["normW"]->asTexture();
        const auto& diffuseOpacity = renderData["diffuseOpacity"]->asTexture();
        const auto& specRough = renderData["specRough"]->asTexture();
        const auto& emissive = renderData["emissive"]->asTexture();
        const auto& visibility = renderData["visibility"]->asTexture();

        mpPass["gPosW"] = posW;
        mpPass["gNormW"] = normW;
        mpPass["gDiffuse"] = diffuseOpacity;
        mpPass["gSpecRough"] = specRough;
        mpPass["gEmissive"] = emissive;
        mpPass["gVisibility"] = visibility;

        auto pCB = mpPass["PerFrameCB"];
        pCB["lightCount"] = numLights;
        mpPass["lights"] = mpLightsBuffer;

        pCB["cameraPosition"] = mpScene->getCamera()->getPosition();
        pCB["world2View"] = mpScene->getCamera()->getViewMatrix();

        pCB["currVP"] = mpScene->getCamera()->getViewProjMatrix();
        pCB["prevVP"] = prevVP;

        mpPass->execute(pRenderContext, mpFbo);
    }
}
