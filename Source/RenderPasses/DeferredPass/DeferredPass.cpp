#include "DeferredPass.h"

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerClass("DeferredPass", "Deferred rasterization.", DeferredPass::create);
}

const ChannelList GBuffers =
{
    { "depth",          "gDepth",           "depth buffer",                 false, ResourceFormat::D32Float},
    { "posW",           "gPosW",            "world space position",         true},
    { "normW",          "gNormW",           "world space normal",           true},
    { "diffuseOpacity", "gDiffuseOpacity",  "diffuse color",                true},
    { "specRough",      "gSpecRough",       "specular color",               true},
    { "emissive",       "gEmissive",        "emissive color",               true},
};

const D3D12_SHADING_RATE_COMBINER Combiners[] = {
    D3D12_SHADING_RATE_COMBINER_PASSTHROUGH,
    D3D12_SHADING_RATE_COMBINER_OVERRIDE
};

DeferredPass::DeferredPass()
{
    framebuffer = Fbo::create();
    pass = FullScreenPass::create("RenderPasses/DeferredPass/DeferredPass.slang");
    pass["gSampler"] = Sampler::create(Sampler::Desc().setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point));
    sceneBlock = ParameterBlock::create(pass->getProgram()->getReflector()->getParameterBlock("tScene"));
    vars = GraphicsVars::create(pass->getProgram()->getReflector());
}


void DeferredPass::setScene(RenderContext* context, const Scene::SharedPtr& scene)
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

RenderPassReflection DeferredPass::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    addRenderPassInputs(reflector, GBuffers);
    reflector.addInputOutput("output", "Shaded result").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addInput("visibility", "Visibility for shadowing").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addInput("vrs", "Variable rate shading image").flags(RenderPassReflection::Field::Flags::Optional);
    return reflector;
}

void DeferredPass::execute(RenderContext* context, const RenderData& data)
{
    if (scene) {
        pass["gPosW"] = data["posW"]->asTexture();
        pass["gNormW"] = data["normW"]->asTexture();
        pass["gDiffuse"] = data["diffuseOpacity"]->asTexture();;
        pass["gSpecRough"] = data["specRough"]->asTexture();
        pass["gEmissive"] = data["emissive"]->asTexture();
        pass["gVisibility"] = data["visibility"]->asTexture();
        pass["constants"]["cameraPosition"] = scene->getCamera()->getPosition();
        pass["constants"]["lightCount"] = numLights;
        pass["lights"] = lightsBuffer;

        ID3D12GraphicsCommandList5Ptr directX;
        d3d_call(context->getLowLevelData()->getCommandList()->QueryInterface(IID_PPV_ARGS(&directX)));
        directX->RSSetShadingRateImage(data["vrs"]->getApiHandle());
        directX->RSSetShadingRate(D3D12_SHADING_RATE_1X1, Combiners);

        framebuffer->attachColorTarget(data["output"]->asTexture(), 0);
        pass->execute(context, framebuffer);

        directX->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
        directX->RSSetShadingRateImage(nullptr);
    }
}
