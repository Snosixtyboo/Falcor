#include "CapturePass.h"
#include <filesystem>
#include <string>
#include <cstdlib>

const std::string kShaderFilename = "RenderPasses/CapturePass/CapturePass.slang";
const ChannelList CapturePass::kGBufferChannels =
{
    { "diffuse", "gDiffuseOpacity",        "Diffuse color",    true /* optional */, ResourceFormat::RGBA32Float },
    { "specular",      "gSpecRough",       "Specular color",   true /* optional */, ResourceFormat::RGBA32Float },
    { "emissive",       "gEmissive",       "Emissive color",   true /* optional */, ResourceFormat::RGBA32Float },
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("CapturePass", "Writes gbuffer data at multiple shading resolutions to image files.", CapturePass::create);
}

CapturePass::CapturePass()
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

CapturePass::SharedPtr CapturePass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pPass = SharedPtr(new CapturePass);

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

    return pPass;
}

Dictionary CapturePass::getScriptingDictionary()
{
    return Dictionary();
}

RenderPassReflection CapturePass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    addRenderPassInputs(reflector, kGBufferChannels);

    reflector.addInput("composite", "Final color result").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addInput("visibility", "Shadowing visibility buffer").flags(RenderPassReflection::Field::Flags::Optional);
    reflector.addInput("normals", "View-space normals").format(ResourceFormat::RGBA8Unorm).texture2D(0, 0, 1);
    reflector.addInput("extraMat", "Roughness, opacity and visibility in dedicated texture").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    reflector.addInput("motion", "Motion vectors").format(ResourceFormat::RGBA32Float).texture2D(0, 0, 1);
    return reflector;
}

void CapturePass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
  
}

void CapturePass::loadViewPoints()
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

void CapturePass::renderUI(Gui::Widgets& widget)
{
    // Capturing control
    if (mpScene && mpScene->getCamera()->hasAnimation()) {
        widget.dropdown("Viewpoint Generation", viewpointList, (uint32_t&)viewpointMethod);
    }

    if (viewpointMethod == ViewpointGeneration::FromFile) {
        if (widget.button("Load Viewpoints"))
          loadViewPoints();
    } else {
        widget.var("Capture interval (ms)", captureInterval);
    }

    widget.textbox("Directory for captured images", targetDir);

    if (!capturing && widget.button("Start capturing")) {
        doingPrevious = true;
        viewPointToCapture = 0;
        capturing = true;
    } else if (capturing && widget.button("Stop capturing")) {
        capturing = false;
    }
}
