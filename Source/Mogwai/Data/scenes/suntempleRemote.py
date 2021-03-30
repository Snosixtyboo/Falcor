from falcor import *
import os

# Pipeline
def render_graph_DefaultRenderGraph():
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('RemoteRenderPass.dll')
    loadRenderPassLibrary('ToneMapper.dll')

    g = RenderGraph('RemoteRenderGraph')
    g.addPass(createPass('RemoteRenderPass'), 'RemoteRenderPass')
    g.addPass(createPass('DepthPass', {'depthFormat': ResourceFormat.D32Float}), 'DepthPass')
    g.addPass(createPass('ForwardLightingPass', {'sampleCount': 1, 'enableSuperSampling': False}), 'ForwardLightingPass')
    g.addPass(createPass('CSM'), 'CSM')
    g.addPass(createPass('ToneMapper', {'autoExposure': True}), 'ToneMapper')

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
m.loadScene(os.getenv('FALCOR_DATA').strip('/') + '/SunTemple_v3/SunTemple/SunTemple2.fscene')
#m.loadScene('C:/Users/jaliborc/Documents/Coding/vrs/data/scenes/SunTemple/SunTemple.fscene')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.scene.camera.position = float3(0.017157,3.184416,71.125000)
m.scene.camera.target = float3(0.008678,3.202756,70.125206)
m.scene.camera.up = float3(0.002032,0.999836,0.018367)
m.scene.camera.nearPlane = 0.10000000149011612
m.scene.camera.farPlane = 10000.0
m.scene.cameraSpeed = 1.0

# Window
m.resizeSwapChain(512, 256)
m.ui = False
t.time = 0
t.framerate = 0
