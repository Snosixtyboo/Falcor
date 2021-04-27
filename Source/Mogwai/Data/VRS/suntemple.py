from Data.VRS.graphs import *
import os

# Scene
m.loadScene(os.getenv('FALCOR_MEDIA_FOLDERS').strip('/') + '/SunTemple/SunTemple.fscene')
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
m.addGraph(captureGraph())
m.addGraph(vrsGraph())
m.addGraph(remoteGraph())

# Window
m.resizeSwapChain(1920, 1080)
m.ui = True
fc.outputDir = '.'
t.framerate = 0
t.time = 0
