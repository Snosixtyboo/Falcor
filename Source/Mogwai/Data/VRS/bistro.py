from Data.VRS.graphs import *
import os

# Scene
p = os.getenv('FALCOR_MEDIA_FOLDERS').strip('/')
m.loadScene(p + '/Bistro/BistroInterior.fbx')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.scene.setEnvMap(p + '/Bistro/san_giuseppe_bridge_4k.hdr')
m.scene.camera.nearPlane = 0.10000000149011612
m.scene.camera.farPlane = 100.0
m.scene.cameraSpeed = 1.0
m.addGraph(jaliGraph())
m.addGraph(yangGraph())
m.addGraph(captureGraph())
m.addGraph(remoteGraph())

# Window Configuration
m.ui = True
m.resizeSwapChain(3840, 2160)
#m.resizeSwapChain(1920, 1080)
m.frameCapture.outputDir = '.'
m.clock.framerate = 0
m.clock.time = 0
