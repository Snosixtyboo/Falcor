#pragma once
#include "Falcor.h"

using namespace Falcor;

class TemporalReproject : public RenderPass
{
private:
    FullScreenPass::SharedPtr shader;
    Fbo::SharedPtr framebuffer;
    TemporalReproject();

public:
    using SharedPtr = std::shared_ptr<TemporalReproject>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) {return SharedPtr(new TemporalReproject);}
    virtual std::string getDesc() override { return desc; }
    static const char* desc;
 
    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {}
};
