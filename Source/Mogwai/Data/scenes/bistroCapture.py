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
    loadRenderPassLibrary('DeferredCapturePassPass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('SceneWritePass.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')
    DeferredCapturePassPass = createPass('DeferredCapturePassPass')
    g.addPass(DeferredCapturePassPass, 'DeferredCapturePassPass')
    SSAO = createPass('SSAO', {'aoMapSize': uint2(1024,1024), 'kernelSize': 16, 'noiseSize': uint2(16,16), 'radius': 0.10000000149011612, 'distribution': SampleDistribution.CosineHammersley, 'blurWidth': 5, 'blurSigma': 2.0})
    g.addPass(SSAO, 'SSAO')
    SkyBox = createPass('SkyBox', {'texName': '', 'loadAsSrgb': True, 'filter': SamplerFilter.Linear})
    g.addPass(SkyBox, 'SkyBox')
    FXAA = createPass('FXAA', {'qualitySubPix': 0.75, 'qualityEdgeThreshold': 0.16599999368190765, 'qualityEdgeThresholdMin': 0.08330000191926956, 'earlyOut': True})
    g.addPass(FXAA, 'FXAA')
    GBufferRaster = createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack})
    g.addPass(GBufferRaster, 'GBufferRaster')
    ToneMapper = createPass('ToneMapper', {'exposureCompensation': 0.0, 'autoExposure': True, 'exposureValue': 0.0, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': ToneMapOp.Aces, 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': ExposureMode.AperturePriority})
    CSM = createPass('CSM', {'mapSize': uint2(2048,2048), 'visibilityBufferSize': uint2(0,0), 'cascadeCount': 4, 'visibilityMapBitsPerChannel': 32, 'kSdsmReadbackLatency': 1, 'blurWidth': 5, 'blurSigma': 2.0})
    g.addPass(CSM, 'CSM')
    g.addPass(ToneMapper, 'ToneMapper')
    g.addEdge('GBufferRaster.posW', 'DeferredCapturePassPass.posW')
    g.addEdge('GBufferRaster.normW', 'DeferredCapturePassPass.normW')
    g.addEdge('GBufferRaster.tangentW', 'DeferredCapturePassPass.tangentW')
    g.addEdge('GBufferRaster.texC', 'DeferredCapturePassPass.texC')
    g.addEdge('GBufferRaster.diffuseOpacity', 'DeferredCapturePassPass.diffuseOpacity')
    g.addEdge('GBufferRaster.specRough', 'DeferredCapturePassPass.specRough')
    g.addEdge('GBufferRaster.emissive', 'DeferredCapturePassPass.emissive')
    g.addEdge('GBufferRaster.matlExtra', 'DeferredCapturePassPass.matlExtra')
    g.addEdge('GBufferRaster.depth', 'SSAO.depth')
    g.addEdge('ToneMapper.dst', 'SSAO.colorIn')
    g.addEdge('DeferredCapturePassPass.normalsOut', 'SSAO.normals')
    g.addEdge('SSAO.colorOut', 'FXAA.src')
    g.addEdge('GBufferRaster.depth', 'DeferredCapturePassPass.depth')
    g.addEdge('DeferredCapturePassPass.color', 'ToneMapper.src')
    g.addEdge('GBufferRaster.depth', 'SkyBox.depth')
    g.addEdge('SkyBox.target', 'DeferredCapturePassPass.color')
    g.addEdge('GBufferRaster.depth', 'CSM.depth')
    g.addEdge('CSM.visibility', 'DeferredCapturePassPass.visibilityBuffer')
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

