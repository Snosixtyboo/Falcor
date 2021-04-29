#pragma once
#include "Falcor.h"
#include "NvInfer.h"

using namespace Falcor;
using namespace nvinfer1;

class JaliVRS : public RenderPass
{
private:
    class Logger : public ILogger {
    public:
        void log(Severity severity, const char* msg) override {
            if ((severity == Severity::kERROR) || (severity == Severity::kINTERNAL_ERROR))
                throw std::runtime_error(msg);
        }
    } Logger;

    struct Destroy {
        template<class T>
        void operator()(T* self) const { if (self) self->destroy(); }
    };
    template<class T>
    using RT = std::unique_ptr<T, Destroy>;

    RT<IExecutionContext> context;
    RT<ICudaEngine> engine;
    float limit = 0.25f;
    JaliVRS();

public:
    using SharedPtr = std::shared_ptr<JaliVRS>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {}) { return SharedPtr(new JaliVRS); };
    virtual std::string getDesc() override { return "Our deep learning based VRS implementation"; }

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {};
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void renderUI(Gui::Widgets& widget) override;
};
