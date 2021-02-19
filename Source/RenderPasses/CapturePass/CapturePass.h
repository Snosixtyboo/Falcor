#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"
#include "RenderGraph/RenderPassHelpers.h"

using namespace Falcor;

class CapturePass : public RenderPass
{
public:
    enum class ViewpointGeneration { FromFile, FromGameplay };
    using SharedPtr = std::shared_ptr<CapturePass>;

    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {});
    virtual std::string getDesc() override { return "Dumps rendering data at multiple shading resolutions to image files."; }

    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void compile(RenderContext* context, const CompileData& data) override {}
    virtual void setScene(RenderContext* context, const Scene::SharedPtr& scene) override;
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void renderUI(Gui::Widgets& widget) override;

private:
    std::string dumpDir = ".";
    Gui::DropdownValue dumpFormat = {(uint32_t)Bitmap::FileFormat::PfmFile, "pfm"};
    ViewpointGeneration viewpointMethod = ViewpointGeneration::FromFile;
    size_t captureInterval = 1000;
    size_t framesWait = 3;

    Scene::SharedPtr scene;
    std::queue<glm::mat4x4> viewpoints;
    bool capturing = false;
    int numDumped = 0;
    float delay = 0;

    void loadViewpoints();
    void nextViewpoint();
    void dumpReproject(const RenderData& data);
    void dumpFrame(const RenderData& data);
    std::filesystem::path dumpPath();
};
