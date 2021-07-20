#pragma once
#include "Falcor.h"
#include "NvInfer.h"

using namespace Falcor;
using namespace nvinfer1;

class JaliVRS : public RenderPass
{
private:
    struct Destroy {
        template<class T>
        void operator()(T* self) const { if (self) self->destroy(); }
    };
    template<class T>
    using RT = std::unique_ptr<T, Destroy>;

    uint2 resIn, resOut;
    ComputePass::SharedPtr copyIn, copyOut;
    RT<ICudaEngine> engine;
    RT<IExecutionContext> neural;
    std::vector<void*> buffers;
    float limit = 0.25f;
    JaliVRS();

public:
    using SharedPtr = std::shared_ptr<JaliVRS>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) { return SharedPtr(new JaliVRS); };
    static const char* desc;

    virtual std::string getDesc() override { return desc; }
    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {};
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void renderUI(Gui::Widgets& widget) override;
};
