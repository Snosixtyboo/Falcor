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
    loadRenderPassLibrary('DeferredMultiresPass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('SceneWritePass.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')

    g.addPass(createPass('CSM'), 'CSM')
    g.addPass(createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack}), 'GBufferRaster')
    g.addPass(createPass('DeferredMultiresPass'), 'DeferredMultires')

    g.addEdge('GBufferRaster.depth', 'CSM.depth')
    g.addEdge('CSM.visibility', 'DeferredMultires.visibility')

    g.addEdge('GBufferRaster.posW', 'DeferredMultires.posW')
    g.addEdge('GBufferRaster.normW', 'DeferredMultires.normW')
    g.addEdge('GBufferRaster.diffuseOpacity', 'DeferredMultires.diffuseOpacity')
    g.addEdge('GBufferRaster.specRough', 'DeferredMultires.specRough')
    g.addEdge('GBufferRaster.emissive', 'DeferredMultires.emissive')
    g.addEdge('GBufferRaster.depth', 'DeferredMultires.depth')

    tonemap = createPass('ToneMapper', {'autoExposure': True})
    skybox = createPass('SkyBox')
    ssao = createPass('SSAO')
    fxaa = createPass('FXAA')

    for x in ['1x1', '2x2', '4x4']:
        g.addPass(skybox, f'SkyBox{x}')
        g.addPass(ssao, f'SSAO{x}')
        g.addPass(tonemap, f'ToneMapper{x}')
        g.addPass(fxaa, f'FXAA{x}')

        g.addEdge('GBufferRaster.depth', f'SSAO{x}.depth')
        g.addEdge('GBufferRaster.depth', f'SkyBox{x}.depth')
        g.addEdge(f'SkyBox{x}.target', f'DeferredMultires.color{x}')

        g.addEdge('DeferredMultires.normalsOut', f'SSAO{x}.normals')
        g.addEdge(f'DeferredMultires.color{x}', f'ToneMapper{x}.src')
        g.addEdge(f'ToneMapper{x}.dst', f'SSAO{x}.colorIn')
        g.addEdge(f'SSAO{x}.colorOut', f'FXAA{x}.src')
        g.markOutput(f'FXAA{x}.dst')

    return g

m.addGraph(render_graph_DefaultRenderGraph())
