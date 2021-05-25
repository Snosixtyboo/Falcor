#include "JaliVRS.h"
#include <NvOnnxParser.h>
#include <filesystem>
#include <cuda.h>

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

JaliVRS::JaliVRS()
{
    resetCuda(); // because magic
    copyOut = ComputePass::create("RenderPasses/AdaptiveVRS/Jaliborc/CopyOut.slang", "main");
    copyIn = ComputePass::create("RenderPasses/AdaptiveVRS/Jaliborc/CopyIn.slang", "main");

    auto onnxPath = "../RenderPasses/AdaptiveVRS/Jaliborc/4channel.onnx";
    if (!std::filesystem::exists(onnxPath))
        throw std::runtime_error("ONNX file not found");

    RT<IBuilder> builder {createInferBuilder(Log)};
    RT<IBuilderConfig> config {builder->createBuilderConfig()};
    RT<INetworkDefinition> network {builder->createNetworkV2(1U << static_cast<uint32_t>(NetworkDefinitionCreationFlag::kEXPLICIT_BATCH))};

    RT<nvonnxparser::IParser> parser {nvonnxparser::createParser(*network, Log)};
    if (!parser->parseFromFile(onnxPath, static_cast<int>(ILogger::Severity::kINFO)))
        throw std::runtime_error("Could not parse ONNX file");

    /*auto profile = builder->createOptimizationProfile();
    auto inputName = network->getInput(0)->getName();
    profile->setDimensions(inputName, OptProfileSelector::kMIN, Dims4{ 1, 4, 2160, 3840 });
    profile->setDimensions(inputName, OptProfileSelector::kOPT, Dims4{ 1, 4, 2160, 3840 });
    profile->setDimensions(inputName, OptProfileSelector::kMAX, Dims4{ 1, 4, 2160, 3840 });
    config->addOptimizationProfile(profile);*/

    config->setMaxWorkspaceSize((1 << 30));
    config->setFlag(BuilderFlag::kFP16);
    config->setFlag(BuilderFlag::kTF32);
    builder->setMaxBatchSize(1);

    RT<ICudaEngine> engine {builder->buildEngineWithConfig(*network, *config)};
    if (!engine.get())
        throw std::runtime_error("Error creating TensorRT engine");

    if (engine->getNbBindings() != 2)
        throw std::runtime_error("Wrong number of input and output buffers in TensorRT network");

    for (int i = 0; i < engine->getNbBindings(); i++) {
        std::string name = engine->getBindingName(i);

        auto shape = engine->getBindingDimensions(i);
        auto bits = size(engine->getBindingDataType(i));
        auto buffer = Buffer::createStructured(bits, length(shape), Resource::BindFlags::Shared | Resource::BindFlags::UnorderedAccess, Buffer::CpuAccess::None, nullptr, false);
        buffer->setName(name);

        if (name.compare("input") == 0) {
            copyIn["constant"]["resolution"] = resIn = int2(shape.d[3], shape.d[2]);
            copyIn["input"] = buffer;
        }
        else if (name.compare("metric") == 0) {
            copyOut["constant"]["resolution"] = resOut = int2(shape.d[3], shape.d[2]);
            copyOut["metric"] = buffer;
        }
        else {
            throw std::runtime_error("Unexpected name for buffer in TensorRT network");
        }

        buffers.push_back(buffer->getCUDADeviceAddress());
    }

    neural.reset(engine->createExecutionContext());
    //neural->setBindingDimensions(0, { 1, 4, 2160, 3840 });
    //neural->setOptimizationProfile(0);
}

RenderPassReflection JaliVRS::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    reflector.addOutput("rate", "Rate").bindFlags(ResourceBindFlags::UnorderedAccess).format(ResourceFormat::R8Uint).texture2D(resOut.x, resOut.y);
    reflector.addInput("reproject", "Reproject");
    reflector.addInput("diffuse", "Diffuse");
    reflector.addInput("normals", "Normals");
    return reflector;
}

void JaliVRS::execute(RenderContext* context, const RenderData& data)
{
    copyIn["reproject"] = data["reproject"]->asTexture();
    copyIn["diffuse"] = data["diffuse"]->asTexture();
    copyIn["normals"] = data["normals"]->asTexture();
    copyIn->execute(context, resIn.x, resIn.y);

    std::cout << neural->allInputDimensionsSpecified() << "\n";
    std::cout << neural->getOptimizationProfile() << "\n";
    neural->executeV2(buffers.data());

    copyOut["rate"] = data["rate"]->asTexture();
    copyOut->execute(context, resOut.x, resOut.y);
}

void JaliVRS::renderUI(Gui::Widgets& widget)
{
    widget.slider("Max Perceptual Error", limit, 0.0f, 1.0f);
    //shader->addDefine("LIMIT", std::to_string(limit));
}
