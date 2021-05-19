from Data.VRS.graphs import *
import os

# Scene
p = os.getenv('FALCOR_MEDIA_FOLDERS').strip('/')
m.loadScene(p + '/Tatzgern/Sponza.fscene')
m.scene.renderSettings = SceneRenderSettings(useEnvLight=True, useAnalyticLights=True, useEmissiveLights=True)
m.addGraph(yangGraph())
m.addGraph(captureGraph())
m.addGraph(remoteGraph())

# Window Configuration
m.ui = True
m.resizeSwapChain(1920, 1080)
m.frameCapture.outputDir = '.'
m.clock.framerate = 0
m.clock.time = 0
