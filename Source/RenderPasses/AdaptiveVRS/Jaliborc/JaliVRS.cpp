#include "JaliVRS.h"
#include "NvInfer.h"
#include "NvOnnxParser.h"

class RTLogger : public nvinfer1::ILogger
{
public:
    void log(Severity severity, const char* msg) override
    {
        if ((severity == Severity::kERROR) || (severity == Severity::kINTERNAL_ERROR))
                std::cout << msg << "n";
    }
} RTLogger;


JaliVRS::JaliVRS()
{
    auto builder = nvinfer1::createInferBuilder(RTLogger);
    auto network = builder->createNetwork();
    auto parser = nvonnxparser::createParser(*network, RTLogger);

    if (!parser->parseFromFile("RenderPasses/AdaptiveVRS/Jaliborc/JaliVRS.onnx", (int)nvinfer1::ILogger::Severity::kINFO)) {
        std::cerr << "ERROR: could not parse the model.\n";
    }

}

RenderPassReflection JaliVRS::reflect(const CompileData& data)
{
    return RenderPassReflection();
}

void JaliVRS::execute(RenderContext* context, const RenderData& data)
{
}

void JaliVRS::renderUI(Gui::Widgets& widget)
{
    widget.slider("Max Perceptual Error", limit, 0.0f, 1.0f);
    shader->addDefine("LIMIT", std::to_string(limit));
}
