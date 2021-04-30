from Data.VRS.graphs import *
import os

# Scene
p = os.getenv('FALCOR_MEDIA_FOLDERS').strip('/')
m.loadScene(p + '/Bistro/BistroExterior.fscene')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.scene.setEnvMap(p + '/Bistro/san_giuseppe_bridge_4k.hdr')
m.scene.getLight(0).intensity = 10.0
m.scene.camera.nearPlane = 1
m.scene.camera.farPlane = 800.0
m.addGraph(captureGraph())
m.addGraph(vrsGraph())
m.addGraph(remoteGraph())

# Window Configuration
m.resizeSwapChain(1920, 1080)
m.ui = True
fc.outputDir = '.'
t.framerate = 0
t.time = 0
