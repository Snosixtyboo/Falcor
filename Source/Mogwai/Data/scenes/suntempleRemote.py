# Graphs
from falcor import *
import os

def render_graph_DefaultRenderGraph():
    g = RenderGraph('RemoteRenderGraph')
    loadRenderPassLibrary('AccumulatePass.dll')
    loadRenderPassLibrary('BSDFViewer.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('FeedbackPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('DeferredCapturePass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('RemoteRenderPass.dll')
    loadRenderPassLibrary('SceneWritePass.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')
    RemoteRenderPass = createPass('RemoteRenderPass')
    g.addPass(RemoteRenderPass, 'RemoteRenderPass')
    DepthPass = createPass('DepthPass', {'depthFormat': ResourceFormat.D32Float})
    g.addPass(DepthPass, 'DepthPass')
    ForwardLightingPass = createPass('ForwardLightingPass', {'sampleCount': 1, 'enableSuperSampling': False})
    g.addPass(ForwardLightingPass, 'ForwardLightingPass')
    ToneMapper = createPass('ToneMapper', {'autoExposure': True})
    CSM = createPass('CSM')
    g.addPass(CSM, 'CSM')
    g.addPass(ToneMapper, 'ToneMapper')
    g.addEdge('DepthPass.depth', 'ForwardLightingPass.depth')
    g.addEdge('DepthPass.depth', 'RemoteRenderPass.depth')
    g.addEdge('RemoteRenderPass.target', 'ForwardLightingPass.color')
    g.addEdge('ForwardLightingPass.color', 'ToneMapper.src')
    g.addEdge('DepthPass.depth', 'CSM.depth')
    g.addEdge('CSM.visibility', 'ForwardLightingPass.visibilityBuffer')
    g.markOutput('ToneMapper.dst')
    return g
m.addGraph(render_graph_DefaultRenderGraph())

# Scene
BASE_PATH = os.getenv('FALCOR_DATA').strip("/")
m.loadScene(BASE_PATH + '/SunTemple_v3/SunTemple/SunTemple2.fscene')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.scene.camera.position = float3(0.017157,3.184416,71.125000)
m.scene.camera.target = float3(0.008678,3.202756,70.125206)
m.scene.camera.up = float3(0.002032,0.999836,0.018367)
m.scene.camera.nearPlane = 0.10000000149011612
m.scene.camera.farPlane = 10000.0
m.scene.cameraSpeed = 1.0

# Window Configuration
m.resizeSwapChain(1920, 1080)
m.ui = True

# Time Settings
t.time = 0
t.framerate = 0
# If framerate is not zero, you can use the frame property to set the start frame
# t.frame = 0

# Frame Capture
fc.outputDir = '.'
fc.baseFilename = 'Mogwai'

