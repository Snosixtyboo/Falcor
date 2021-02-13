#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"

#include "RenderGraph/RenderPassHelpers.h"

using namespace Falcor;

class DeferredLightingPass : public RenderPass
{
private:
    static const ChannelList kGBufferChannels;

    FullScreenPass::SharedPtr mpPass;
    Fbo::SharedPtr mpFbo;
    GraphicsVars::SharedPtr mpVars;
    Scene::SharedPtr mpScene;
    ParameterBlock::SharedPtr mpSceneBlock;
    glm::mat4x4 prevVP;

    int numLights;
    ShaderVar lightsBufferVar;
    Buffer::SharedPtr mpLightsBuffer;
    LightProbe::SharedPtr lightProbe;

    DeferredLightingPass();
    bool doingPrevious = true;

public:
    using SharedPtr = std::shared_ptr<DeferredLightingPass>;
    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual std::string getDesc() override { return "Blits one texture into another texture"; }
    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pContext, const CompileData& compileData) override {}
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual bool onMouseEvent(const MouseEvent& mouseEvent) override { return false; }
};
