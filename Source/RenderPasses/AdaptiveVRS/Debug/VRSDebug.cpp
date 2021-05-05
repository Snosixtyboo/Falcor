#include "VRSDebug.h"

VRSDebug::VRSDebug()
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
    defines.add("VRS_4x4", std::to_string(D3D12_SHADING_RATE_4X4));

    shader = ComputePass::create("RenderPasses/AdaptiveVRS/Debug/VRSDebug.slang", "main", defines);
}

RenderPassReflection VRSDebug::reflect(const CompileData& data)
{
    auto fbo = gpFramework->getTargetFbo();
    resolution = uint2(fbo->getWidth(), fbo->getHeight());
    shader["constant"]["resolution"] = resolution;

    RenderPassReflection reflector;
    reflector.addInput("rendering", "Rendering").texture2D(resolution.x, resolution.y);
    reflector.addInput("rate", "Rate").format(ResourceFormat::R8Uint).texture2D(resolution.x / tileSize, resolution.y / tileSize);
    reflector.addOutput("color", "Color").bindFlags(ResourceBindFlags::UnorderedAccess).format(ResourceFormat::RGBA32Float);
    return reflector;
}

void VRSDebug::execute(RenderContext* context, const RenderData& data)
{
    shader["rate"] = data["rate"]->asTexture();
    shader["color"] = data["color"]->asTexture();
    shader["rendering"] = data["rendering"]->asTexture();
    shader->execute(context, resolution.x, resolution.y);
}
