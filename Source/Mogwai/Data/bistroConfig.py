# Graphs
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    loadRenderPassLibrary('AccumulatePass.dll')
    loadRenderPassLibrary('BSDFViewer.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('FeedbackPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('ExampleBlitPass.dll')
    loadRenderPassLibrary('ForwardLightingPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Utils.dll')
    DepthPass = createPass('DepthPass', {'depthFormat': ResourceFormat.D32Float})
    g.addPass(DepthPass, 'DepthPass')
    ExampleBlitPass = createPass('ExampleBlitPass')
    g.addPass(ExampleBlitPass, 'ExampleBlitPass')
    SSAO = createPass('SSAO', {'aoMapSize': uint2(1024,1024), 'kernelSize': 16, 'noiseSize': uint2(16,16), 'radius': 0.10000000149011612, 'distribution': SampleDistribution.CosineHammersley, 'blurWidth': 5, 'blurSigma': 2.0})
    g.addPass(SSAO, 'SSAO')
    FXAA = createPass('FXAA', {'qualitySubPix': 0.75, 'qualityEdgeThreshold': 0.16599999368190765, 'qualityEdgeThresholdMin': 0.08330000191926956, 'earlyOut': True})
    g.addPass(FXAA, 'FXAA')
    GBufferRaster = createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack})
    g.addPass(GBufferRaster, 'GBufferRaster')
    ToneMapper = createPass('ToneMapper', {'exposureCompensation': 0.0, 'autoExposure': True, 'exposureValue': 0.0, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': ToneMapOp.Aces, 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': ExposureMode.AperturePriority})
    g.addPass(ToneMapper, 'ToneMapper')
    g.addEdge('GBufferRaster.posW', 'ExampleBlitPass.posW')
    g.addEdge('GBufferRaster.normW', 'ExampleBlitPass.normW')
    g.addEdge('GBufferRaster.tangentW', 'ExampleBlitPass.tangentW')
    g.addEdge('GBufferRaster.texC', 'ExampleBlitPass.texC')
    g.addEdge('GBufferRaster.diffuseOpacity', 'ExampleBlitPass.diffuseOpacity')
    g.addEdge('GBufferRaster.specRough', 'ExampleBlitPass.specRough')
    g.addEdge('GBufferRaster.emissive', 'ExampleBlitPass.emissive')
    g.addEdge('GBufferRaster.matlExtra', 'ExampleBlitPass.matlExtra')
    g.addEdge('ExampleBlitPass.output', 'ToneMapper.src')
    g.addEdge('ToneMapper.dst', 'SSAO.colorIn')
    g.addEdge('ExampleBlitPass.normalsOut', 'SSAO.normals')
    g.addEdge('DepthPass.depth', 'ExampleBlitPass.depth')
    g.addEdge('DepthPass.depth', 'SSAO.depth')
    g.addEdge('SSAO.colorOut', 'FXAA.src')
    g.markOutput('FXAA.dst')
    return g
m.addGraph(render_graph_DefaultRenderGraph())

# Scene
m.loadScene('../../../Scenes/Bistro_v5_1/BistroInterior.fbx')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
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
