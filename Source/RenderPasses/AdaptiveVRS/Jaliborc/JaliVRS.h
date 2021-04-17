#pragma once
#include "Falcor.h"

using namespace Falcor;

class JaliVRS : public RenderPass
{
private:
    ComputePass::SharedPtr shader;
    uint2 resolution; uint tileSize;
    float limit = 0.25f;
    JaliVRS();

public:
    using SharedPtr = std::shared_ptr<JaliVRS>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) {return SharedPtr(new JaliVRS);};
    virtual std::string getDesc() override { return "Our deep learning based VRS implementation"; }

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {};
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void renderUI(Gui::Widgets& widget) override;
};
