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

const ChannelList ExampleBlitPass::kGBufferChannels =
{
    { "depth",          "gDepth",           "depth buffer",                 false               , ResourceFormat::D32Float},
    { "posW",           "gPosW",            "world space position",         true /* optional */, ResourceFormat::RGBA32Float },
    { "normW",          "gNormW",           "world space normal",           true /* optional */, ResourceFormat::RGBA32Float },
    { "tangentW",       "gTangentW",        "world space tangent",          true /* optional */, ResourceFormat::RGBA32Float },
    { "texC",           "gTexC",            "texture coordinates",          true /* optional */, ResourceFormat::RGBA32Float },
    { "diffuseOpacity", "gDiffuseOpacity",  "diffuse color and opacity",    true /* optional */, ResourceFormat::RGBA32Float },
    { "specRough",      "gSpecRough",       "specular color and roughness", true /* optional */, ResourceFormat::RGBA32Float },
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
    reflector.addOutput("pictureOutput", "the target texture for PNGs");
    reflector.addOutput("normalsOut", "World-space normal, [0,1] range.").format(ResourceFormat::RGBA8Unorm).texture2D(0, 0, 1);
    return reflector;
}

static int captures = 0;
static int lastTime = 0;

void ExampleBlitPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // renderData holds the requested resources
    const auto& output = renderData["output"]->asTexture();
    const auto& normalsOut = renderData["normalsOut"]->asTexture();
    const auto& pictureOutput = renderData["pictureOutput"]->asTexture();
    mpFbo->attachColorTarget(output, 0);
    mpFbo->attachColorTarget(normalsOut, 1);
    mpFbo->attachColorTarget(pictureOutput, 2);

    if (mpScene)
    {
        // Cast to required command list
        d3d_call(pRenderContext->getLowLevelData()->getCommandList()->QueryInterface(IID_PPV_ARGS(&commandList5)));
        // Set Shading Rate to 4x4 using ID3D12GraphicsCommandList5.
        commandList5->RSSetShadingRate(activeRate, nullptr);

        const auto& posW = renderData["posW"]->asTexture();
        const auto& normW = renderData["normW"]->asTexture();
        const auto& diffuse = renderData["diffuseOpacity"]->asTexture();
        const auto& specRough = renderData["specRough"]->asTexture();
        const auto& emissive = renderData["emissive"]->asTexture();

        mpPass["gPosW"] = posW;
        mpPass["gNormW"] = normW;
        mpPass["gDiffuse"] = diffuse;
        mpPass["gSpecRough"] = specRough;
        mpPass["gEmissive"] = emissive;

        auto pCB = mpPass["PerFrameCB"];
        pCB["lightCount"] = numLights;
        mpPass["lights"] = mpLightsBuffer;

        pCB["cameraPosition"] = mpScene->getCamera()->getPosition();

        mpPass->execute(pRenderContext, mpFbo);

        if (capturing) {
            if (clock() - lastTime > 2000)
            {
                lastTime = clock();

                gpFramework->getGlobalClock().pause();
                gpFramework->pauseRenderer(true);
                //gpDevice->flushAndSync();
                captures++;
                for (int i = 0; i < 3; i++)
                {
                    commandList5->RSSetShadingRate(dumpRates[i], nullptr);
                    mpPass->execute(pRenderContext, mpFbo);
                    //gpDevice->flushAndSync();
                    //pThis->mpFence->gpuSignal(pCtx->getLowLevelData()->getCommandQueue());

                    output->captureToFile(0, 0, "rendering_" + std::to_string(captures) + "_" + std::to_string(i) + ".exr", Falcor::Bitmap::FileFormat::ExrFile);
                    pictureOutput->captureToFile(0, 0, "rendering_" + std::to_string(captures) + std::to_string(i) + ".png", Falcor::Bitmap::FileFormat::PngFile);
                }
                gpFramework->pauseRenderer(false);
                gpFramework->getGlobalClock().play();
            }
        }

        commandList5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
    }
}

void ExampleBlitPass::renderUI(Gui::Widgets& widget)
{
}
