#pragma once
#include "Falcor.h"
#include "RenderGraph/RenderPassHelpers.h"

using namespace Falcor;

class CapturePass : public RenderPass
{
public:
    struct ImageFormat { Bitmap::FileFormat id; std::string extension;  bool hdr = false; };
    enum class ViewpointGeneration { FromFile, FromGameplay };
    using SharedPtr = std::shared_ptr<CapturePass>;

    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {});
    static const char* desc;

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {}
    virtual void setScene(RenderContext* context, const Scene::SharedPtr& scene) override;
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void renderUI(Gui::Widgets& widget) override;
    virtual std::string getDesc() override { return desc; }

private:
    std::string dumpDir = ".";
    ImageFormat dumpFormat = { Bitmap::FileFormat::PngFile, "png" };
    ViewpointGeneration viewpointMethod = ViewpointGeneration::FromFile;
    size_t captureInterval = 1000;
    size_t framesWait = 4;

    Scene::SharedPtr scene;
    std::queue<glm::mat4x4> viewpoints;
    bool capturing = false;
    int numDumped = 0;
    float delay = 0;

    void loadViewpoints();
    void nextViewpoint();
    void dumpReproject(RenderContext* context, const RenderData& data);
    void dumpFrame(RenderContext* context, const RenderData& data);
    void dumpFile(RenderContext* context, const RenderData& data, ChannelDesc channel);
};
