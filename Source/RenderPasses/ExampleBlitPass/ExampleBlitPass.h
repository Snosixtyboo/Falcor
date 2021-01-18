/***************************************************************************
 # Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"

#include "RenderGraph/RenderPassHelpers.h"

using namespace Falcor;

class ExampleBlitPass : public RenderPass
{
private:
    enum class ViewpointGeneration { FromFile, FromScenePath};

    std::string targetDir = ".";
    size_t captureInterval = 1000;
    Falcor::Bitmap::FileFormat dumpFormat = Falcor::Bitmap::FileFormat::PfmFile;
    ViewpointGeneration viewpointMethod = ViewpointGeneration::FromFile;

    Gui::DropdownList viewpointList = {
        {(uint32_t)ViewpointGeneration::FromFile, "Viewpoint file"},
        {(uint32_t)ViewpointGeneration::FromScenePath, "Scene path"}
    };

    bool capturing = false;
    int framesCaptured;
    std::queue<glm::mat4x4> camMatrices;
    uint32_t viewPointToCapture = 0;
    size_t lastCaptureTime;
    std::string fileEnding;

    int activeRateID = 0;
    D3D12_SHADING_RATE activeRate = D3D12_SHADING_RATE_1X1;
    ID3D12GraphicsCommandList5Ptr commandList5;

    // Constants used in derived classes
    static const ChannelList kGBufferChannels;

    FullScreenPass::SharedPtr mpPass;
    Fbo::SharedPtr mpFbo;
    GraphicsVars::SharedPtr mpVars;
    Scene::SharedPtr mpScene;
    ParameterBlock::SharedPtr mpSceneBlock;

    int numLights;
    ShaderVar lightsBufferVar;
    Buffer::SharedPtr mpLightsBuffer;
    LightProbe::SharedPtr lightProbe;

    ExampleBlitPass();
    void loadViewPoints();

public:
    using SharedPtr = std::shared_ptr<ExampleBlitPass>;

    /** Create a new render pass object.
        \param[in] pRenderContext The render context.
        \param[in] dict Dictionary of serialized parameters.
        \return A new object, or an exception is thrown if creation failed.
    */
    static SharedPtr create(RenderContext* pRenderContext = nullptr, const Dictionary& dict = {});

    virtual std::string getDesc() override { return "Blits one texture into another texture"; }
    virtual Dictionary getScriptingDictionary() override;
    virtual RenderPassReflection reflect(const CompileData& compileData) override;
    virtual void compile(RenderContext* pContext, const CompileData& compileData) override {}
    virtual void execute(RenderContext* pRenderContext, const RenderData& renderData) override;
    virtual void renderUI(Gui::Widgets& widget) override;
    virtual void setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene) override;
    virtual bool onMouseEvent(const MouseEvent& mouseEvent) override { return false; }

    const D3D12_SHADING_RATE rates[7] = {
                D3D12_SHADING_RATE_1X1,
                D3D12_SHADING_RATE_1X2,
                D3D12_SHADING_RATE_2X1,
                D3D12_SHADING_RATE_2X2,
                D3D12_SHADING_RATE_2X4,
                D3D12_SHADING_RATE_4X2,
                D3D12_SHADING_RATE_4X4 };

    virtual bool onKeyEvent(const KeyboardEvent& keyEvent) override
    {
        if (keyEvent.type == KeyboardEvent::Type::KeyReleased)
        {
            if (keyEvent.key == KeyboardEvent::Key::R)
            {
                activeRateID = (activeRateID + 1) % 7;
                activeRate = rates[activeRateID];
                return true;
            }
            if (keyEvent.key == KeyboardEvent::Key::C)
            {
                capturing = !capturing;
                return true;
            }
        }
        return false;
    }
};
