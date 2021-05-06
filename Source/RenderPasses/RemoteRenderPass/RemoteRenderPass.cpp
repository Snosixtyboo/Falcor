#include "RemoteRenderPass.h"
#include "glm/gtx/transform.hpp"

const char* RemoteRenderPass::desc = "Renders an environment map provided by the user or taken from a scene";
extern "C" __declspec(dllexport) const char* getProjDir() { return PROJECT_DIR; }
extern "C" __declspec(dllexport) void getPasses(Falcor::RenderPassLibrary & lib)
{
    lib.registerClass("RemoteRenderPass", RemoteRenderPass::desc, RemoteRenderPass::create);
    ScriptBindings::registerBinding([](pybind11::module& m) {
        pybind11::class_<RemoteRenderPass, RenderPass, RemoteRenderPass::SharedPtr> pass(m, "RemoteRenderPass");
        pass.def_property("filter", &RemoteRenderPass::getFilter, &RemoteRenderPass::setFilter);
        pass.def_property("scale", &RemoteRenderPass::getScale, &RemoteRenderPass::setScale);
    });
}

const Gui::DropdownList kFilterList =
{
    { (uint32_t)Sampler::Filter::Linear, "Linear" },
    { (uint32_t)Sampler::Filter::Point, "Point" },
};

const std::string kTarget = "target";
const std::string kDepth = "depth";

const std::string kTexName = "texName";
const std::string kLoadAsSrgb = "loadAsSrgb";
const std::string kFilter = "filter";

WSOrg::WSOrg()
{
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
    sockaddr.sin_port = htons(port); // htons is necessary to convert a number to
                                     // network byte order
    memset(sockaddr.sin_zero, '\0', sizeof(sockaddr.sin_zero));

    if (bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        std::cout << "Failed to bind to port " << port << ". errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    int yes;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(int)) < 0)
    {
        std::cout << "Failed to set sockopt. errno: " << errno << std::endl;
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

WSOrg::~WSOrg()
{
    WSACleanup();
}

RemoteRenderPass::RemoteRenderPass()
{
    cubeScene = Scene::create("cube.obj");
    if (cubeScene == nullptr) throw std::runtime_error("RemoteRenderPass::RemoteRenderPass - Failed to load cube model");

    program = GraphicsProgram::createFromFile("RenderPasses/Skybox/Skybox.slang", "vs", "ps");
    program->addDefines(cubeScene->getSceneDefines());
    vars = GraphicsVars::create(program->getReflector());
    framebuffer = Fbo::create();

    // Create state
    state = GraphicsState::create();
    BlendState::Desc blendDesc;
    for (uint32_t i = 1; i < Fbo::getMaxColorTargetCount(); i++) blendDesc.setRenderTargetWriteMask(i, false, false, false, false);
    blendDesc.setIndependentBlend(true);
    state->setBlendState(BlendState::create(blendDesc));

    // Create the rasterizer state
    RasterizerState::Desc rastDesc;
    rastDesc.setCullMode(RasterizerState::CullMode::Front).setDepthClamp(true);
    state->setRasterizerState(RasterizerState::create(rastDesc));

    DepthStencilState::Desc dsDesc;
    dsDesc.setDepthWriteMask(false).setDepthFunc(DepthStencilState::Func::LessEqual);
    state->setDepthStencilState(DepthStencilState::create(dsDesc));
    state->setProgram(program);

    setFilter((uint32_t)filter);

    occlusionHeap = QueryHeap::create(QueryHeap::Type::Occlusion, 1);
    occlusionBuffer = Buffer::create(8, Falcor::ResourceBindFlags::None, Falcor::Buffer::CpuAccess::Read);
    copyFence = GpuFence::create();
}

RemoteRenderPass::SharedPtr RemoteRenderPass::create(RenderContext* context, const Dictionary& dict)
{
    SharedPtr pass = SharedPtr(new RemoteRenderPass());

    for (const auto& [key, value] : dict) {
        if (key == kTexName) pass->textureName = value.operator std::string();
        else if (key == kLoadAsSrgb) pass->srgb = value;
        else if (key == kFilter) pass->setFilter(value);
        else logWarning("Unknown field '" + key + "' in a RemoteRenderPass dictionary");
    }

    std::shared_ptr<Texture> pTexture;
    if (pass->textureName.size() != 0) {
        pTexture = Texture::createFromFile(pass->textureName, false, pass->srgb);
        if (pTexture == nullptr) throw std::runtime_error("RemoteRenderPass::create - Error creating texture from file");
        pass->setTexture(pTexture);
    }

    return pass;
}

Dictionary RemoteRenderPass::getScriptingDictionary()
{
    Dictionary dict;
    dict[kTexName] = textureName;
    dict[kLoadAsSrgb] = srgb;
    dict[kFilter] = filter;
    return dict;
}

RenderPassReflection RemoteRenderPass::reflect(const CompileData& data)
{
    RenderPassReflection reflector;
    reflector.addOutput(kTarget, "Color buffer").format(ResourceFormat::RGBA32Float);
    reflector.addInputOutput(kDepth, "Depth buffer, which should be pre-initialized or cleared before calling the pass").bindFlags(Resource::BindFlags::DepthStencil);
    return reflector;
}

void RemoteRenderPass::execute(RenderContext* context, const RenderData& data)
{
    framebuffer->attachColorTarget(data[kTarget]->asTexture(), 0);
    framebuffer->attachDepthStencilTarget(data[kDepth]->asTexture());
    context->clearRtv(framebuffer->getRenderTargetView(0).get(), float4(0));

    if (!scene) return;

    glm::mat4 world = glm::translate(scene->getCamera()->getPosition());
    vars["PerFrameCB"]["gWorld"] = world;
    vars["PerFrameCB"]["gScale"] = scale;
    vars["PerFrameCB"]["gViewMat"] = scene->getCamera()->getViewMatrix();
    vars["PerFrameCB"]["gProjMat"] = scene->getCamera()->getProjMatrix();
    state->setFbo(framebuffer);

    context->getLowLevelData()->getCommandList()->BeginQuery(occlusionHeap->getApiHandle(), D3D12_QUERY_TYPE_OCCLUSION, 0);
    cubeScene->rasterize(context, state.get(), vars.get(), Scene::RenderFlags::UserRasterizerState);
    context->getLowLevelData()->getCommandList()->EndQuery(occlusionHeap->getApiHandle(), D3D12_QUERY_TYPE_OCCLUSION, 0);

    // Resolve the occlusion query and store the results in the query result buffer
    // to be used on the subsequent frame.
    context->resourceBarrier(occlusionBuffer.get(), Resource::State::CopyDest);
    context->getLowLevelData()->getCommandList()->ResolveQueryData(occlusionHeap->getApiHandle(), D3D12_QUERY_TYPE_OCCLUSION, 0, 1, occlusionBuffer->getApiHandle(), 0);
    context->resourceBarrier(occlusionBuffer.get(), Resource::State::CopySource);

    // Wait for GPU to finish writing and transitioning
    context->flush(false);
    copyFence->gpuSignal(context->getLowLevelData()->getCommandQueue());
    copyFence->syncCpu();

    float sum = (float)(data[kTarget]->asTexture()->getHeight() * data[kTarget]->asTexture()->getWidth());
    unsigned long long int* queryRes = (unsigned long long int*)occlusionBuffer->map(Falcor::Buffer::MapType::Read);
    float foreground = 1.0f - ((*queryRes) / sum);
    occlusionBuffer->unmap();

    if (capturing) {
        passed[donePoints] = (foreground >= threshold) ? 1 : 0;
        donePoints++;

        if (donePoints == viewPointsToDo) {
            int sent = 0;
            while (sent < (int)passed.size())
                sent += send(wsorg.connection, passed.data() + sent, (int)passed.size() - sent, 0);

            capturing = false;
        }
    }
    else  {
        struct {
            int numCams;
            float threshold;
        } info;

        if (wsorg.connection) {
            int res = recv(wsorg.connection, (char*)&info, sizeof(info), MSG_PEEK);
            if (res == 0) { // closed
                closesocket(wsorg.connection);
                std::cout << "Closing " << wsorg.connection << std::endl;
                wsorg.connection = 0;
            } else if (res < 0) {
                return;
            }
        }

        // Grab a connection from the queue
        auto addrlen = sizeof(sockaddr);
        SOCKET newConn = accept(wsorg.sockfd, (struct sockaddr*)&wsorg.sockaddr, (int*)&addrlen);
        if (newConn != INVALID_SOCKET) {
            wsorg.connection = newConn;
            std::cout << "Accepting " << wsorg.connection << std::endl;
        }

        if (wsorg.connection == 0)
            return;

        while (recv(wsorg.connection, (char*)&info, sizeof(info), MSG_PEEK) < (int)sizeof(info));
        recv(wsorg.connection, (char*)&info, sizeof(info), 0);

        std::vector<glm::mat4> matrices(info.numCams);
        while (recv(wsorg.connection, (char*)matrices.data(), sizeof(glm::mat4) * info.numCams, MSG_PEEK) != sizeof(glm::mat4) * info.numCams);
        recv(wsorg.connection, (char*)matrices.data(), sizeof(glm::mat4) * info.numCams, 0);

        if (info.numCams == 0)
            return;

        // Convert blender coordinate system to Falcor
        for (int i = 0; i < matrices.size(); i++) {
            glm::mat4& mat = matrices[i];
            mat = transpose(mat);

            for (int j = 0; j < 4; j++) {
                float v = mat[j][1];
                float w = mat[j][2];
                mat[j][2] = -v;
                mat[j][1] = w;
            }

            mat[2] = -mat[2];
        }

        donePoints = 0;
        threshold = info.threshold;
        viewPointsToDo = info.numCams;
        passed.resize(info.numCams);

        std::queue<glm::mat4> configs;
        for (auto m : matrices)
            configs.push(m);

        scene->getCamera()->queueConfigs(configs);
        capturing = true;
    }
}

void RemoteRenderPass::setScene(RenderContext* context, const Scene::SharedPtr& scene)
{
    this->scene = scene;

    if (scene) {
        cubeScene->setCamera(scene->getCamera());
        if (scene->getEnvMap()) setTexture(scene->getEnvMap()->getEnvMap());
    }
}

void RemoteRenderPass::renderUI(Gui::Widgets& widget)
{
    widget.var("Scale", scale, 0.f);
    if (widget.button("Load Image"))
      loadImage();

    uint32_t f = (uint32_t) filter;
    if (widget.dropdown("Filter", kFilterList, f))
      setFilter(f);
}

void RemoteRenderPass::loadImage()
{
    std::string filename;
    FileDialogFilterVec filters = { {"bmp"}, {"jpg"}, {"dds"}, {"png"}, {"tiff"}, {"tif"}, {"tga"}, {"hdr"} };

    if (openFileDialog(filters, filename))
        setTexture(Texture::createFromFile(filename, false, srgb));
}

void RemoteRenderPass::setTexture(const Texture::SharedPtr& texture)
{
    if (texture) {
        assert(texture->getType() == Texture::Type::TextureCube || texture->getType() == Texture::Type::Texture2D);
        (texture->getType() == Texture::Type::Texture2D) ? program->addDefine("_SPHERICAL_MAP") : program->removeDefine("_SPHERICAL_MAP");
    }

    this->texture = texture;
    vars["gTexture"] = texture;
}

void RemoteRenderPass::setFilter(uint32_t id)
{
    filter = (Sampler::Filter) id;
    vars["gSampler"] = Sampler::create(Sampler::Desc().setFilterMode(filter, filter, filter));
}
