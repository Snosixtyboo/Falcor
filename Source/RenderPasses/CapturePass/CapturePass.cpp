#include "CapturePass.h"
#include <filesystem>
#include <string>
#include <cstdlib>

const Gui::DropdownList ViewpointMethods = {
    {(uint32_t) CapturePass::ViewpointGeneration::FromFile, "Viewpoint file"},
    {(uint32_t) CapturePass::ViewpointGeneration::FromGameplay, "Gameplay intervals"}
};

const Gui::DropdownList ImageFormats = {
    {(uint32_t) Bitmap::FileFormat::BmpFile, "bmp"},
    {(uint32_t) Bitmap::FileFormat::ExrFile, "exr"},
    {(uint32_t) Bitmap::FileFormat::JpegFile, "jpg"},
    {(uint32_t) Bitmap::FileFormat::PfmFile, "pfm"},
    {(uint32_t) Bitmap::FileFormat::PngFile, "png"},
    {(uint32_t) Bitmap::FileFormat::TgaFile, "tga"},
};

const ChannelList Channels =
{
    { "color1x1",     "output_1x1",      "Full shading result"                                   },
    { "color2x2",     "output_2x2",      "Half shading result"                                   },
    { "reproject",    "reproject_1x1",   "Previous frame reprojected shading result"             },
    { "diffuse",      "diffuse_1x1",     "Diffuse color"                                         },
    { "specular",     "specular_1x1",    "Specular color"                                        },
    //{ "emissive",     "emissive_1x1",    "Emissive color"                                        },
    { "normals",      "normals_1x1",      "View-space normals",                                  },
    { "extras",       "extra_1x1",       "Roughness, opacity and visibility"                     },
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
    return SharedPtr(new CapturePass);
}

void CapturePass::setScene(RenderContext* context, const Scene::SharedPtr& scene)
{
    this->scene = scene;
}

RenderPassReflection CapturePass::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    for (const auto& channel : Channels)
        reflector.addInputOutput(channel.name, channel.desc).format(channel.format).texture2D(0,0,1).flags(RenderPassReflection::Field::Flags::Optional);

    return reflector;
}

void CapturePass::execute(RenderContext* context, const RenderData& data)
{
  if (capturing) {
    if (viewpointMethod == ViewpointGeneration::FromGameplay) {
      if (clock() > delay) {
        dumpReproject(data);
        dumpFrame(data);

        delay = clock() + captureInterval * (0.9f + (float) (rand()) /( (float) (RAND_MAX/(0.2f))));
      }
    } else if (viewpointMethod == ViewpointGeneration::FromFile) {
      bool waited = delay <= 0;
      bool immediate = delay == framesWait;
      bool isMain = (viewpoints.size() % 2) == 0;
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
  data["reproject"]->asTexture()->captureToFile(0,0, (path / ("reproject_1x1." + dumpFormat.label)).string(), (Bitmap::FileFormat)dumpFormat.value);
}

void CapturePass::dumpFrame(const RenderData& data)
{
  auto path = dumpPath();
  for (const auto& channel : Channels)
    if (channel.name.compare("reproject") != 0)
      data[channel.name]->asTexture()->captureToFile(0,0, (path / (channel.texname + "." + dumpFormat.label)).string(), (Bitmap::FileFormat)dumpFormat.value);

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

    widget.textbox("Destination Directory", dumpDir);

    uint32_t format = dumpFormat.value;
    if (widget.dropdown("File Format", ImageFormats, format))
        for (auto& line : ImageFormats)
            if (line.value == format)
                dumpFormat = line;

    if (!capturing && widget.button("Start capturing")) {
        capturing = true;
    } else if (capturing && widget.button("Stop capturing")) {
        capturing = false;
    }
}

void CapturePass::loadViewpoints()
{
    std::string filename;

    if (openFileDialog({{"cfg", "txt"}}, filename)) {
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

            for (int l = 0; l < 2; l++) { // actual view point, previous viewpoint
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

                mat[2] = -mat[2]; // apparently handedness mismatch with blender
            }

            viewpoints.push(mats[1]);
            viewpoints.push(mats[0]);
        }

        nextViewpoint();
    }
}
