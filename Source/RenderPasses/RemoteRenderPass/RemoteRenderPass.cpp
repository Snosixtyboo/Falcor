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
#include "RemoteRenderPass.h"
#include "glm/gtx/transform.hpp"

 // Don't remove this. it's required for hot-reload to function properly
extern "C" __declspec(dllexport) const char* getProjDir()
{
    return PROJECT_DIR;
}

static void regRemoteRenderPass(pybind11::module& m)
{
    pybind11::class_<RemoteRenderPass, RenderPass, RemoteRenderPass::SharedPtr> pass(m, "RemoteRenderPass");
    pass.def_property("scale", &RemoteRenderPass::getScale, &RemoteRenderPass::setScale);
    pass.def_property("filter", &RemoteRenderPass::getFilter, &RemoteRenderPass::setFilter);
}

extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerClass("RemoteRenderPass", "Render an environment map", RemoteRenderPass::create);
    ScriptBindings::registerBinding(regRemoteRenderPass);
}
const char* RemoteRenderPass::kDesc = "Render an environment-map. The map can be provided by the user or taken from a scene";

namespace
{
    const Gui::DropdownList kFilterList =
    {
        { (uint32_t)Sampler::Filter::Linear, "Linear" },
        { (uint32_t)Sampler::Filter::Point, "Point" },
    };

    const std::string kTarget = "target";
    const std::string kDepth = "depth";

    // Dictionary keys
    const std::string kTexName = "texName";
    const std::string kLoadAsSrgb = "loadAsSrgb";
    const std::string kFilter = "filter";
}

WSOrg::WSOrg()
{}

WSOrg::~WSOrg()
{
    WSACleanup();
}

RemoteRenderPass::RemoteRenderPass()
{
    mpCubeScene = Scene::create("cube.obj");
    if (mpCubeScene == nullptr) throw std::runtime_error("RemoteRenderPass::RemoteRenderPass - Failed to load cube model");

    mpProgram = GraphicsProgram::createFromFile("RenderPasses/Skybox/Skybox.slang", "vs", "ps");
    mpProgram->addDefines(mpCubeScene->getSceneDefines());
    mpVars = GraphicsVars::create(mpProgram->getReflector());
    mpFbo = Fbo::create();

    // Create state
    mpState = GraphicsState::create();
    BlendState::Desc blendDesc;
    for (uint32_t i = 1; i < Fbo::getMaxColorTargetCount(); i++) blendDesc.setRenderTargetWriteMask(i, false, false, false, false);
    blendDesc.setIndependentBlend(true);
    mpState->setBlendState(BlendState::create(blendDesc));

    // Create the rasterizer state
    RasterizerState::Desc rastDesc;
    rastDesc.setCullMode(RasterizerState::CullMode::Front).setDepthClamp(true);
    mpState->setRasterizerState(RasterizerState::create(rastDesc));

    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::LessEqual);
    mpState->setDepthStencilState(DepthStencilState::create(dsDesc));
    mpState->setProgram(mpProgram);

    setFilter((uint32_t)mFilter);

    occlusionHeap = QueryHeap::create(QueryHeap::Type::Occlusion, 1);
    occlusionBuffer = Buffer::create(8);
}

RemoteRenderPass::SharedPtr RemoteRenderPass::create(RenderContext* pRenderContext, const Dictionary& dict)
{
    SharedPtr pRemoteRenderPass = SharedPtr(new RemoteRenderPass());
    for (const auto& [key, value] : dict)
    {
        if (key == kTexName) pRemoteRenderPass->mTexName = value.operator std::string();
        else if (key == kLoadAsSrgb) pRemoteRenderPass->mLoadSrgb = value;
        else if (key == kFilter) pRemoteRenderPass->setFilter(value);
        else logWarning("Unknown field '" + key + "' in a RemoteRenderPass dictionary");
    }

    std::shared_ptr<Texture> pTexture;
    if (pRemoteRenderPass->mTexName.size() != 0)
    {
        pTexture = Texture::createFromFile(pRemoteRenderPass->mTexName, false, pRemoteRenderPass->mLoadSrgb);
        if (pTexture == nullptr) throw std::runtime_error("RemoteRenderPass::create - Error creating texture from file");
        pRemoteRenderPass->setTexture(pTexture);
    }
    return pRemoteRenderPass;
}

Dictionary RemoteRenderPass::getScriptingDictionary()
{
    Dictionary dict;
    dict[kTexName] = mTexName;
    dict[kLoadAsSrgb] = mLoadSrgb;
    dict[kFilter] = mFilter;
    return dict;
}

RenderPassReflection RemoteRenderPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addOutput(kTarget, "Color buffer").format(ResourceFormat::RGBA32Float);
    auto& depthField = reflector.addInputOutput(kDepth, "Depth-buffer. Should be pre-initialized or cleared before calling the pass").bindFlags(Resource::BindFlags::DepthStencil);
    return reflector;
}

void RemoteRenderPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    mpFbo->attachColorTarget(renderData[kTarget]->asTexture(), 0);
    mpFbo->attachDepthStencilTarget(renderData[kDepth]->asTexture());

    pRenderContext->clearRtv(mpFbo->getRenderTargetView(0).get(), float4(0));

    if (!mpScene) return;

    glm::mat4 world = glm::translate(mpScene->getCamera()->getPosition());
    mpVars["PerFrameCB"]["gWorld"] = world;
    mpVars["PerFrameCB"]["gScale"] = mScale;
    mpVars["PerFrameCB"]["gViewMat"] = mpScene->getCamera()->getViewMatrix();
    mpVars["PerFrameCB"]["gProjMat"] = mpScene->getCamera()->getProjMatrix();
    mpState->setFbo(mpFbo);

    pRenderContext->getLowLevelData()->getCommandList()->BeginQuery(occlusionHeap->getApiHandle(), D3D12_QUERY_TYPE_OCCLUSION, 0);
    mpCubeScene->render(pRenderContext, mpState.get(), mpVars.get(), Scene::RenderFlags::UserRasterizerState);
    pRenderContext->getLowLevelData()->getCommandList()->EndQuery(occlusionHeap->getApiHandle(), D3D12_QUERY_TYPE_OCCLUSION, 0);

    // Resolve the occlusion query and store the results in the query result buffer
    // to be used on the subsequent frame.
    //pRenderContext->resourceBarrier(occlusionBuffer.get(), Resource::State::CopyDest);
    pRenderContext->getLowLevelData()->getCommandList()->ResolveQueryData(occlusionHeap->getApiHandle(), D3D12_QUERY_TYPE_OCCLUSION, 0, 1, occlusionBuffer->getApiHandle(), 0);
    //pRenderContext->resourceBarrier(occlusionBuffer.get(), Resource::State::CopySource);

    float sum = (float)(renderData[kTarget]->asTexture()->getHeight() * renderData[kTarget]->asTexture()->getWidth());
    unsigned long long int* queryRes = (unsigned long long int*)occlusionBuffer->map(Falcor::Buffer::MapType::Read);
    float foreground = 1.0f - ((*queryRes) / sum);
    occlusionBuffer->unmap();

    if (capturing)
    {
        passed[donePoints] = (foreground >= threshold) ? 1 : 0;
        donePoints++;
        if (donePoints == viewPointsToDo)
        {
            int sent = 0;
            while (sent < (int)passed.size())
            {
                sent += send(connection, passed.data() + sent, (int)passed.size() - sent, 0);
            }
            capturing = false;
            closesocket(connection);
        }
    }
    else
    {
        // Grab a connection from the queue
        auto addrlen = sizeof(sockaddr);
        connection = accept(sockfd, (struct sockaddr*)&sockaddr, (int*)&addrlen);
        if (connection == INVALID_SOCKET) {
            return;
        }

        //std::cout << "Connected!" << std::endl;

        struct {
            int numCams;
            float threshold;
        } info;

        while (recv(connection, (char*)&info, sizeof(info), MSG_PEEK) < (int)sizeof(info));
        recv(connection, (char*)&info, sizeof(info), 0);

        std::vector<glm::mat4> matrices(info.numCams);
        while (recv(connection, (char*)matrices.data(), sizeof(glm::mat4) * info.numCams, MSG_PEEK) != sizeof(glm::mat4) * info.numCams);
        recv(connection, (char*)matrices.data(), sizeof(glm::mat4) * info.numCams, 0);

        // Convert blender coordinate system to Falcor
        for (int i = 0; i < matrices.size(); i++)
        {
            glm::mat4& mat = matrices[i];
            mat = transpose(mat);
            for (int j = 0; j < 4; j++)
            {
                //std::cout << mat[j][0] << " " << mat[j][1] << " " << mat[j][2] << " " << mat[j][3] << std::endl;
                float v = mat[j][1];
                float w = mat[j][2];
                mat[j][2] = -v;
                mat[j][1] = w;
            }
            //std::cout << std::endl;
            mat[2] = -mat[2]; // Apparently handedness mismatch with blender
        }

        donePoints = 0;
        threshold = info.threshold;
        viewPointsToDo = info.numCams;
        passed.resize(info.numCams);

        std::queue<glm::mat4> configs;
        for (auto m : matrices)
        {
            configs.push(m);
        }

        mpScene->getCamera()->queueConfigs(configs);

        capturing = true;
    }
}

void RemoteRenderPass::setScene(RenderContext* pRenderContext, const Scene::SharedPtr& pScene)
{
    mpScene = pScene;

    if (mpScene)
    {
        mpCubeScene->setCamera(mpScene->getCamera());
        if (mpScene->getEnvMap()) setTexture(mpScene->getEnvMap()->getEnvMap());
    }

    // Socket
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }

    // Create a socket (IPv4, TCP)
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        std::cout << "Failed to create socket. errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    // Listen to port 4242 on any address
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = INADDR_ANY;
    sockaddr.sin_port = htons(4242); // htons is necessary to convert a number to
                                     // network byte order
    if (bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        std::cout << "Failed to bind to port 4242. errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    // Start listening. Hold at most 10 connections in the queue
    if (listen(sockfd, 10) < 0) {
        std::cout << "Failed to listen on socket. errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    u_long iMode = 1;
    auto iResult = ioctlsocket(sockfd, FIONBIO, &iMode); // Put socket in non-blocking mode
    if (iResult != NO_ERROR)
        printf("ioctlsocket failed with error: %ld\n", iResult);
}

void RemoteRenderPass::renderUI(Gui::Widgets& widget)
{
    float scale = mScale;
    if (widget.var("Scale", scale, 0.f)) setScale(scale);

    if (widget.button("Load Image")) { loadImage(); }

    uint32_t filter = (uint32_t)mFilter;
    if (widget.dropdown("Filter", kFilterList, filter)) setFilter(filter);
}

void RemoteRenderPass::loadImage()
{
    std::string filename;
    FileDialogFilterVec filters = { {"bmp"}, {"jpg"}, {"dds"}, {"png"}, {"tiff"}, {"tif"}, {"tga"} };
    if (openFileDialog(filters, filename))
    {
        mpTexture = Texture::createFromFile(filename, false, mLoadSrgb);
        setTexture(mpTexture);
    }
}

void RemoteRenderPass::setTexture(const Texture::SharedPtr& pTexture)
{
    mpTexture = pTexture;
    if (mpTexture)
    {
        assert(mpTexture->getType() == Texture::Type::TextureCube || mpTexture->getType() == Texture::Type::Texture2D);
        (mpTexture->getType() == Texture::Type::Texture2D) ? mpProgram->addDefine("_SPHERICAL_MAP") : mpProgram->removeDefine("_SPHERICAL_MAP");
    }
    mpVars["gTexture"] = mpTexture;
}

void RemoteRenderPass::setFilter(uint32_t filter)
{
    mFilter = (Sampler::Filter)filter;
    Sampler::Desc samplerDesc;
    samplerDesc.setFilterMode(mFilter, mFilter, mFilter);
    mpSampler = Sampler::create(samplerDesc);
    mpVars["gSampler"] = mpSampler;
}
