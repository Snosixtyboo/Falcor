#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"
#include "RenderGraph/RenderPassHelpers.h"

using namespace Falcor;

class DeferredPass : public RenderPass
{
public:
    struct ShadingRate { D3D12_SHADING_RATE id; std::string name; };
    using SharedPtr = std::shared_ptr<DeferredPass>;

    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) {return SharedPtr(new DeferredPass);};
    virtual std::string getDesc() override { return "Deferred rasterization at given shading rate."; }

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {};
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void setScene(RenderContext* context, const Scene::SharedPtr& scene) override;

private:
    Fbo::SharedPtr framebuffer = Fbo::create();
    FullScreenPass::SharedPtr pass;
    Scene::SharedPtr scene;

    DeferredPass() {};
};
