#include "DeferredCapturePass.h"
#include <filesystem>
#include <string>
#include <cstdlib>

const ChannelList DeferredCapturePass::kGBufferChannels =
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
const std::string kVisBuffer = "visibilityBuffer";
const std::string kShaderFilename = "RenderPasses/DeferredCapturePass/Deferred.slang";

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("DeferredCapturePass", "Write render buffers to image files.", DeferredCapturePass::create);
}

DeferredCapturePass::DeferredCapturePass()
{
    mpPass = FullScreenPass::create(kShaderFilename);
    mpFbo = Fbo::create();
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(Sampler::Filter::Point, Sampler::Filter::Point, Sampler::Filter::Point);
    mpPass["gSampler"] = Sampler::create(samplerDesc);

    mpVars = GraphicsVars::create(mpPass->getProgram()->getReflector());
    ParameterBlockReflection::SharedConstPtr pReflection = mpPass->getProgram()->getReflector()->getParameterBlock("tScene");
    mpSceneBlock = ParameterBlock::create(pReflection);

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
}


void DeferredCapturePass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
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

DeferredCapturePass::SharedPtr DeferredCapturePass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new DeferredCapturePass);
    return pPass;
}

Dictionary DeferredCapturePass::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection DeferredCapturePass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;

    addRenderPassInputs(reflector, kGBufferChannels);
    reflector.addInputOutput("color", "the color texture").format(ResourceFormat::RGBA32Float).texture2D(0,0, 1);
    reflector.addInput(kVisBuffer, "Visibility buffer used for shadowing. Range is [0,1] where 0 means the pixel is fully-shadowed and 1 means the pixel is not shadowed at all").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addOutput("normalsOut", "World-space normal, [0,1] range.").format(ResourceFormat::RGBA8Unorm).texture2D(0, 0, 1);
    reflector.addOutput("viewDirsOut", "View directions").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("viewNormalsOut", "View-space normals").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addOutput("roughOpacVisOut", "Roughness and opacity and visibility in dedicated texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    return reflector;
}

void DeferredCapturePass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // renderData holds the requested resources
    const auto& output = renderData["color"]->asTexture();
    const auto& normalsOut = renderData["normalsOut"]->asTexture();
    const auto& viewDirsOut = renderData["viewDirsOut"]->asTexture();
    const auto& viewNormalsOut = renderData["viewNormalsOut"]->asTexture();
    const auto& roughOpacVisOut = renderData["roughOpacVisOut"]->asTexture();

    mpFbo->attachColorTarget(output, 0);
    mpFbo->attachColorTarget(normalsOut, 1);
    mpFbo->attachColorTarget(viewDirsOut, 2);
    mpFbo->attachColorTarget(viewNormalsOut, 3);
    mpFbo->attachColorTarget(roughOpacVisOut, 4);

    for (int i = 1; i < 5; i++)
    {
        pRenderContext->clearRtv(mpFbo->getRenderTargetView(i).get(), float4(0,0,0,1));
    }

    if (mpScene)
    {
        // Cast to required command list
        d3d_call(pRenderContext->getLowLevelData()->getCommandList()->QueryInterface(IID_PPV_ARGS(&commandList5)));
        commandList5->RSSetShadingRate(activeRate, nullptr);

        const auto& posW = renderData["posW"]->asTexture();
        const auto& normW = renderData["normW"]->asTexture();
        const auto& diffuseOpacity = renderData["diffuseOpacity"]->asTexture();
        const auto& specRough = renderData["specRough"]->asTexture();
        const auto& emissive = renderData["emissive"]->asTexture();
        const auto& visibility = renderData[kVisBuffer]->asTexture();

        mpPass[kVisBuffer] = visibility;
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

        waitedFrames++;
        if (waitedFrames % framesWait)
        {
            return; // No capturing right now
        }

        if (capturing) {
            float antiBias = 0.9f + static_cast <float> (rand()) /( static_cast <float> (RAND_MAX/(0.2f)));

            if (viewpointMethod != ViewpointGeneration::FromScenePath || clock() - lastCaptureTime > (captureInterval * antiBias)) {
                if (viewpointMethod == ViewpointGeneration::FromFile) {
                    viewPointToCapture++;

                    if (viewPointToCapture == 1)
                    {
                        auto camMatrices2 = camMatrices;
                        std::queue<glm::mat4> toQueue;
                        while (!camMatrices2.empty())
                        {
                            auto& c = camMatrices2.front();
                            camMatrices2.pop();
                            for (int i = 0; i < framesWait; i++)
                            {   toQueue.push(c);  }
                        }
                        mpScene->getCamera()->queueConfigs(toQueue);
                        waitedFrames = 0;
                        return;
                    }
                    else if (viewPointToCapture > camMatrices.size())
                    {
                        capturing = false;
                    }
                }

                std::filesystem::path targetPath(targetDir);
                std::stringstream ss;
                ss << std::setw(4) << std::setfill('0') << framesCaptured;
                std::string capturedStr = ss.str();
                targetPath /= capturedStr;

                gpFramework->getGlobalClock().pause();
                gpFramework->pauseRenderer(true);

                const D3D12_SHADING_RATE dumpRates[3] = { D3D12_SHADING_RATE_1X1, D3D12_SHADING_RATE_2X2, D3D12_SHADING_RATE_4X4 };
                const std::string dumpRateNames[3] = { "1x1", "2x2", "4x4" };

                if (doingPrevious)
                {
                    if (!std::filesystem::create_directory(targetPath))
                    {
                        if (viewpointMethod == ViewpointGeneration::FromFile) {
                            viewPointToCapture--;
                        }
                        logError("Failed to create directory " + targetPath.string() + " for frame dumps!", Logger::MsgBox::RetryAbort);
                        commandList5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
                        gpFramework->pauseRenderer(false);
                        gpFramework->getGlobalClock().play();
                        return;
                    }

                    doingPrevious = false;

                    std::filesystem::path prevFile = targetPath / ("previous_" + dumpRateNames[0] + fileEnding);
                    output->captureToFile(0, 0, prevFile.string(), dumpFormat);
                }
                else
                {
                    doingPrevious = true;

                    std::filesystem::path diffuseOpacityFile = targetPath / ("diffuse-opacity_" + dumpRateNames[0] + fileEnding);
                    std::filesystem::path specRoughFile = targetPath / ("specular-roughness_" + dumpRateNames[0] + fileEnding);
                    std::filesystem::path emissiveFile = targetPath / ("emissive_" + dumpRateNames[0] + fileEnding);
                    std::filesystem::path viewFile = targetPath / ("view_" + dumpRateNames[0] + fileEnding);
                    std::filesystem::path normalFile = targetPath / ("normal_" + dumpRateNames[0] + fileEnding);
                    std::filesystem::path roughOpacVisFile = targetPath / ("roughness-opacity-visibility_" + dumpRateNames[0] + fileEnding);

                    diffuseOpacity->captureToFile(0, 0, diffuseOpacityFile.string(), dumpFormat);
                    specRough->captureToFile(0, 0, specRoughFile.string(), dumpFormat);
                    emissive->captureToFile(0, 0, emissiveFile.string(), dumpFormat);
                    viewDirsOut->captureToFile(0, 0, viewFile.string(), dumpFormat);
                    viewNormalsOut->captureToFile(0, 0, normalFile.string(), dumpFormat);
                    roughOpacVisOut->captureToFile(0, 0, roughOpacVisFile.string(), dumpFormat);

                    for (int i = 0; i < sizeof(dumpRates) / sizeof(dumpRates[0]); i++)
                    {
                        commandList5->RSSetShadingRate(dumpRates[i], nullptr);
                        mpPass->execute(pRenderContext, mpFbo);
                        std::filesystem::path outputFile = targetPath / ("output_" + dumpRateNames[i] + fileEnding);
                        output->captureToFile(0, 0, outputFile.string(), dumpFormat);
                    }

                    framesCaptured++;
                }

                gpFramework->pauseRenderer(false);
                gpFramework->getGlobalClock().play();

                lastCaptureTime = clock();

                commandList5->RSSetShadingRate(D3D12_SHADING_RATE_1X1, nullptr);
            }
        }
    }
}

void DeferredCapturePass::loadViewPoints()
{
    std::string filename;
    FileDialogFilterVec filters = { {"cfg", "txt"} };
    if (openFileDialog(filters, filename))
    {
        std::ifstream infile(filename, std::ios_base::in);
        if (!infile.good())
        {
            logError("Failed to open file: " + filename, Logger::MsgBox::ContinueAbort);
            return;
        }

        camMatrices = std::queue<glm::mat4x4>();

        std::string line;
        while (std::getline(infile, line))
        {
            std::stringstream ss(line);
            glm::mat4x4 mats[2];
            for (int l = 0; l < 2; l++) //actual view point, previous viewpoint
            {
                glm::mat4x4& mat = mats[l];
                for (int i = 0; i < 4; i++)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        ss >> mat[j][i];
                    }
                }
                for (int j = 0; j < 4; j++)
                {
                    float v = mat[j][1];
                    float w = mat[j][2];
                    mat[j][2] = -v;
                    mat[j][1] = w;
                }
                mat[2] = -mat[2]; // Apparently handedness mismatch with blender
            }
            camMatrices.push(mats[1]);
            camMatrices.push(mats[0]);
        }
    }
}

void DeferredCapturePass::renderUI(Gui::Widgets& widget)
{
    // Capturing control
    if (mpScene && mpScene->getCamera()->hasAnimation())
    {
        widget.dropdown("Viewpoint Generation", viewpointList, (uint32_t&)viewpointMethod);
    }

    if (viewpointMethod == ViewpointGeneration::FromFile)
    {
        if (widget.button("Load Viewpoints")) { loadViewPoints(); }
    }
    else
    {
        widget.var("Capture interval (ms)", captureInterval);
    }

    widget.textbox("Directory for captured images", targetDir);

    if (!capturing && widget.button("Start capturing"))
    {
        doingPrevious = true;
        viewPointToCapture = 0;
        capturing = true;
    }
    else if (capturing && widget.button("Stop capturing"))
    {
        capturing = false;
    }
}
