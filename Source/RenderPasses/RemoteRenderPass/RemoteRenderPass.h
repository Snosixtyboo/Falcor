#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"
#include <winsock.h>

using namespace Falcor;

class WSOrg
{
public:
    int port = 4242;
    sockaddr_in sockaddr;
    SOCKET sockfd;
    SOCKET connection = 0;
    WSOrg();
    ~WSOrg();
};

class RemoteRenderPass : public RenderPass
{
public:
    using SharedPtr = std::shared_ptr<RemoteRenderPass>;
    static SharedPtr create(RenderContext* context = nullptr, const Dictionary& dict = {});
    static const char* desc;

    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData& data) override;
    virtual void execute(RenderContext* context, const RenderData& data) override;
    virtual void setScene(RenderContext* context, const Scene::SharedPtr& scene) override;
    virtual void renderUI(Gui::Widgets& widget) override;
    virtual std::string getDesc() override { return desc; }

    void setFilter(uint32_t filter);
    void setScale(float scale) { this->scale = scale; }
    float getScale() { return scale; }
    uint32_t getFilter() { return (uint32_t)filter; }

private:
    WSOrg wsorg;
    QueryHeap::SharedPtr occlusionHeap;
    Buffer::SharedPtr occlusionBuffer;
    GpuFence::SharedPtr copyFence;

    bool capturing = false;
    int viewPointsToDo = 0;
    int donePoints = 0;
    float threshold;
    std::vector<char> passed;

    RemoteRenderPass();
    void loadImage();
    void setTexture(const Texture::SharedPtr& texture);

    float scale = 1;
    bool srgb = true;
    Sampler::Filter filter = Sampler::Filter::Linear;
    Texture::SharedPtr texture;
    std::string textureName;

    Scene::SharedPtr cubeScene;
    GraphicsProgram::SharedPtr program;
    GraphicsVars::SharedPtr vars;
    GraphicsState::SharedPtr state;
    Fbo::SharedPtr framebuffer;
    Scene::SharedPtr scene;
};
