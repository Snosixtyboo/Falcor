# Graphs
from falcor import *
import os

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DeferredCaptureGraph')
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
    loadRenderPassLibrary('SceneWritePass.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')
    DeferredCapturePass = createPass('DeferredCapturePass')
    g.addPass(DeferredCapturePass, 'DeferredCapturePass')
    SSAO = createPass('SSAO')
    g.addPass(SSAO, 'SSAO')
    SkyBox = createPass('SkyBox')
    g.addPass(SkyBox, 'SkyBox')
    FXAA = createPass('FXAA')
    g.addPass(FXAA, 'FXAA')
    GBufferRaster = createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack})
    g.addPass(GBufferRaster, 'GBufferRaster')
    ToneMapper = createPass('ToneMapper', {'autoExposure': True})
    CSM = createPass('CSM')
    g.addPass(CSM, 'CSM')
    g.addPass(ToneMapper, 'ToneMapper')
    g.addEdge('GBufferRaster.posW', 'DeferredCapturePass.posW')
    g.addEdge('GBufferRaster.normW', 'DeferredCapturePass.normW')
    g.addEdge('GBufferRaster.tangentW', 'DeferredCapturePass.tangentW')
    g.addEdge('GBufferRaster.texC', 'DeferredCapturePass.texC')
    g.addEdge('GBufferRaster.diffuseOpacity', 'DeferredCapturePass.diffuseOpacity')
    g.addEdge('GBufferRaster.specRough', 'DeferredCapturePass.specRough')
    g.addEdge('GBufferRaster.emissive', 'DeferredCapturePass.emissive')
    g.addEdge('GBufferRaster.matlExtra', 'DeferredCapturePass.matlExtra')
    g.addEdge('GBufferRaster.depth', 'SSAO.depth')
    g.addEdge('ToneMapper.dst', 'SSAO.colorIn')
    g.addEdge('DeferredCapturePass.normalsOut', 'SSAO.normals')
    g.addEdge('SSAO.colorOut', 'FXAA.src')
    g.addEdge('GBufferRaster.depth', 'DeferredCapturePass.depth')
    g.addEdge('DeferredCapturePass.color', 'ToneMapper.src')
    g.addEdge('GBufferRaster.depth', 'SkyBox.depth')
    g.addEdge('SkyBox.target', 'DeferredCapturePass.color')
    g.addEdge('GBufferRaster.depth', 'CSM.depth')
    g.addEdge('CSM.visibility', 'DeferredCapturePass.visibilityBuffer')
    g.markOutput('FXAA.dst')
    return g
m.addGraph(render_graph_DefaultRenderGraph())

# Scene
BASE_PATH = os.getenv('FALCOR_DATA').strip("/")
m.loadScene(BASE_PATH + '/Bistro_v5_1/BistroInterior.fbx')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
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

# Frame Capture
fc.outputDir = '.'
fc.baseFilename = 'Mogwai'

