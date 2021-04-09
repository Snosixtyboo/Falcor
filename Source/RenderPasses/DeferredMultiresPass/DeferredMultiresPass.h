#pragma once
#include "Falcor.h"
#include "RenderGraph/RenderPassHelpers.h"

using namespace Falcor;

class DeferredMultiresPass : public RenderPass
{
public:
    struct ShadingRate { D3D12_SHADING_RATE id; std::string name; };
    using SharedPtr = std::shared_ptr<DeferredMultiresPass>;

    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) {return SharedPtr(new DeferredMultiresPass);};
    virtual std::string getDesc() override { return "Deferred rasterization at multiple shading rates."; }

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {}
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void setScene(RenderContext* context, const Scene::SharedPtr& scene) override;

private:
    FullScreenPass::SharedPtr pass;
    Fbo::SharedPtr framebuffers;
    GraphicsVars::SharedPtr vars;
    Scene::SharedPtr scene;
    ParameterBlock::SharedPtr sceneBlock;
    ID3D12GraphicsCommandList5Ptr directX;

    int numLights;
    Buffer::SharedPtr lightsBuffer;
    LightProbe::SharedPtr lightProbe;

    DeferredMultiresPass();
};
