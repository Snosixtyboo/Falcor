#include <filesystem>
#include <iostream>
#include <NvOnnxParser.h>
#include <NvInfer.h>
#include <cuda_runtime_api.h>
#include <cuda.h>

using namespace nvinfer1;

struct Destroy {
    template<class T>
    void operator()(T* self) const { if (self) self->destroy(); }
};
template<class T>
using RT = std::unique_ptr<T, Destroy>;

class Log : public ILogger {
public:
    void log(Severity severity, const char* msg) override {
        if ((severity == Severity::kERROR) || (severity == Severity::kINTERNAL_ERROR))
            std::cout << "---------------------TENSOR RT---------------------\n" << msg << "\n---------------------------------------------------\n";
    }
} Log;

int size(DataType t)
{
    switch (t) {
    case DataType::kINT32:
    case DataType::kFLOAT: return 8;
    case DataType::kHALF:  return 4;
    case DataType::kINT8:  return 2;
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

void resetCuda()
{
    cuInit(0);
    CUcontext context;
    cuCtxGetCurrent(&context);
    CUdevice device;
    cuCtxGetDevice(&device);
    cuCtxCreate(&context, 0, device);
    cuCtxSetCurrent(context);
}

std::vector<void*> buffers;
RT<IExecutionContext> neural;

int main(int argc, char** argv)
{
    resetCuda(); // because magic

    auto onnxPath = "../../RenderPasses/AdaptiveVRS/Jaliborc/4channel.onnx";
    if (!std::filesystem::exists(onnxPath))
        throw std::runtime_error("ONNX file not found");

    RT<IBuilder> builder{ createInferBuilder(Log) };
    RT<IBuilderConfig> config{ builder->createBuilderConfig() };
    RT<INetworkDefinition> network{ builder->createNetworkV2(1U << static_cast<uint32_t>(NetworkDefinitionCreationFlag::kEXPLICIT_BATCH)) };

    RT<nvonnxparser::IParser> parser{ nvonnxparser::createParser(*network, Log) };
    if (!parser->parseFromFile(onnxPath, static_cast<int>(ILogger::Severity::kINFO)))
        throw std::runtime_error("Could not parse ONNX file");


    config->setMaxWorkspaceSize((1 << 30));
    config->setFlag(BuilderFlag::kFP16);
    config->setFlag(BuilderFlag::kTF32);
    builder->setMaxBatchSize(1);

    RT<ICudaEngine> engine{ builder->buildEngineWithConfig(*network, *config) };
    if (!engine.get())
        throw std::runtime_error("Error creating TensorRT engine");

    if (engine->getNbBindings() != 2)
        throw std::runtime_error("Wrong number of input and output buffers in TensorRT network");

    for (int i = 0; i < engine->getNbBindings(); i++) {
        void** buffer = nullptr;
        auto shape = engine->getBindingDimensions(i);
        auto bits = size(engine->getBindingDataType(i));

        cudaMalloc(buffer, (size_t) bits * length(shape));
        buffers.push_back(buffer);
    }

    neural.reset(engine->createExecutionContext());
    neural->executeV2(buffers.data());
    std::cout << "Done!\n";

    for (auto it = buffers.begin(); it != buffers.end(); it++)
        cudaFree(*it);
}
