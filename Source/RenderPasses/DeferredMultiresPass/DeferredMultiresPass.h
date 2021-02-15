#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"
#include "RenderGraph/RenderPassHelpers.h"

using namespace Falcor;

class DeferredMultiresPass : public RenderPass
{
private:
    FullScreenPass::SharedPtr mpPass;
    Fbo::SharedPtr mpFbo;
    GraphicsVars::SharedPtr mpVars;
    Scene::SharedPtr mpScene;
    ParameterBlock::SharedPtr mpSceneBlock;
    ID3D12GraphicsCommandList5Ptr directX;
    glm::mat4x4 prevVP; // TODO

    int numLights;
    Buffer::SharedPtr mpLightsBuffer;
    LightProbe::SharedPtr lightProbe;

    DeferredMultiresPass();

public:
    struct ShadingRate { D3D12_SHADING_RATE id; std::string name; };
    using SharedPtr = std::shared_ptr<DeferredMultiresPass>;

    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});
    virtual std::string getDesc() override { return "Deferred rasterization at multiple shading rates."; }
    virtual Dictionary getScriptingDictionary() override;

    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pContext, const CompileData& compileData) override {}
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
};
