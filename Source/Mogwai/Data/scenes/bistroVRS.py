from falcor import *
import os

# Scene
#m.loadScene(os.getenv('FALCOR_DATA').strip("/") + '/Bistro_v5_1/BistroInterior.fbx')
m.loadScene('C:/Users/jaliborc/Documents/Coding/vrs/data/scenes/Bistro/BistroInterior.fbx')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.scene.camera.nearPlane = 0.10000000149011612
m.scene.camera.farPlane = 100.0
m.scene.cameraSpeed = 1.0

# Window Configuration
m.resizeSwapChain(3840, 2160)
m.ui = True
t.time = 0
t.framerate = 0
fc.outputDir = '.'
fc.baseFilename = 'Mogwai'

# Pipeline
def render_graph_DefaultRenderGraph():
    loadRenderPassLibrary('AdaptiveVRS.dll')
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('DeferredPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Reproject.dll')

    g = RenderGraph('AdaptiveVRSGraph')
    g.addPass(createPass('BlitPass'), 'CSMBlit')
    g.addPass(createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack}), 'Raster')
    g.addPass(createPass('ToneMapper', {'autoExposure': True}), 'ToneMapper')
    g.addPass(createPass('DeferredPass'), 'Shading')
    g.addPass(createPass('Reproject'), 'Reproject')
    g.addPass(createPass('VRSDebug'), 'VRSDebug')
    g.addPass(createPass('YangVRS'), 'YangVRS')
    g.addPass(createPass('SkyBox'), 'SkyBox')
    g.addPass(createPass('FXAA'), 'FXAA')
    g.addPass(createPass('SSAO'), 'SSAO')
    g.addPass(createPass('CSM'), 'CSM')

    # Raster
    g.addEdge('Raster.depth', 'CSM.depth')
    g.addEdge('Raster.posW', 'Shading.posW')
    g.addEdge('Raster.normW', 'Shading.normW')
    g.addEdge('Raster.diffuseOpacity', 'Shading.diffuseOpacity')
    g.addEdge('Raster.specRough', 'Shading.specRough')
    g.addEdge('Raster.emissive', 'Shading.emissive')
    g.addEdge('Raster.depth', 'Shading.depth')
    g.addEdge('Raster.depth', 'SSAO.depth')
    g.addEdge('Raster.depth', 'SkyBox.depth')
    g.addEdge('Raster.normW', 'SSAO.normals')
    g.addEdge('Raster.mvec', 'Reproject.motion')

    # Features
    g.addEdge('CSM.visibility', 'Shading.visibility')
    g.addEdge('CSM.visibility', 'CSMBlit.src')
    g.addEdge('Reproject.output', 'YangVRS.input')
    g.addEdge('YangVRS.rate', 'Shading.vrs')

    # Post-Processing
    g.addEdge('SkyBox.target', 'Shading.output')
    g.addEdge('Shading.output', 'ToneMapper.src')
    g.addEdge('ToneMapper.dst', 'SSAO.colorIn')
    g.addEdge('SSAO.colorOut', 'FXAA.src')
    g.addEdge('Reproject.buffer', 'FXAA.dst')
    g.markOutput('FXAA.dst')

    # Debug
    g.addEdge('YangVRS.rate', 'VRSDebug.rate')
    g.addEdge('FXAA.dst', 'VRSDebug.rendering')
    g.markOutput('VRSDebug.color')
    return g

m.addGraph(render_graph_DefaultRenderGraph())
