from falcor import *

loadRenderPassLibrary('AdaptiveVRS.dll')
loadRenderPassLibrary('Antialiasing.dll')
loadRenderPassLibrary('BlitPass.dll')
loadRenderPassLibrary('CSM.dll')
loadRenderPassLibrary('CapturePass.dll')
loadRenderPassLibrary('DepthPass.dll')
loadRenderPassLibrary('DeferredMultiresPass.dll')
loadRenderPassLibrary('DeferredPass.dll')
loadRenderPassLibrary('FakeInput.dll')
loadRenderPassLibrary('ForwardLightingPass.dll')
loadRenderPassLibrary('GBuffer.dll')
loadRenderPassLibrary('ImageLoader.dll')
loadRenderPassLibrary('SSAO.dll')
loadRenderPassLibrary('SkyBox.dll')
#loadRenderPassLibrary('RemoteRenderPass.dll')
loadRenderPassLibrary('ToneMapper.dll')
loadRenderPassLibrary('TemporalReproject.dll')

def remoteGraph():
    g = RenderGraph('RemoteRender')
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

def captureGraph():
    g = RenderGraph('DeferredCapture')
    g.addPass(createPass('CSM'), 'CSM')
    g.addPass(createPass('BlitPass'), 'CSMBlit')
    g.addPass(createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack}), 'Raster')
    g.addPass(createPass('DeferredMultiresPass'), 'Shading')
    g.addPass(createPass('TemporalReproject'), 'Reproject')
    g.addPass(createPass('CapturePass'), 'Capture')

    # Raster
    g.addEdge('Raster.depth', 'CSM.depth')
    g.addEdge('Raster.mvec', 'Reproject.motion')

    g.addEdge('CSM.visibility', 'CSMBlit.src')
    g.addEdge('CSM.visibility', 'Shading.visibility')

    g.addEdge('Raster.posW', 'Shading.posW')
    g.addEdge('Raster.normW', 'Shading.normW')
    g.addEdge('Raster.diffuseOpacity', 'Shading.diffuseOpacity')
    g.addEdge('Raster.specRough', 'Shading.specRough')
    g.addEdge('Raster.emissive', 'Shading.emissive')
    g.addEdge('Raster.depth', 'Shading.depth')

    # Shading Rates
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
        g.addEdge('Raster.normW', f'SSAO{x}.normals')

        g.addEdge(f'SkyBox{x}.target', f'Shading.color{x}')
        g.addEdge(f'Shading.color{x}', f'ToneMapper{x}.src')
        g.addEdge(f'ToneMapper{x}.dst', f'SSAO{x}.colorIn')
        g.addEdge(f'SSAO{x}.colorOut', f'FXAA{x}.src')

        g.addEdge(f'FXAA{x}.dst', f'Capture.color{x}')
        g.markOutput(f'Capture.color{x}')

        if x == '1x1':
            g.addEdge('Reproject.target', f'FXAA{x}.dst')
        else:
            g.addPass(createPass('FakeInput'), f'Fake{x}')
            g.addEdge(f'Fake{x}.dst', f'FXAA{x}.dst')

    # Capture
    g.addEdge('Raster.diffuseOpacity', 'Capture.diffuse')
    g.addEdge('Raster.specRough', 'Capture.specular')
    g.addEdge('Raster.emissive', 'Capture.emissive')
    g.addEdge('Reproject.dst', 'Capture.reproject')
    g.addEdge('Shading.viewNormalsOut', 'Capture.normals')
    g.addEdge('CSMBlit.dst', 'Capture.extras')

    g.markOutput('Capture.reproject')
    g.markOutput('Capture.diffuse')
    g.markOutput('Capture.specular')
    g.markOutput('Capture.emissive')
    g.markOutput('Capture.normals')
    g.markOutput('Capture.extras')
    return g

def vrsGraph():
    g = RenderGraph('AdaptiveVRS')
    g.addPass(createPass('BlitPass'), 'CSMBlit')
    g.addPass(createPass('GBufferRaster', {'samplePattern': SamplePattern.Center, 'sampleCount': 16, 'disableAlphaTest': False, 'forceCullMode': False, 'cull': CullMode.CullBack}), 'Raster')
    g.addPass(createPass('ToneMapper', {'autoExposure': True}), 'ToneMapper')
    g.addPass(createPass('TemporalReproject'), 'Reproject')
    g.addPass(createPass('DeferredPass'), 'Shading')
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
    g.addEdge('Reproject.dst', 'YangVRS.input')
    g.addEdge('YangVRS.rate', 'Shading.vrs')

    # Post-Processing
    g.addEdge('SkyBox.target', 'Shading.output')
    g.addEdge('Shading.output', 'ToneMapper.src')
    g.addEdge('ToneMapper.dst', 'SSAO.colorIn')
    g.addEdge('SSAO.colorOut', 'FXAA.src')
    g.addEdge('Reproject.target', 'FXAA.dst')
    g.markOutput('FXAA.dst')

    # Debug
    g.addEdge('YangVRS.rate', 'VRSDebug.rate')
    g.addEdge('FXAA.dst', 'VRSDebug.rendering')
    g.markOutput('VRSDebug.color')
    return g
