# Graphs
from falcor import *
import os

# Scene
#BASE_PATH = os.getenv('FALCOR_DATA').strip("/")
#m.loadScene(BASE_PATH + '/SunTemple_v3/SunTemple/SunTemple2.fscene')
m.loadScene('C:/Users/jaliborc/Documents/Coding/vrs/data/scenes/SunTemple/SunTemple.fscene')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.scene.camera.position = float3(0.017157,3.184416,71.125000)
m.scene.camera.target = float3(-0.021863,3.224318,70.126556)
m.scene.camera.up = float3(-0.000038,1.000000,-0.000924)
m.scene.camera.nearPlane = 0.10000000149011612
m.scene.camera.farPlane = 10000.0
m.scene.cameraSpeed = 1.0
m.scene.getLight(0).active = True
m.scene.getLight(1).active = True
m.scene.getLight(2).active = True
m.scene.getLight(3).active = True
m.scene.getLight(4).active = True
m.scene.getLight(5).active = True
m.scene.getLight(6).active = True
m.scene.getLight(7).active = True
m.scene.getLight(8).active = True
m.scene.getLight(9).active = True
m.scene.getLight(10).active = True
m.scene.getLight(11).active = True
m.scene.getLight(12).active = True
m.scene.getLight(13).active = True

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

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DeferredCaptureGraph')
    loadRenderPassLibrary('BSDFViewer.dll')
    loadRenderPassLibrary('AccumulatePass.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('FeedbackPass.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('DeferredLightingPass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('SceneWritePass.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')

    g.addPass(createPass('DeferredLightingPass'), 'DeferredLighting')
    g.addPass(createPass('SSAO'), 'SSAO')
    g.addPass(createPass('SkyBox'), 'SkyBox')
    g.addPass(createPass('FXAA'), 'FXAA')
    g.addPass(createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack}), 'GBufferRaster')
    g.addPass(createPass('CSM'), 'CSM')
    g.addPass(createPass('ToneMapper', {'autoExposure': True}), 'ToneMapper')

    g.addEdge('GBufferRaster.texC', 'DeferredLighting.texC')
    g.addEdge('GBufferRaster.posW', 'DeferredLighting.posW')
    g.addEdge('GBufferRaster.normW', 'DeferredLighting.normW')
    g.addEdge('GBufferRaster.tangentW', 'DeferredLighting.tangentW')
    g.addEdge('GBufferRaster.diffuseOpacity', 'DeferredLighting.diffuseOpacity')
    g.addEdge('GBufferRaster.specRough', 'DeferredLighting.specRough')
    g.addEdge('GBufferRaster.emissive', 'DeferredLighting.emissive')
    g.addEdge('GBufferRaster.matlExtra', 'DeferredLighting.matlExtra')
    g.addEdge('GBufferRaster.depth', 'DeferredLighting.depth')
    g.addEdge('GBufferRaster.depth', 'SkyBox.depth')
    g.addEdge('GBufferRaster.depth', 'SSAO.depth')
    g.addEdge('GBufferRaster.depth', 'CSM.depth')

    g.addEdge('SkyBox.target', 'DeferredLighting.color')
    g.addEdge('CSM.visibility', 'DeferredLighting.visibility')

    g.addEdge('DeferredLighting.normalsOut', 'SSAO.normals')
    g.addEdge('DeferredLighting.color', 'ToneMapper.src')
    g.addEdge('ToneMapper.dst', 'SSAO.colorIn')
    g.addEdge('SSAO.colorOut', 'FXAA.src')
    g.markOutput('FXAA.dst')

    return g

m.addGraph(render_graph_DefaultRenderGraph())
