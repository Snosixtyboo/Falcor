#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"
#include "RenderGraph/RenderPassHelpers.h"

using namespace Falcor;

class DeferredMultiresPass : public RenderPass
{
public:
    struct ShadingRate { D3D12_SHADING_RATE id; std::string name; };
    using SharedPtr = std::shared_ptr<DeferredMultiresPass>;

    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {});
    virtual std::string getDesc() override { return "Deferred rasterization at multiple shading rates."; }

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {}
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void setScene(RenderContext* context, const Scene::SharedPtr& scene) override;

private:
    FullScreenPass::SharedPtr mpPass;
    Fbo::SharedPtr mpFbo;
    GraphicsVars::SharedPtr mpVars;
    Scene::SharedPtr mpScene;
    ParameterBlock::SharedPtr mpSceneBlock;
    ID3D12GraphicsCommandList5Ptr directX;
    glm::mat4x4 prevVP;

    int numLights;
    Buffer::SharedPtr mpLightsBuffer;
    LightProbe::SharedPtr lightProbe;

    DeferredMultiresPass();
};
