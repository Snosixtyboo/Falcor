#include "JaliVRS.h"
#include "NvOnnxParser.h"

JaliVRS::JaliVRS()
{
    ////////// debug sanity check for now
    auto onnxPath = "JaliVRS.onnx";
    auto file = fopen(onnxPath, "r");
    if (file == NULL)
        throw std::runtime_error("Could not open ONNX file");
    else
        fclose(file);
    //////////

    RT<IBuilder> builder {createInferBuilder(Logger)};
    RT<IBuilderConfig> config {builder->createBuilderConfig()};
    RT<INetworkDefinition> network {builder->createNetworkV2(1U << static_cast<uint32_t>(NetworkDefinitionCreationFlag::kEXPLICIT_BATCH))};

    RT<nvonnxparser::IParser> parser {nvonnxparser::createParser(*network, Logger)};
    if (!parser->parseFromFile(onnxPath, static_cast<int>(ILogger::Severity::kINFO)))
        throw std::runtime_error("Could not parse ONNX network");

    config->setMaxWorkspaceSize((1 << 30));
    builder->setMaxBatchSize(1);

   /* auto profile = builder->createOptimizationProfile();
    profile->setDimensions(network->getInput(0)->getName(), OptProfileSelector::kMIN, Dims4{ 1, 3, 256 , 256 });
    profile->setDimensions(network->getInput(0)->getName(), OptProfileSelector::kOPT, Dims4{ 1, 3, 256 , 256 });
    profile->setDimensions(network->getInput(0)->getName(), OptProfileSelector::kMAX, Dims4{ 32, 3, 256 , 256 });
    config->addOptimizationProfile(profile);*/

    engine.reset(builder->buildEngineWithConfig(*network, *config));
    if (!engine.get())
        throw std::runtime_error("Error creating TensorRT engine");

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

void JaliVRS::execute(RenderContext* context, const RenderData& data)
{
}

void JaliVRS::renderUI(Gui::Widgets& widget)
{
    widget.slider("Max Perceptual Error", limit, 0.0f, 1.0f);
    //shader->addDefine("LIMIT", std::to_string(limit));
}
