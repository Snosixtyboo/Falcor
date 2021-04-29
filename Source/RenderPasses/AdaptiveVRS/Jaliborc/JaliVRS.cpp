#include "JaliVRS.h"
#include "NvOnnxParser.h"

JaliVRS::JaliVRS()
{
    RT<IBuilder> builder {nvinfer1::createInferBuilder(Logger)};
    builder->setMaxBatchSize(1);
    builder->setFp16Mode(true);

    RT<INetworkDefinition> network {builder->createNetwork()};
    RT<nvonnxparser::IParser> parser {nvonnxparser::createParser(*network, Logger)};

    auto file = fopen("mnist-1.onnx", "r");
    if (file == NULL)
        throw std::runtime_error("Could not open onnx file");
    else
        fclose(file);

    if (!parser->parseFromFile("mnist-1.onnx", (int)Logger::Severity::kVERBOSE))
        throw std::runtime_error("Could not parse onnx model");

    std::cout << network->getNbLayers();

    engine = RT<ICudaEngine>(builder->buildCudaEngine(*network));
    if (!engine.get())
        throw std::runtime_error("Error creating engine");

    RT<IExecutionContext> context {engine->createExecutionContext()};
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
