from Data.VRS.graphs import *
import os

# Scene
m.loadScene(os.getenv('FALCOR_MEDIA_FOLDERS').strip("/") + '/Bistro/BistroExterior.fbx')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
#m.scene.setEnvMap('C:/Users/jaliborc/Documents/Coding/vrs/data/scenes/SunTemple/SunTemple_Skybox.jpg')
m.scene.camera.nearPlane = 0.1
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
