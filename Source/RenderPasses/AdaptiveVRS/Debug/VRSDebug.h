#pragma once
#include "Falcor.h"

using namespace Falcor;

class VRSDebug : public RenderPass
{
private:
    ComputePass::SharedPtr shader;
    uint2 resolution; uint tileSize;
    VRSDebug();

public:
    using SharedPtr = std::shared_ptr<VRSDebug>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) {return SharedPtr(new VRSDebug);};
    static const char* desc;

    virtual std::string getDesc() override { return desc; }
    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {};
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void renderUI(Gui::Widgets& widget) override {};
};
