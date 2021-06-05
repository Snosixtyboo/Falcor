#include "DeferredMultiresPass.h"

const char* DeferredMultiresPass::desc = "Deferred lighting pass at multiple shading rates.";
extern "C" __declspec(dllexport) const char* getProjDir() { return PROJECT_DIR; }
extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerClass("DeferredMultiresPass", DeferredMultiresPass::desc, DeferredMultiresPass::create);
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

void DeferredMultiresPass::setScene(RenderContext* context, const Scene::SharedPtr& scene)
{
    if (scene) {
        this->scene = scene;
        int numLights = scene->getLightCount();

        pass = FullScreenPass::create("RenderPasses/DeferredMultiresPass/DeferredMultiresPass.slang", scene->getSceneDefines());
        pass["gSampler"] = Sampler::create(Sampler::Desc().setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point));
        pass["constants"]["numLights"] = numLights;

        if (numLights) {
            auto lights = Buffer::createStructured(sizeof(LightData), numLights, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
            lights->setName("lightsBuffer");

            for (int l = 0; l < numLights; l++) {
                auto light = scene->getLight(l);
                if (light->isActive())
                    lights->setElement(l, light->getData());
            }

            pass["gLights"] = lights;
        }

        if (scene->useEnvLight()) {
            auto vars = GraphicsVars::create(pass->getProgram()->getReflector());
            auto envmap = EnvMapLighting::create(context, scene->getEnvMap());
            envmap->setShaderData(vars["gEnvMapLighting"]);
        }
    }
}

RenderPassReflection DeferredMultiresPass::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    addRenderPassInputs(reflector, GBuffers);

    for (const auto& rate : ShadingRates)
        reflector.addInputOutput(rate.name, "Shading color").flags(RenderPassReflection::Field::Flags::Optional);

    reflector.addInput("visibility", "Visibility buffer used for shadowing").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addOutput("viewNormalsOut", "View-space normals");
    reflector.addOutput("extrasOut", "Additional data useful for training");
    return reflector;
}

void DeferredMultiresPass::execute(RenderContext* context, const RenderData& data)
{
    if (scene) {
        auto constants = pass["constants"];
        constants["cameraPosition"] = scene->getCamera()->getPosition();
        constants["world2View"] = scene->getCamera()->getViewMatrix();

        framebuffer->attachColorTarget(data["viewNormalsOut"]->asTexture(), 1);
        framebuffer->attachColorTarget(data["extrasOut"]->asTexture(), 2);

        pass["gPosW"] = data["posW"]->asTexture();
        pass["gNormW"] = data["normW"]->asTexture();;
        pass["gDiffuse"] = data["diffuseOpacity"]->asTexture();;
        pass["gSpecRough"] = data["specRough"]->asTexture();
        pass["gEmissive"] = data["emissive"]->asTexture();
        pass["gVisibility"] = data["visibility"]->asTexture();

        ID3D12GraphicsCommandList5Ptr directX;
        d3d_call(context->getLowLevelData()->getCommandList()->QueryInterface(IID_PPV_ARGS(&directX)));

        for (const auto& rate : ShadingRates) {
            for (int i = 1; i <= 2; i++)
                context->clearRtv(framebuffer->getRenderTargetView(i).get(), float4(0, 0, 0, 1));

            directX->RSSetShadingRate(rate.id, nullptr);
            framebuffer->attachColorTarget(data[rate.name]->asTexture(), 0);
            pass->execute(context, framebuffer);
        }
    }
}
