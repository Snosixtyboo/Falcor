import bpy
from bpy.props import PointerProperty
import mathutils
import numpy as np
import bmesh
import time
import threading
from math import radians
from bpy_extras.object_utils import AddObjectHelper, object_data_add
from random import seed
from random import random

sceneBVH = None
marcherWorld = None
marcherBBWorld = None
domainWorldInvs = None
possibleLocations = None
BVHTree = mathutils.bvhtree.BVHTree
helperCam = None
verified = set()


def sceneIntersects(obj, delta):
    bm1 = bmesh.new()
    bm1.from_mesh(obj.data) 
    bm1.transform(mathutils.Matrix.Translation(delta) @ obj.matrix_basis) 
    objBVH = BVHTree.FromBMesh(bm1)
    inter = objBVH.overlap(sceneBVH)
    bm1.free()
    if inter != []:
        return True
    return False


def marcherFits(context, offset):
    for inv in domainWorldInvs:
        inside = True
        for b in marcherBBWorld:
            point = b + offset
            pointDomain = inv @ mathutils.Vector((point.x, point.y, point.z))

            if (
                pointDomain.x > 1 or 
                pointDomain.y > 1 or
                pointDomain.z > 1 or
                pointDomain.x < -1 or
                pointDomain.y < -1 or
                pointDomain.z < -1
            ):
                inside = False
                break
        if inside:
            return True
    return False


def DFSFill(context):
    visited = set()
    coordsToDO = [mathutils.Vector((0,0,0))]
    while len(coordsToDO) != 0:
        coords = coordsToDO.pop(0)
        for i in range(3):
            offsets = (2, -2)
            for offset in offsets:
                testCoords = coords.copy()
                testCoords[i] += offset
                t = (testCoords.x, testCoords.y, testCoords.z)
                if t not in visited:
                    visited.add(t)
                    if marcherFits(context, marcherWorld.to_3x3() @ testCoords):
                        delta = marcherWorld.to_3x3() @ testCoords
                        if sceneIntersects(bpy.context.scene.marchProp, delta):
                            continue
                        coordsToDO.append(testCoords)
                        testPosW = bpy.context.scene.marchProp.location + delta
                        possibleLocations.append(testPosW)
                        if bpy.context.scene.fillersProp:
                            copyobj = bpy.context.scene.marchProp.copy()
                            copyobj.location = testPosW
                            bpy.context.scene.fillersProp.objects.link(copyobj)
        
def clearFillers():
    if bpy.context.scene.fillersProp:
        for obj in bpy.context.scene.fillersProp.objects:
            bpy.data.objects.remove(obj)

   
def clearViewpoints():
    if bpy.context.scene.camLocsProp:
        for obj in bpy.context.scene.camLocsProp.objects:
            bpy.data.objects.remove(obj)


def march(context):
    global marcherWorld
    marcherWorld = bpy.context.scene.marchProp.matrix_world
    
    global marcherBBWorld
    marcherBBWorld = [marcherWorld @ mathutils.Vector(bbvert) for bbvert in bpy.context.scene.marchProp.bound_box]    
    
    global domainWorldInvs
    domainWorldInvs = []
    for domainObj in bpy.context.scene.domainProp.objects:
        inv = domainObj.matrix_world.copy()
        inv.invert()
        domainWorldInvs.append(inv)
        
    global sceneBVH
    bm1 = bmesh.new()
    bm1.from_mesh(bpy.context.scene.sceneProp.data)
    bm1.transform(bpy.context.scene.sceneProp.matrix_world)
    sceneBVH = BVHTree.FromBMesh(bm1)  
    bm1.free()
    
    global possibleLocations
    possibleLocations = []
    
    clearFillers()
    DFSFill(context)


def makeOrGetBrightGreen():
    mat = bpy.data.materials.get("CamGreen")
    if mat is None:
        mat = bpy.data.materials.new(name="CamGreen")
        nodes = mat.node_tree.nodes
        nodes.clear()
        node_emission = nodes.new(type='ShaderNodeEmission')
        node_emission.inputs[0].default_value = (0,1,0,1)  # green RGBA
        node_emission.inputs[1].default_value = 5.0 # strength
        node_output = nodes.new(type="ShaderNodeOutputMaterial")
        links = mat.node_tree.links
        links.new(node_emission.outputs[0], node_output.inputs[0])
    return mat    


def checkImage():
    # render
    bpy.ops.render.render()
    # get viewer pixels
    pixels = bpy.data.images['Viewer Node'].pixels
    # copy buffer to numpy array for faster manipulation
    arr = np.array(pixels[:])
    arr = np.reshape(arr, (-1, 4))
    num_samples = np.count_nonzero(arr[:,3])
    
    print(100*num_samples/(len(pixels)/4))
    if (100*num_samples/(len(pixels)/4)) < bpy.context.scene.minForegroundProp:
        return False
    return True
 
def selectUnverified():
    print(len(verified))
    for obj in bpy.context.selected_objects:
        obj.select_set(False)
    for cam in bpy.context.scene.camLocsProp.objects:
        t = cam.location
        r = cam.rotation_euler
        if (t.x, t.y, t.z, r.x, r.y, r.z) not in verified:
            cam.select_set(True)
            
def verifyObjects(objs):
    global helperCam
    if helperCam is None:
        cam1 = bpy.data.cameras.new("Camera 1")
        helperCam = bpy.data.objects.new("Helper Cam", cam1)
        #Set up renderer for taking screenshots
    bpy.context.scene.camera = helperCam
    bpy.context.scene.render.engine = 'BLENDER_EEVEE'
    bpy.context.scene.eevee.taa_render_samples = 1
    bpy.context.scene.display_settings.display_device = 'None'
    bpy.context.scene.view_settings.view_transform = 'Standard'
    bpy.context.scene.view_settings.look = 'None'
    bpy.context.scene.sequencer_colorspace_settings.name = 'Raw'
    bpy.context.scene.render.filter_size = 0
    bpy.context.scene.render.film_transparent = True
    bpy.context.scene.render.resolution_x = bpy.context.scene.renderCamResX
    bpy.context.scene.render.resolution_y = bpy.context.scene.renderCamResY
    bpy.context.scene.marchProp.hide_render = True
    bpy.context.scene.domainProp.hide_render = True
    bpy.context.scene.camProp.hide_render = True
    # Set up compositor context
    bpy.context.scene.use_nodes = True
    tree = bpy.context.scene.node_tree
    links = tree.links
    # clear default nodes
    for n in tree.nodes:
        tree.nodes.remove(n)
    # create input render layer node
    rl = tree.nodes.new('CompositorNodeRLayers')      
    rl.location = 185,285
    v = tree.nodes.new('CompositorNodeViewer')   
    v.location = 750,210
    links.new(rl.outputs[0], v.inputs[0])  # link Image output to Viewer input
    
    allGood = True    
        
    for obj in objs:
        helperCam.location = obj.location
        helperCam.rotation_euler = obj.rotation_euler
        
        if not checkImage():
            print("Rejecting view due to lack of foreground geometry")
            allGood = False
            continue
        
        if sceneIntersects(obj, mathutils.Vector((0,0,0))):
            print("Rejecting view due to intersection with scene!")
            allGood = False
            continue
        
        t = helperCam.location
        r = helperCam.rotation_euler
        verified.add((t.x, t.y, t.z, r.x, r.y, r.z))
        
    return allGood
    
def generateViews(numViews, reset, checked):
    
    if reset:
        #Remove existing cams
        clearViewpoints()
        #Init seed
        seed(bpy.context.scene.randSeedProp)
        #Generate and test views
    
    viewsMade = 0
    while viewsMade < numViews:
        print("Making view ", viewsMade, "/", numViews)
        locID = random() * len(possibleLocations)
        testLocation = possibleLocations[int(locID)]
        testRotation = bpy.context.scene.camProp.rotation_euler.copy()
        testRotation[1] += radians(random() * (
            bpy.context.scene.rollMaxProp - bpy.context.scene.rollMinProp
        ) + bpy.context.scene.rollMinProp)
        testRotation[0] += radians(random() * (
            bpy.context.scene.pitchMaxProp - bpy.context.scene.pitchMinProp
        ) + bpy.context.scene.pitchMinProp)
        testRotation[2] += radians(random() * (
            bpy.context.scene.yawMaxProp - bpy.context.scene.yawMinProp
        ) + bpy.context.scene.yawMinProp)
            
        
        copyobj = bpy.context.scene.camProp.copy()
        copyobj.location = testLocation
        copyobj.rotation_euler = testRotation
        
        if checked and not verifyObjects([copyobj,]):
            bpy.data.objects.remove(copyobj)
            continue
        
        bpy.context.scene.camLocsProp.objects.link(copyobj)
        copyobj.select_set(True)
        viewsMade += 1
        
class FixSelectedOperator(bpy.types.Operator):
    """Fix selected viewpoints"""
    bl_idname = "object.fix_selected"
    bl_label = "Fix selected"

    @classmethod
    def poll(cls, context):
        return (
            bpy.context.scene.camProp is not None and
            bpy.context.scene.camLocsProp is not None and
            possibleLocations is not None and
            sceneBVH is not None and
            len(possibleLocations) != 0
        )

    def execute(self, context):
        toFix = len(bpy.context.selected_objects)
        for obj in bpy.context.selected_objects:
            bpy.data.objects.remove(obj)
        generateViews(toFix, reset=False, checked=True)
        return {'FINISHED'}
        
class VerifySelectedOperator(bpy.types.Operator):
    """Verify selected objects as viewpoints"""
    bl_idname = "object.verify_selected"
    bl_label = "Verify selected"

    @classmethod
    def poll(cls, context):
        return sceneBVH is not None

    def execute(self, context):
        verifyObjects(bpy.context.selected_objects)
        return {'FINISHED'}

class SelectUnverifiedOperator(bpy.types.Operator):
    """Select view points that haven't been verified"""
    bl_idname = "object.select_unverified"
    bl_label = "Select unverified viewpoints"

    @classmethod
    def poll(cls, context):
        return bpy.context.scene.camLocsProp is not None

    def execute(self, context):
        selectUnverified()
        return {'FINISHED'}
    
class ClearVerificationOperator(bpy.types.Operator):
    """Remove cached verification data"""
    bl_idname = "object.clear_verification"
    bl_label = "Clear verification data"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        global verified
        verified = set()
        return {'FINISHED'}

class ClearFillersOperator(bpy.types.Operator):
    """Clear fillers"""
    bl_idname = "object.clear_fillers_operator"
    bl_label = "Clear filler cubes"

    @classmethod
    def poll(cls, context):
        return bpy.context.scene.fillersProp is not None

    def execute(self, context):
        clearFillers()
        return {'FINISHED'}


class ClearCamsOperator(bpy.types.Operator):
    """Clear cams"""
    bl_idname = "object.clear_cams_operator"
    bl_label = "Clear viewpoints"

    @classmethod
    def poll(cls, context):
        return bpy.context.scene.camLocsProp is not None

    def execute(self, context):
        clearViewpoints()
        return {'FINISHED'}


class MarchingOperator(bpy.types.Operator):
    """Start marching procedure"""
    bl_idname = "object.marching_operator"
    bl_label = "Begin Marching"

    @classmethod
    def poll(cls, context):
        return (
            bpy.context.scene.marchProp is not None and
            bpy.context.scene.domainProp is not None and
            bpy.context.scene.sceneProp is not None
        )

    def execute(self, context):
        march(context)
        return {'FINISHED'}

    
class ViewGenOperator(bpy.types.Operator):
    """Start view generation procedure"""
    bl_idname = "object.viewgen_operator"
    bl_label = "Generate viewpoints"

    @classmethod
    def poll(cls, context):
        return (
            bpy.context.scene.camProp is not None and
            bpy.context.scene.camLocsProp is not None and
            possibleLocations is not None and
            len(possibleLocations) != 0
        )

    def execute(self, context):
        generateViews(bpy.context.scene.numViewsProp, reset=True, checked=False)
        return {'FINISHED'}

    
class MakeDefaultCamOperator(bpy.types.Operator, AddObjectHelper):
    """Default Cam"""
    bl_idname = "object.make_default_cam"
    bl_label = "Default"

    @classmethod
    def poll(cls, context):
        return True

    def execute(self, context):
        verts = [
            mathutils.Vector((0, 0, 0.2)),
            mathutils.Vector((1, -0.5, -2)),
            mathutils.Vector((1, 0.5, -2)),
            mathutils.Vector((-1, -0.5, -2)),
            mathutils.Vector((-1, 0.5, -2))
        ]

        edges = []
        faces = [[0,1,2],[0,2,4],[0,3,1],[0,4,3],[1,3,2],[2,3,4]]
        
        mesh = bpy.data.meshes.new('cam_mesh')
        mesh.from_pydata(verts, edges, faces)

        obj = object_data_add(context, mesh, operator=self)
        obj.rotation_euler = (radians(-90),0,0)
        obj.modifiers.new("Wireframe", 'WIREFRAME')
        obj.data.materials.append(makeOrGetBrightGreen())

        bpy.context.scene.camProp = obj
        
        return {'FINISHED'}
    
class WriteCamFileOperator(bpy.types.Operator, AddObjectHelper):
    """Default Cam"""
    bl_idname = "object.write_cam_file"
    bl_label = "Write viewpoints to file"

    @classmethod
    def poll(cls, context):
        return bpy.context.scene.camLocsProp is not None

    def execute(self, context):
        file = open(bpy.context.scene.outFileProp, "w")
        firstLine = True
        for obj in bpy.context.scene.camLocsProp.objects:
            if not firstLine:
                file.write("\n")
            firstLine = False
            m = obj.matrix_world.copy()
            #m.invert()
            for i in range(0, 4):
                for j in range(0, 4):
                    file.write(str(m[i][j]) + " ")
        
        return {'FINISHED'}


class AUTOVIEW_PT_layout_panel(bpy.types.Panel):
    bl_label = "Auto-View Generation"
    bl_category = "View Generation Panel"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "scene"

    def draw(self, context):
        scene = context.scene
        layout = self.layout
        
        col = layout.column() 
        col.prop(scene, "sceneProp", text="Scene")
        col.prop(scene, "domainProp", text="Domain")
        col.prop(scene, "marchProp", text="Marcher")
        col.prop(scene, "fillersProp", text="Fillers")
        col.operator("object.clear_fillers_operator")
        col.operator("object.marching_operator")
        
        col = layout.column()
        col.label(text="Viewpoint Indicator Geometry:")
        row = layout.row(align=True)
        row.operator("object.make_default_cam")
        row.prop(scene, "camProp", text="")
        
        col = layout.column()
        col.label(text="Viewpoint Angle Variation:")
        
        row = layout.row(align=True)
        row.label(text="Yaw:")
        row.prop(scene, "yawMinProp", text="")
        row.prop(scene, "yawMaxProp", text="")
        row = layout.row(align=True)
        row.label(text="Pitch:")
        row.prop(scene, "pitchMinProp", text="")
        row.prop(scene, "pitchMaxProp", text="")
        row = layout.row(align=True)
        row.label(text="Roll:")
        row.prop(scene, "rollMinProp", text="")
        row.prop(scene, "rollMaxProp", text="")
        
        row = layout.row(align=True)
        row.label(text="Number Views:")
        row.prop(scene, "numViewsProp", text="#Views")
        row = layout.row(align=True)
        row.label(text="Random Seed:")
        row.prop(scene, "randSeedProp", text="")
        
        col = layout.column() 
        col.label(text="Viewpoint Generation:")
        col.prop(scene, "camLocsProp", text="Result")
        col.operator("object.clear_cams_operator")
        col.operator("object.viewgen_operator")
        
        col.label(text="Viewpoint Verification:")
        row = layout.row(align=True)
        row.label(text="Resolution:")
        row.prop(scene, "renderCamResX",text="X")
        row.prop(scene, "renderCamResY",text="Y")
        row = layout.row(align=True)
        row.label(text="Min. Foreground:")
        row.prop(scene, "minForegroundProp", text="")
        
        col = layout.column() 
        col.operator("object.clear_verification")
        col.operator("object.select_unverified")
        col.operator("object.verify_selected")
        col.operator("object.fix_selected")
        
        col.label(text="Output:")
        col.prop(scene, "outFileProp", text="Path")
        col.operator("object.write_cam_file")

def register():
    bpy.types.Scene.domainProp = PointerProperty(type=bpy.types.Collection)
    bpy.types.Scene.marchProp = PointerProperty(type=bpy.types.Object)
    bpy.types.Scene.sceneProp = PointerProperty(type=bpy.types.Object)
    bpy.types.Scene.fillersProp = PointerProperty(type=bpy.types.Collection)
    bpy.types.Scene.camProp = PointerProperty(type=bpy.types.Object)
    bpy.types.Scene.renderCamResX = bpy.props.IntProperty(default=30, min=1, step=1)
    bpy.types.Scene.renderCamResY = bpy.props.IntProperty(default=20, min=1, step=1)
    bpy.types.Scene.camLocsProp = PointerProperty(type=bpy.types.Collection)
    bpy.types.Scene.numViewsProp = bpy.props.IntProperty(default=100, min=0, step=1)
    bpy.types.Scene.yawMinProp = bpy.props.FloatProperty(default=-180, min=-180, max = 0)
    bpy.types.Scene.yawMaxProp = bpy.props.FloatProperty(default=180, min=0, max=180)
    bpy.types.Scene.pitchMinProp = bpy.props.FloatProperty(default=-20, min=-180, max = 0)
    bpy.types.Scene.pitchMaxProp = bpy.props.FloatProperty(default=20, min=0, max=180)
    bpy.types.Scene.rollMinProp = bpy.props.FloatProperty(default=-0, min=-180, max = 0)
    bpy.types.Scene.rollMaxProp = bpy.props.FloatProperty(default=0, min=0, max=180)
    bpy.types.Scene.randSeedProp = bpy.props.IntProperty(default=42, min=0, max=1024, step=1)
    bpy.types.Scene.minForegroundProp = bpy.props.IntProperty(default=15, min=0, max=100, step=1, subtype="PERCENTAGE")
    bpy.types.Scene.outFileProp = bpy.props.StringProperty(default='./out.cfg', subtype='FILE_PATH')
    bpy.utils.register_class(AUTOVIEW_PT_layout_panel)
    bpy.utils.register_class(MarchingOperator)
    bpy.utils.register_class(ViewGenOperator)
    bpy.utils.register_class(ClearFillersOperator)
    bpy.utils.register_class(MakeDefaultCamOperator)
    bpy.utils.register_class(ClearCamsOperator)
    bpy.utils.register_class(SelectUnverifiedOperator)
    bpy.utils.register_class(ClearVerificationOperator)
    bpy.utils.register_class(VerifySelectedOperator)
    bpy.utils.register_class(FixSelectedOperator)
    bpy.utils.register_class(WriteCamFileOperator)

def unregister():
    bpy.utils.unregister_class(AUTOVIEW_PT_layout_panel)
    bpy.utils.unregister_class(MarchingOperator)
    bpy.utils.unregister_class(ViewGenOperator)
    bpy.utils.unregister_class(ClearFillersOperator)
    bpy.utils.unregister_class(MakeDefaultCamOperator)
    bpy.utils.unregister_class(ClearCamsOperator)

   
if __name__ == "__main__":
    register()