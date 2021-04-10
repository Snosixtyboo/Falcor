#include "AdaptiveVRS.h"

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("AdaptiveVRS", "Sets shading rate based on rendering result.", AdaptiveVRS::create);
}

AdaptiveVRS::AdaptiveVRS()
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS6 hardware;
    gpDevice->getApiHandle()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &hardware, sizeof(hardware));
    tileSize = hardware.ShadingRateImageTileSize;

    Shader::DefineList defines;
    defines.add("VRS_1x1", std::to_string(D3D12_SHADING_RATE_1X1));
    defines.add("VRS_1x2", std::to_string(D3D12_SHADING_RATE_1X2));
    defines.add("VRS_2x1", std::to_string(D3D12_SHADING_RATE_2X1));
    defines.add("VRS_2x2", std::to_string(D3D12_SHADING_RATE_2X2));
    defines.add("VRS_4x4", std::to_string(D3D12_SHADING_RATE_4X4));
    defines.add("VRS_TILE", std::to_string(tileSize));
    defines.add("LIMIT", std::to_string(limit));

    shader = ComputePass::create("RenderPasses/AdaptiveVRS/AdaptiveVRS.slang", "main", defines);
}

RenderPassReflection AdaptiveVRS::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    reflector.addInput("input", "Input").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addOutput("rate", "Rate").bindFlags(ResourceBindFlags::UnorderedAccess).format(ResourceFormat::R8Uint).texture2D(240, 135);
    return reflector;
}

void AdaptiveVRS::compile(RenderContext* context, const CompileData& data)
{
    resolution = data.defaultTexDims / uint2(tileSize, tileSize);
    shader["constant"]["resolution"] = resolution;
}

void AdaptiveVRS::execute(RenderContext* context, const RenderData& data)
{
    shader["rate"] = data["rate"]->asTexture();
    shader["input"] = data["input"]->asTexture();
    shader->execute(context, resolution.x, resolution.y);
}

void AdaptiveVRS::renderUI(Gui::Widgets& widget)
{
    widget.slider("Perceptual Threshold", limit, 0.0f, 1.0f);
    shader->addDefine("LIMIT", std::to_string(limit));
}
