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


JaliVRS::JaliVRS()
{
    auto builder = nvinfer1::createInferBuilder(RTLogger);
    builder->setMaxBatchSize(1);

    auto config = builder->createBuilderConfig();
    config->setFlag(nvinfer1::BuilderFlag::kFP16);
    config->setMaxWorkspaceSize(1ULL << 30);

    auto network = builder->createNetwork();
    auto parser = nvonnxparser::createParser(*network, RTLogger);
    if (!parser->parseFromFile("RenderPasses/AdaptiveVRS/Jaliborc/JaliVRS.2onnx", (int)nvinfer1::ILogger::Severity::kINFO))
        std::cerr << "ERROR: could not parse the model.\n";

    model = builder->buildEngineWithConfig(*network, *config)->createExecutionContext();
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
