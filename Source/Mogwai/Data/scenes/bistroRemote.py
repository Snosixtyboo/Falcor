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
#m.loadScene(os.getenv('FALCOR_DATA').strip("/") + '/Bistro_v5_1/BistroInterior.fbx')
m.loadScene('C:/Users/jaliborc/Documents/Coding/vrs/data/scenes/Bistro/BistroInterior.fbx')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.scene.camera.animated = False
m.scene.camera.position = float3(2.858435,1.494224,-0.186009)
m.scene.camera.target = float3(2.867967,1.493495,-0.188943)
m.scene.camera.up = float3(0.000678,0.009973,-0.000276)
m.scene.camera.nearPlane = 0.10000000149011612
m.scene.camera.farPlane = 100.0
m.scene.cameraSpeed = 1.0

# Window
m.resizeSwapChain(512, 256)
m.ui = True
t.time = 0
t.framerate = 0
t.pause()
fc.outputDir = '.'
fc.baseFilename = 'Mogwai'
