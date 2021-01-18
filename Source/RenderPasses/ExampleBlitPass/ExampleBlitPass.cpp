/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "ExampleBlitPass.h"
#include <filesystem>
#include <cstdlib>

const ChannelList ExampleBlitPass::kGBufferChannels =
{
    { "depth",          "gDepth",           "depth buffer",                 false               , ResourceFormat::D32Float},
    { "posW",           "gPosW",            "world space position",         true /* optional */, ResourceFormat::RGBA32Float },
    { "normW",          "gNormW",           "world space normal",           true /* optional */, ResourceFormat::RGBA32Float },
    { "tangentW",       "gTangentW",        "world space tangent",          true /* optional */, ResourceFormat::RGBA32Float },
    { "texC",           "gTexC",            "texture coordinates",          true /* optional */, ResourceFormat::RGBA32Float },
    { "diffuseOpacity", "gDiffuseOpacity",  "diffuse color",                true /* optional */, ResourceFormat::RGBA32Float },
    { "specRough",      "gSpecRough",       "specular color",               true /* optional */, ResourceFormat::RGBA32Float },
    { "emissive",       "gEmissive",        "emissive color",               true /* optional */, ResourceFormat::RGBA32Float },
    { "matlExtra",      "gMatlExtra",       "additional material data",     true /* optional */, ResourceFormat::RGBA32Float },
};

const std::string kDepthName = "depth";

const std::string kShaderFilename = "RenderPasses/ExampleBlitPass/CopyPass.slang";

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("ExampleBlitPass", "Blits a texture into another texture", ExampleBlitPass::create);
}

ExampleBlitPass::ExampleBlitPass()
{
    mpPass = FullScreenPass::create(kShaderFilename);
    mpFbo = Fbo::create();
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPass["gSampler"] = Sampler::create(samplerDesc);

    mpVars = GraphicsVars::create(mpPass->getProgram()->getReflector());
    ParameterBlockReflection::SharedConstPtr pReflection = mpPass->getProgram()->getReflector()->getParameterBlock("tScene");
    mpSceneBlock = ParameterBlock::create(pReflection);
}


void ExampleBlitPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    if (pScene)
    {
        mpScene = pScene;

        lastCaptureTime = 0;
        framesCaptured = 0;

        numLights = pScene->getLightCount();
        if (numLights)
        {
            mpLightsBuffer = Buffer::createStructured(sizeof(LightData), (uint32_t)numLights, Resource::BindFlags::ShaderResource, Buffer::CpuAccess::None, nullptr, false);
            mpLightsBuffer->setName("mpLightsBuffer");

            for (int l = 0; l < numLights; l++)
            {
                auto light = mpScene->getLight(l);
                if (!light->isActive()) continue;
                mpLightsBuffer->setElement(l, light->getData());
            }

            lightProbe = pScene->getLightProbe();
            if (lightProbe)
            {
                lightProbe->setShaderData(mpSceneBlock["lightProbe"]);
                mpVars->setParameterBlock("tScene", mpSceneBlock);
                mpPass["tScene"] = mpSceneBlock;
            }
        }
    }
}

ExampleBlitPass::SharedPtr ExampleBlitPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new ExampleBlitPass);
    return pPass;
}

Dictionary ExampleBlitPass::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection ExampleBlitPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;

    addRenderPassInputs(reflector, kGBufferChannels);
    reflector.addOutput("output", "the target texture").format(ResourceFormat::RGBA32Float).texture2D(0,0, 1);
    reflector.addOutput("normalsOut", "World-space normal, [0,1] range.").format(ResourceFormat::RGBA8Unorm).texture2D(0, 0, 1);
    reflector.addOutput("viewDirsOut", "View directions").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("viewNormalsOut", "View-space normals").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("roughOpacOut", "Roughness and opacity in dedicated texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    return reflector;
}

void ExampleBlitPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // renderData holds the requested resources
    const auto& output = renderData["output"]->asTexture();
    const auto& normalsOut = renderData["normalsOut"]->asTexture();
    const auto& viewDirsOut = renderData["viewDirsOut"]->asTexture();
    const auto& viewNormalsOut = renderData["viewNormalsOut"]->asTexture();
    const auto& roughOpacOut = renderData["roughOpacOut"]->asTexture();

    mpFbo->attachColorTarget(output, 0);
    mpFbo->attachColorTarget(normalsOut, 1);
    mpFbo->attachColorTarget(viewDirsOut, 2);
    mpFbo->attachColorTarget(viewNormalsOut, 3);
    mpFbo->attachColorTarget(roughOpacOut, 4);

    if (mpScene)
    {
        // Cast to required command list
        d3d_call(pRenderContext->getLowLevelData()->getCommandList()->QueryInterface(IID_PPV_ARGS(&commandList5)));
        // Set Shading Rate to 4x4 using ID3D12GraphicsCommandList5.
        commandList5->RSSetShadingRate(activeRate, nullptr);

        const auto& posW = renderData["posW"]->asTexture();
        const auto& normW = renderData["normW"]->asTexture();
        const auto& diffuseOpacity = renderData["diffuseOpacity"]->asTexture();
        const auto& specRough = renderData["specRough"]->asTexture();
        const auto& emissive = renderData["emissive"]->asTexture();

        mpPass["gPosW"] = posW;
        mpPass["gNormW"] = normW;
        mpPass["gDiffuse"] = diffuseOpacity;
        mpPass["gSpecRough"] = specRough;
        mpPass["gEmissive"] = emissive;

        auto pCB = mpPass["PerFrameCB"];
        pCB["lightCount"] = numLights;
        mpPass["lights"] = mpLightsBuffer;

        pCB["cameraPosition"] = mpScene->getCamera()->getPosition();
        pCB["world2View"] = mpScene->getCamera()->getViewMatrix();

        mpPass->execute(pRenderContext, mpFbo);

        if (capturing) {
            float antiBias = 0.9f + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(0.2f)));

            if (clock() - lastCaptureTime > (captureInterval * antiBias))
            {
                dumpFormat = Falcor::Bitmap::FileFormat::PfmFile;
                std::string fileEnding;

                if (dumpFormat == Falcor::Bitmap::FileFormat::PngFile)
                    fileEnding = ".png";
                else if (dumpFormat == Falcor::Bitmap::FileFormat::JpegFile)
                    fileEnding = ".jpg";
                else if (dumpFormat == Falcor::Bitmap::FileFormat::TgaFile)
                    fileEnding = ".tga";
                else if (dumpFormat == Falcor::Bitmap::FileFormat::BmpFile)
                    fileEnding = ".bmp";
                else if (dumpFormat == Falcor::Bitmap::FileFormat::PfmFile)
                    fileEnding = ".pfm";
                else if (dumpFormat == Falcor::Bitmap::FileFormat::ExrFile)
                    fileEnding = ".exr";

                std::filesystem::path targetPath(targetDir);
                std::stringstream ss;
                ss << std::setw(4) << std::setfill('0') << framesCaptured;
                std::string capturedStr = ss.str();
                targetPath /= capturedStr;

                if (!std::filesystem::create_directory(targetPath))
                {
                    logError("Failed to create directory " + targetPath.string() + " for frame dumps!", Logger::MsgBox::RetryAbort);
                    return;
                }

                gpFramework->getGlobalClock().pause();
                gpFramework->pauseRenderer(true);

                const D3D12_SHADING_RATE dumpRates[3] = { D3D12_SHADING_RATE_1X1, D3D12_SHADING_RATE_2X2, D3D12_SHADING_RATE_4X4 };
                const std::string dumpRateNames[3] = { "1x1", "2x2", "4x4" };

                std::filesystem::path diffuseOpacityFile = targetPath / ("diffuse-opacity_" + dumpRateNames[0] + fileEnding);
                std::filesystem::path specRoughFile = targetPath / ("specular-roughness_" + dumpRateNames[0] + fileEnding);
                std::filesystem::path emissiveFile = targetPath / ("emissive_" + dumpRateNames[0] + fileEnding);
                std::filesystem::path viewFile = targetPath / ("view_" + dumpRateNames[0] + fileEnding);
                std::filesystem::path normalFile = targetPath / ("normal_" + dumpRateNames[0] + fileEnding);
                std::filesystem::path roughOpacFile = targetPath / ("roughness-opacity_" + dumpRateNames[0] + fileEnding);

                diffuseOpacity->captureToFile(0, 0, diffuseOpacityFile.string(), dumpFormat);
                specRough->captureToFile(0, 0, specRoughFile.string(), dumpFormat);
                emissive->captureToFile(0, 0, emissiveFile.string(), dumpFormat);
                viewDirsOut->captureToFile(0, 0, viewFile.string(), dumpFormat);
                viewNormalsOut->captureToFile(0, 0, normalFile.string(), dumpFormat);
                roughOpacOut->captureToFile(0, 0, roughOpacFile.string(), dumpFormat);

                for (int i = 0; i < sizeof(dumpRates)/sizeof(dumpRates[0]); i++)
                {
                    commandList5->RSSetShadingRate(dumpRates[i], nullptr);
                    mpPass->execute(pRenderContext, mpFbo);
                    std::filesystem::path outputFile = targetPath / ("output_" + dumpRateNames[i] + fileEnding);
                    output->captureToFile(0, 0, outputFile.string(), dumpFormat);
                }

                gpFramework->pauseRenderer(false);
                gpFramework->getGlobalClock().play();

                lastCaptureTime = clock();
                framesCaptured++;
            }
        }

        commandList5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
    }
}

void ExampleBlitPass::renderUI(Gui::Widgets& widget)
{
    // Capturing control
    widget.textbox("Directory for captured images", targetDir);
    widget.var("Capture interval (ms)", captureInterval);
    capturing = capturing ? !widget.button("Stop capturing") : widget.button("Start capturing");
}
