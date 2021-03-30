from falcor import *
import os

# Scene
m.loadScene(os.getenv('FALCOR_DATA').strip('/') + '/SunTemple_v3/SunTemple/SunTemple2.fscene')
#m.loadScene('C:/Users/jaliborc/Documents/Coding/vrs/data/scenes/SunTemple/SunTemple.fscene')
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
t.time = 0
t.framerate = 0
fc.outputDir = '.'
fc.baseFilename = 'Mogwai'

# Pipeline
def render_graph_DefaultRenderGraph():
    loadRenderPassLibrary('Antialiasing.dll')
    loadRenderPassLibrary('BlitPass.dll')
    loadRenderPassLibrary('CSM.dll')
    loadRenderPassLibrary('CapturePass.dll')
    loadRenderPassLibrary('DepthPass.dll')
    loadRenderPassLibrary('DeferredMultiresPass.dll')
    loadRenderPassLibrary('GBuffer.dll')
    loadRenderPassLibrary('SSAO.dll')
    loadRenderPassLibrary('SkyBox.dll')
    loadRenderPassLibrary('ToneMapper.dll')
    loadRenderPassLibrary('Reproject.dll')

    g = RenderGraph('DeferredCaptureGraph')
    g.addPass(createPass('CSM'), 'CSM')
    g.addPass(createPass('BlitPass'), 'CSMBlit')
    g.addPass(createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack}), 'Raster')
    g.addPass(createPass('DeferredMultiresPass'), 'Shading')
    g.addPass(createPass('Reproject'), 'Reproject')
    g.addPass(createPass('CapturePass'), 'Capture')

    g.addEdge('Raster.depth', 'CSM.depth')
    g.addEdge('CSM.visibility', 'Shading.visibility')
    g.addEdge('CSM.visibility', 'CSMBlit.src')

    g.addEdge('Raster.posW', 'Shading.posW')
    g.addEdge('Raster.normW', 'Shading.normW')
    g.addEdge('Raster.diffuseOpacity', 'Shading.diffuseOpacity')
    g.addEdge('Raster.specRough', 'Shading.specRough')
    g.addEdge('Raster.emissive', 'Shading.emissive')
    g.addEdge('Raster.depth', 'Shading.depth')

    tonemap = createPass('ToneMapper', {'autoExposure': True})
    skybox = createPass('SkyBox')
    ssao = createPass('SSAO')
    fxaa = createPass('FXAA')

    for x in ['1x1', '1x2', '2x1', '2x2', '2x4', '4x2', '4x4']:
        g.addPass(skybox, f'SkyBox{x}')
        g.addPass(ssao, f'SSAO{x}')
        g.addPass(tonemap, f'ToneMapper{x}')
        g.addPass(fxaa, f'FXAA{x}')

        g.addEdge('Raster.depth', f'SSAO{x}.depth')
        g.addEdge('Raster.depth', f'SkyBox{x}.depth')
        g.addEdge(f'SkyBox{x}.target', f'Shading.color{x}')

        g.addEdge('Shading.normalsOut', f'SSAO{x}.normals')
        g.addEdge(f'Shading.color{x}', f'ToneMapper{x}.src')
        g.addEdge(f'ToneMapper{x}.dst', f'SSAO{x}.colorIn')
        g.addEdge(f'SSAO{x}.colorOut', f'FXAA{x}.src')

        g.addEdge(f'FXAA{x}.dst', f'Capture.color{x}')
        g.markOutput(f'Capture.color{x}')

    g.addEdge('Shading.motionOut', 'Reproject.motion')
    g.addEdge('FXAA1x1.dst', 'Reproject.input')

    g.addEdge('Reproject.output', 'Capture.reproject')
    g.addEdge('Raster.diffuseOpacity', 'Capture.diffuse')
    g.addEdge('Raster.specRough', 'Capture.specular')
    g.addEdge('Raster.emissive', 'Capture.emissive')
    g.addEdge('Shading.viewNormalsOut', 'Capture.normals')
    g.addEdge('CSMBlit.dst', 'Capture.extras')

    g.markOutput('Capture.reproject')
    g.markOutput('Capture.diffuse')
    g.markOutput('Capture.specular')
    g.markOutput('Capture.emissive')
    g.markOutput('Capture.normals')
    g.markOutput('Capture.extras')
    return g

m.addGraph(render_graph_DefaultRenderGraph())
