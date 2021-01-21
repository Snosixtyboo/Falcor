#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"

using namespace Falcor;

class SceneWritePass : public RenderPass
{
public:
    using SharedPtr = std::shared_ptr<SceneWritePass>;

    static const char* kDesc;

    /** Create a new object
    */
    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void execute(RenderContext* pContext, const RenderData& renderData) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual void renderUI(Gui::Widgets& widget) override;
    virtual Dictionary getScriptingDictionary() override;
    virtual std::string getDesc() override { return kDesc; }

    SceneWritePass& setWriteFilePath(std::string path);
    SceneWritePass& setDepthBufferFormat(ResourceFormat format);
    SceneWritePass& setDepthStencilState(const DepthStencilState::SharedPtr& pDsState);
    SceneWritePass& setRasterizerState(const RasterizerState::SharedPtr& pRsState);

private:

    static const char* outFileName;

    SceneWritePass(const Dictionary& dict);
    void parseDictionary(const Dictionary& dict);

    Fbo::SharedPtr mpFbo;
    GraphicsState::SharedPtr mpState;
    GraphicsVars::SharedPtr mpVars;
    RasterizerState::SharedPtr mpRsState;
    ResourceFormat mDepthFormat = ResourceFormat::D32Float;
    std::string mOutFilePath = "out.ply";
    Scene::SharedPtr mpScene;

    //For writing geometry
    Buffer::SharedPtr   mDumpGeomBuffer;
    Buffer::SharedPtr   mDumpGeomCounter;
    int mDumpGeomSize;
};
