//#include <cuda_runtime_api.h>
#include "JaliVRS.h"
#include "NvOnnxParser.h"

class Log : public ILogger {
public:
    void log(Severity severity, const char* msg) override {
        if ((severity == Severity::kERROR) || (severity == Severity::kINTERNAL_ERROR))
            throw std::runtime_error(msg);
    }
} Log;

int size(DataType t)
{
    switch (t) {
        case DataType::kINT32:
        case DataType::kFLOAT: return 32;
        case DataType::kHALF:  return 16;
        case DataType::kINT8:  return 8;
    }
    return 0;
}

int length(Dims shape)
{
    int length = 1;
    for (int i = 0; i < shape.nbDims; i++)
        length *= shape.d[i];

    return length;
}

JaliVRS::JaliVRS()
{
    auto onnxPath = "JaliVRS.onnx";

    RT<IBuilder> builder {createInferBuilder(Log)};
    RT<IBuilderConfig> config {builder->createBuilderConfig()};
    RT<INetworkDefinition> network {builder->createNetworkV2(1U << static_cast<uint32_t>(NetworkDefinitionCreationFlag::kEXPLICIT_BATCH))};

    RT<nvonnxparser::IParser> parser {nvonnxparser::createParser(*network, Log)};
    if (!parser->parseFromFile(onnxPath, static_cast<int>(ILogger::Severity::kINFO)))
        throw std::runtime_error("Could not parse ONNX file");

    config->setMaxWorkspaceSize((1 << 30));
    builder->setMaxBatchSize(1);

    RT<ICudaEngine> engine {builder->buildEngineWithConfig(*network, *config)};
    if (!engine.get())
        throw std::runtime_error("Error creating TensorRT engine");

    if (engine->getNbBindings() != 2 || engine->bindingIsInput(0) == engine->bindingIsInput(1))
        throw std::runtime_error("Wrong number of input and output buffers in TensorRT network");

    for (int i = 0; i < engine->getNbBindings(); i++) {
        int bits = size(engine->getBindingDataType(i));
        int n = length(engine->getBindingDimensions(i));
        auto buffer = Buffer::createStructured(bits, n, Resource::BindFlags::Shared, Buffer::CpuAccess::None, nullptr, false);

        if (engine->bindingIsInput(i))
            gdata = buffer->getCUDADeviceAddress();
        else
            metric = buffer->getCUDADeviceAddress();
    }

    context.reset(engine->createExecutionContext());
    if (!context.get())
        throw std::runtime_error("Error creating TensorRT context");
}

RenderPassReflection JaliVRS::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    reflector.addInput("fake", "Fake");
    return reflector;
}

void JaliVRS::execute(RenderContext* rcontext, const RenderData& data)
{
    /*void* buffers[] = { gdata, metric };
    context->executeV2(buffers);*/
}

void JaliVRS::renderUI(Gui::Widgets& widget)
{
    widget.slider("Max Perceptual Error", limit, 0.0f, 1.0f);
    //shader->addDefine("LIMIT", std::to_string(limit));
}
