#include "CapturePass.h"
#include <filesystem>
#include <string>
#include <cstdlib>

const Gui::DropdownList ViewpointMethods = {
    {(uint32_t)CapturePass::ViewpointGeneration::FromFile, "Viewpoint file"},
    {(uint32_t)CapturePass::ViewpointGeneration::FromGameplay, "Gameplay"}
};

const CapturePass::ImageFormat ImageFormats[] = {
    {Bitmap::FileFormat::JpegFile, "jpg"},
    {Bitmap::FileFormat::PfmFile, "pfm"}
};

const ChannelList GBufferChannels =
{
    { "color1x1",     "",    "Full shading result"                                                           },
    { "color2x2",     "",    "Half shading result"                                                           },
    { "diffuse",      "",    "Diffuse color"                                                                 },
    { "specular",     "",    "Specular color"                                                                },
    { "emissive",     "",    "Emissive color"                                                                },
    { "reproject",    "",    "Previous frame reprojected shading result"                                     },
    { "visibility",   "",    "Shadowing visibility buffer"                                                   },
    { "normals",      "",    "View-space normals",                           true, ResourceFormat::RGBA8Unorm},
    { "extras",       "",    "Roughness, opacity and visibility"                                             },
};

// Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary& lib)
{
    lib.registerClass("CapturePass", "Dumps rendering data at multiple shading resolutions to image files.", CapturePass::create);
}

CapturePass::SharedPtr CapturePass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr p = SharedPtr(new CapturePass);

    for (const auto& [key, value] : dict)
      if (key == "format")
          p->setImageFormat(value);

    return p;
}

void CapturePass::setImageFormat(Bitmap::FileFormat format)
{
    for (const auto& supported : ImageFormats)
        if (supported.id == format) {
            dumpFormat = supported;
            return;
        }
}

Dictionary CapturePass::getScriptingDictionary()
{
    Dictionary d;
    d["format"] = dumpFormat.extension;
    return d;
}

void CapturePass::setScene(RenderContext* context, const Scene::SharedPtr& scene)
{
    this->scene = scene;
}

RenderPassReflection CapturePass::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    for (const auto& channel : GBufferChannels)
        reflector.addInputOutput(channel.name, channel.desc).format(channel.format).texture2D(0,0,1).flags(RenderPassReflection::Field::Flags::Optional);

    return reflector;
}

void CapturePass::execute(RenderContext* context, const RenderData& data)
{
  if (capturing) {
    if (viewpointMethod == ViewpointGeneration::FromGameplay) {
      if (clock() > delay) {
        gpFramework->getGlobalClock().pause();
        gpFramework->pauseRenderer(true);


        dumpReproject(data);
        dumpFrame(data);

        gpFramework->pauseRenderer(false);
        gpFramework->getGlobalClock().play();

        delay = clock() + captureInterval * (0.9f + (float) (rand()) /( (float) (RAND_MAX/(0.2f))));
      }
    } else if (viewpointMethod == ViewpointGeneration::FromFile) {
      bool waited = delay <= 0;
      bool immediate = delay == framesWait;
      bool isMain = (viewpoints.size() % 2) == 1;
      bool done = viewpoints.size() == 0;

      if (done) {
          capturing = false;
      } else if (waited) {
        if (isMain) dumpFrame(data);
        nextViewpoint();
      } else {
        if (isMain && immediate) dumpReproject(data);
        delay--;
      }
    }
  }
}

void CapturePass::nextViewpoint()
{
  scene->getCamera()->updateFromAnimation(viewpoints.front());
  delay = (float) framesWait;
  viewpoints.pop();
}

void CapturePass::dumpReproject(const RenderData& data)
{
  auto path = dumpPath();

  std::filesystem::create_directory(path);
  data["reproject"]->asTexture()->captureToFile(0,0, (path / ("reproject_1x1." + dumpFormat.extension)).string(), dumpFormat.id);
}

void CapturePass::dumpFrame(const RenderData& data)
{
  auto path = dumpPath();

  data["color1x1"]->asTexture()->captureToFile(0,0, (path / ("output_1x1." + dumpFormat.extension)).string(), dumpFormat.id);
  data["color2x2"]->asTexture()->captureToFile(0,0, (path / ("output_2x2." + dumpFormat.extension)).string(), dumpFormat.id);
  numDumped++;
}

std::filesystem::path CapturePass::dumpPath()
{
  std::stringstream subdir;
  subdir << std::setw(4) << std::setfill('0') << numDumped;
  return (std::filesystem::path) dumpDir / subdir.str();
}

void CapturePass::renderUI(Gui::Widgets& widget)
{
    if (scene)
        widget.dropdown("Viewpoint Generation", ViewpointMethods, (uint32_t&)viewpointMethod);

    if (viewpointMethod == ViewpointGeneration::FromFile) {
        if (widget.button("Load Viewpoints"))
          loadViewpoints();
    } else {
        widget.var("Capture interval (ms)", captureInterval);
    }

    widget.textbox("Directory for captured images", dumpDir);

    if (!capturing && widget.button("Start capturing")) {
        capturing = true;
    } else if (capturing && widget.button("Stop capturing")) {
        capturing = false;
    }
}

void CapturePass::loadViewpoints()
{
    FileDialogFilterVec filters = { {"cfg", "txt"} };
    std::string filename;

    if (openFileDialog(filters, filename)) {
        std::ifstream infile(filename, std::ios_base::in);
        if (!infile.good()) {
            logError("Failed to open file: " + filename, Logger::MsgBox::ContinueAbort);
            return;
        }

        viewpoints = std::queue<glm::mat4x4>();
        std::string line;

        while (std::getline(infile, line)) {
            std::stringstream ss(line);
            glm::mat4x4 mats[2];

            for (int l = 0; l < 2; l++) { //actual view point, previous viewpoint
                glm::mat4x4& mat = mats[l];

                for (int i = 0; i < 4; i++)
                    for (int j = 0; j < 4; j++)
                        ss >> mat[j][i];

                for (int j = 0; j < 4; j++) {
                    float v = mat[j][1];
                    float w = mat[j][2];

                    mat[j][2] = -v;
                    mat[j][1] = w;
                }

                mat[2] = -mat[2]; // Apparently handedness mismatch with blender
            }

            viewpoints.push(mats[1]);
            viewpoints.push(mats[0]);
        }
    }
}
