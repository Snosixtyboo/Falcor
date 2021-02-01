# Graphs
import os
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('RemoteRenderGraph')
    loadRenderPassLibrary('BSDFViewer.dll')
    loadRenderPassLibrary('AccumulatePass.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('FeedbackPass.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('DeferredCapturePassPass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('RemoteRenderPass.dll')
    loadRenderPassLibrary('SceneWritePass.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')
    RemoteRenderPass = createPass('RemoteRenderPass', {'texName': '', 'loadAsSrgb': True, 'filter': SamplerFilter.Linear})
    g.addPass(RemoteRenderPass, 'RemoteRenderPass')
    DepthPass = createPass('DepthPass', {'depthFormat': ResourceFormat.D32Float})
    g.addPass(DepthPass, 'DepthPass')
    ForwardLightingPass = createPass('ForwardLightingPass', {'sampleCount': 1, 'enableSuperSampling': False})
    g.addPass(ForwardLightingPass, 'ForwardLightingPass')
    ToneMapper = createPass('ToneMapper', {'exposureCompensation': 0.0, 'autoExposure': True, 'exposureValue': 0.0, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': ToneMapOp.Aces, 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': ExposureMode.AperturePriority})
    CSM = createPass('CSM', {'mapSize': uint2(2048,2048), 'visibilityBufferSize': uint2(0,0), 'cascadeCount': 4, 'visibilityMapBitsPerChannel': 32, 'kSdsmReadbackLatency': 1, 'blurWidth': 5, 'blurSigma': 2.0})
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


# Get environment variables
BASE_PATH = os.getenv('FALCOR_DATA').strip("/")
m.loadScene(BASE_PATH + '/Bistro_v5_1/BistroInterior.fbx')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.scene.camera.animated = False
m.scene.camera.position = float3(2.858435,1.494224,-0.186009)
m.scene.camera.target = float3(2.867967,1.493495,-0.188943)
m.scene.camera.up = float3(0.000678,0.009973,-0.000276)
m.scene.camera.nearPlane = 0.10000000149011612
m.scene.camera.farPlane = 100.0
m.scene.cameraSpeed = 1.0

# Window Configuration
m.resizeSwapChain(1920, 1080)
m.ui = True

# Time Settings
t.time = 0
t.framerate = 0
# If framerate is not zero, you can use the frame property to set the start frame
# t.frame = 0
t.pause()

# Frame Capture
fc.outputDir = '.'
fc.baseFilename = 'Mogwai'

