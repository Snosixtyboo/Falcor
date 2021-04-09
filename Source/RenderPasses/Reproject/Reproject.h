#pragma once
#include "Falcor.h"

using namespace Falcor;

class Reproject : public RenderPass
{
private:
    FullScreenPass::SharedPtr shader;
    Fbo::SharedPtr framebuffers;
    Reproject();

public:
    using SharedPtr = std::shared_ptr<Reproject>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) {return SharedPtr(new Reproject);};
    virtual std::string getDesc() override { return "Reprojects previous render using screen-space motion."; }

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {}
};
