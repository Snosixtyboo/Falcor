#include "CapturePass.h"
#include <filesystem>
#include <string>
#include <cstdlib>

const ChannelList DumpChannels =
{
    { "color1x1",     "output_1x1",      "Full shading result"                                   },
    { "color2x2",     "output_2x2",      "Half shading result"                                   },
    { "reproject",    "reproject_1x1",   "Previous frame reprojected shading result",       true },
    { "diffuse",      "diffuse_1x1",     "Diffuse color"                                         },
    { "specular",     "specular_1x1",    "Specular color + roughness",                      true },
    //{ "emissive",     "emissive_1x1",    "Emissive color"                                      },
    { "normals",      "normals_1x1",      "View-space normals",                                  },
    { "extras",       "extra_1x1",       "Any additional inputs"                                 },
};

const auto ReprojectChannel = DumpChannels[2];
const Gui::DropdownList ViewpointMethods = {
    {(uint32_t) CapturePass::ViewpointGeneration::FromFile, "Viewpoint file"},
    {(uint32_t) CapturePass::ViewpointGeneration::FromGameplay, "Gameplay intervals"}
};

const CapturePass::ImageFormat DumpFormats[] = {
    { Bitmap::FileFormat::BmpFile, "bmp" },
    { Bitmap::FileFormat::ExrFile, "exr", true },
    { Bitmap::FileFormat::JpegFile, "jpg" },
    { Bitmap::FileFormat::PfmFile, "pfm", true },
    { Bitmap::FileFormat::PngFile, "png" },
    { Bitmap::FileFormat::TgaFile, "tga" },
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
    for (const auto& channel : DumpChannels)
        reflector.addInputOutput(channel.name, channel.desc).format(channel.format).flags(RenderPassReflection::Field::Flags::Optional);

    reflector.addInternal("srgb", "Buffer for 8-bit image export.").format(ResourceFormat::RGBA8UnormSrgb);
    return reflector;
}

void CapturePass::execute(RenderContext* context, const RenderData& data)
{
    if (capturing) {
        if (viewpointMethod == ViewpointGeneration::FromGameplay) {
            if (clock() > delay) {
                dumpReproject(context, data);
                dumpFrame(context, data);

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
                if (isMain) dumpFrame(context, data);
                nextViewpoint();
            } else {
                if (isMain && immediate) dumpReproject(context, data);
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

void CapturePass::dumpReproject(RenderContext* context, const RenderData& data)
{
    dumpFile(context, data, ReprojectChannel);
}

void CapturePass::dumpFrame(RenderContext* context, const RenderData& data)
{
    for (const auto& channel : DumpChannels)
        if (channel.name.compare(ReprojectChannel.name) != 0)
            dumpFile(context, data, channel);

    numDumped++;
}

void CapturePass::dumpFile(RenderContext* context, const RenderData& data, ChannelDesc channel)
{
    auto out = (dumpFormat.hdr ? data[channel.name] : data["srgb"])->asTexture();
    auto subdir = std::stringstream() << std::setw(4) << std::setfill('0') << numDumped;
    auto path = (std::filesystem::path)dumpDir / subdir.str();

    if (!dumpFormat.hdr)
        context->blit(data[channel.name]->asTexture()->getSRV(), out->getRTV(), uint4(-1), uint4(-1), Sampler::Filter::Point);

    std::filesystem::create_directory(path);
    out->captureToFile(0,0, (path / (channel.texname + "." + dumpFormat.extension)).string(), dumpFormat.id, channel.optional ? Bitmap::ExportFlags::ExportAlpha : Bitmap::ExportFlags::None);
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

    auto value = (uint32_t)dumpFormat.id;
    auto drop = Gui::DropdownList();
    for (auto& format : DumpFormats)
        drop.push_back({ (uint32_t)format.id, format.extension });

    if (widget.dropdown("File Format", drop, value))
        for (auto& format : DumpFormats)
            if (value == (uint32_t)format.id)
                dumpFormat = format;

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
