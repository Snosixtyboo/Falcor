#pragma once
#include "Falcor.h"

using namespace Falcor;

class YangVRS : public RenderPass
{
private:
    ComputePass::SharedPtr shader;
    uint2 resolution; uint tileSize;
    float limit = 0.25f, luminance = 0.0f;
    YangVRS();

public:
    using SharedPtr = std::shared_ptr<YangVRS>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) {return SharedPtr(new YangVRS);};
    static const char* desc;

    virtual std::string getDesc() override { return desc; }
    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {};
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void renderUI(Gui::Widgets& widget) override;
};
