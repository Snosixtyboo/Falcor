#include "JaliVRS.h"
#include "NvOnnxParser.h"

class RTLogger : public nvinfer1::ILogger
{
public:
    void log(Severity severity, const char* msg) override
    {
        if ((severity == Severity::kERROR) || (severity == Severity::kINTERNAL_ERROR))
                std::cerr << msg << "\n";
    }
} RTLogger;

struct RTPointer
{
    template< class T >
    void operator()(T* obj) const
    {
        if (obj)
            obj->destroy();
    }
};

template<class T>
using RT = std::unique_ptr<T, RTPointer>;

JaliVRS::JaliVRS()
{
    RT<nvinfer1::IBuilder> builder {nvinfer1::createInferBuilder(RTLogger)};
    builder->setMaxBatchSize(1);

    RT<nvinfer1::IBuilderConfig> config {builder->createBuilderConfig()};
    config->setFlag(nvinfer1::BuilderFlag::kFP16);
    config->setMaxWorkspaceSize(1ULL << 30);

    RT<nvinfer1::INetworkDefinition> network {builder->createNetwork()};
    RT<nvonnxparser::IParser> parser {nvonnxparser::createParser(*network, RTLogger)};

    if (!parser->parseFromFile("RenderPasses/AdaptiveVRS/Jaliborc/JaliVRS.onnx", (int)nvinfer1::ILogger::Severity::kINFO))
        std::cerr << "ERROR: could not parse the model.\n";

    RT<nvinfer1::ICudaEngine> engine {builder->buildEngineWithConfig(*network, *config)};
    RT<nvinfer1::IExecutionContext> model {engine->createExecutionContext()};

    std::cout << model->getName();
}

RenderPassReflection JaliVRS::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    reflector.addInput("fake", "Fake");
    return reflector;
}

void JaliVRS::execute(RenderContext* context, const RenderData& data)
{
}

void JaliVRS::renderUI(Gui::Widgets& widget)
{
    widget.slider("Max Perceptual Error", limit, 0.0f, 1.0f);
    //shader->addDefine("LIMIT", std::to_string(limit));
}
