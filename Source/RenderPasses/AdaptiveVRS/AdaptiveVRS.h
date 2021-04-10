#pragma once
#include "Falcor.h"

using namespace Falcor;

class AdaptiveVRS : public RenderPass
{
private:
    ComputePass::SharedPtr shader;
    uint2 resolution; uint tileSize;
    float limit = 0.5;
    AdaptiveVRS();

public:
    using SharedPtr = std::shared_ptr<AdaptiveVRS>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) {return SharedPtr(new AdaptiveVRS);};
    virtual std::string getDesc() override { return "Sets shading rate based on rendering result."; }

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override;
    virtual void renderUI(Gui::Widgets& widget) override;
};
