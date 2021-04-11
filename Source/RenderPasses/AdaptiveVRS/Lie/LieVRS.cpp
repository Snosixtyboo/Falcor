#include "LieVRS.h"

LieVRS::LieVRS()
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS6 hardware;
    gpDevice->getApiHandle()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &hardware, sizeof(hardware));
    tileSize = hardware.ShadingRateImageTileSize;

    Shader::DefineList defines;
    defines.add("VRS_TILE", std::to_string(tileSize));
    defines.add("VRS_1x1", std::to_string(D3D12_SHADING_RATE_1X1));
    defines.add("VRS_1x2", std::to_string(D3D12_SHADING_RATE_1X2));
    defines.add("VRS_2x1", std::to_string(D3D12_SHADING_RATE_2X1));
    defines.add("VRS_2x2", std::to_string(D3D12_SHADING_RATE_2X2));
    defines.add("VRS_2x4", std::to_string(D3D12_SHADING_RATE_2X4));
    defines.add("VRS_4x2", std::to_string(D3D12_SHADING_RATE_4X2));
    defines.add("LUMINANCE", std::to_string(luminance));
    defines.add("LIMIT", std::to_string(limit));

    shader = ComputePass::create("RenderPasses/AdaptiveVRS/Lie/LieVRS.slang", "main", defines);
}

RenderPassReflection LieVRS::reflect(const CompileData& data)
{
    auto fbo = gpFramework->getTargetFbo();
    resolution = uint2(fbo->getWidth(), fbo->getHeight()) / uint2(tileSize, tileSize);
    shader["constant"]["resolution"] = resolution;

    RenderPassReflection reflector;
    reflector.addInput("input", "Input").bindFlags(ResourceBindFlags::ShaderResource);
    reflector.addOutput("rate", "Rate").bindFlags(ResourceBindFlags::UnorderedAccess).format(ResourceFormat::R8Uint).texture2D(resolution.x, resolution.y);
    return reflector;
}

void LieVRS::execute(RenderContext* context, const RenderData& data)
{
    shader["rate"] = data["rate"]->asTexture();
    shader["input"] = data["input"]->asTexture();
    shader->execute(context, resolution.x, resolution.y);
}

void LieVRS::renderUI(Gui::Widgets& widget)
{
    widget.slider("Perceptual Threshold", limit, 0.0f, 0.002f);
    shader->addDefine("LIMIT", std::to_string(limit));

    widget.slider("Environment Luminance", luminance, 0.0f, 0.01f);
    shader->addDefine("LUMINANCE", std::to_string(luminance));
}
