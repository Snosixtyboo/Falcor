from Data.VRS.graphs import *
import os

# Scene
p = os.getenv('FALCOR_MEDIA_FOLDERS').strip('/')
m.loadScene(p + '/Bistro/BistroExterior.fscene')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.scene.setEnvMap(p + '/Bistro/san_giuseppe_bridge_4k.hdr')
m.scene.getLight(0).intensity = float3(10.0, 10.0, 10.0)
m.scene.camera.nearPlane = 1
m.scene.camera.farPlane = 800.0
m.addGraph(yangGraph())
m.addGraph(captureGraph())
m.addGraph(remoteGraph())

# Window Configuration
m.ui = True
m.resizeSwapChain(1920, 1080)
m.frameCapture.outputDir = '.'
m.clock.framerate = 0
m.clock.time = 0