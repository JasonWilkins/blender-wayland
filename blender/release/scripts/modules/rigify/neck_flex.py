# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

import bpy
from rigify import RigifyError
from rigify_utils import bone_class_instance, copy_bone_simple
from rna_prop_ui import rna_idprop_ui_prop_get

# not used, defined for completeness
METARIG_NAMES = ("body", "head")


def metarig_template():
    # generated by rigify.write_meta_rig
    bpy.ops.object.mode_set(mode='EDIT')
    obj = bpy.context.active_object
    arm = obj.data
    bone = arm.edit_bones.new('body')
    bone.head[:] = 0.0000, -0.0276, -0.1328
    bone.tail[:] = 0.0000, -0.0170, -0.0197
    bone.roll = 0.0000
    bone.connected = False
    bone = arm.edit_bones.new('head')
    bone.head[:] = 0.0000, -0.0170, -0.0197
    bone.tail[:] = 0.0000, 0.0726, 0.1354
    bone.roll = 0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['body']
    bone = arm.edit_bones.new('neck.01')
    bone.head[:] = 0.0000, -0.0170, -0.0197
    bone.tail[:] = 0.0000, -0.0099, 0.0146
    bone.roll = 0.0000
    bone.connected = False
    bone.parent = arm.edit_bones['head']
    bone = arm.edit_bones.new('neck.02')
    bone.head[:] = 0.0000, -0.0099, 0.0146
    bone.tail[:] = 0.0000, -0.0242, 0.0514
    bone.roll = 0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['neck.01']
    bone = arm.edit_bones.new('neck.03')
    bone.head[:] = 0.0000, -0.0242, 0.0514
    bone.tail[:] = 0.0000, -0.0417, 0.0868
    bone.roll = 0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['neck.02']
    bone = arm.edit_bones.new('neck.04')
    bone.head[:] = 0.0000, -0.0417, 0.0868
    bone.tail[:] = 0.0000, -0.0509, 0.1190
    bone.roll = 0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['neck.03']
    bone = arm.edit_bones.new('neck.05')
    bone.head[:] = 0.0000, -0.0509, 0.1190
    bone.tail[:] = 0.0000, -0.0537, 0.1600
    bone.roll = 0.0000
    bone.connected = True
    bone.parent = arm.edit_bones['neck.04']

    bpy.ops.object.mode_set(mode='OBJECT')
    pbone = obj.pose.bones['head']
    pbone['type'] = 'neck_flex'


def metarig_definition(obj, orig_bone_name):
    '''
    The bone given is the head, its parent is the body,
    # its only child the first of a chain with matching basenames.
    eg.
        body -> head -> neck_01 -> neck_02 -> neck_03.... etc
    '''
    arm = obj.data
    head = arm.bones[orig_bone_name]
    body = head.parent

    children = head.children
    if len(children) != 1:
        raise RigifyError("expected the head bone '%s' to have only 1 child." % orig_bone_name)

    child = children[0]
    bone_definition = [body.name, head.name, child.name]
    bone_definition.extend([child.name for child in child.children_recursive_basename])
    return bone_definition


def deform(obj, definitions, base_names, options):
    for org_bone_name in definitions[2:]:
        bpy.ops.object.mode_set(mode='EDIT')

        # Create deform bone.
        bone = copy_bone_simple(obj.data, org_bone_name, "DEF-%s" % base_names[org_bone_name], parent=True)

        # Store name before leaving edit mode
        bone_name = bone.name

        # Leave edit mode
        bpy.ops.object.mode_set(mode='OBJECT')

        # Get the pose bone
        bone = obj.pose.bones[bone_name]

        # Constrain to the original bone
        # XXX. Todo, is this needed if the bone is connected to its parent?
        con = bone.constraints.new('COPY_TRANSFORMS')
        con.name = "copy_loc"
        con.target = obj
        con.subtarget = org_bone_name


def main(obj, bone_definition, base_names, options):
    from Mathutils import Vector

    arm = obj.data

    # Initialize container classes for convenience
    mt = bone_class_instance(obj, ["body", "head"]) # meta
    mt.body = bone_definition[0]
    mt.head = bone_definition[1]
    mt.update()

    neck_chain = bone_definition[2:]

    mt_chain = bone_class_instance(obj, [("neck_%.2d" % (i + 1)) for i in range(len(neck_chain))]) # 99 bones enough eh?
    for i, attr in enumerate(mt_chain.attr_names):
        setattr(mt_chain, attr, neck_chain[i])
    mt_chain.update()

    neck_chain_basename = base_names[mt_chain.neck_01_e.name].split(".")[0]
    neck_chain_segment_length = mt_chain.neck_01_e.length

    ex = bone_class_instance(obj, ["head", "head_hinge", "neck_socket", "head_ctrl"]) # hinge & extras

    # Add the head hinge at the bodys location, becomes the parent of the original head

    # apply everything to this copy of the chain
    ex_chain = mt_chain.copy(base_names=base_names)
    ex_chain.neck_01_e.parent = mt_chain.neck_01_e.parent


    # Copy the head bone and offset
    ex.head_e = copy_bone_simple(arm, mt.head, "MCH-%s" % base_names[mt.head], parent=True)
    ex.head_e.connected = False
    ex.head = ex.head_e.name
    # offset
    head_length = ex.head_e.length
    ex.head_e.head.y += head_length / 2.0
    ex.head_e.tail.y += head_length / 2.0

    # Yes, use the body bone but call it a head hinge
    ex.head_hinge_e = copy_bone_simple(arm, mt.body, "MCH-%s_hinge" % base_names[mt.head], parent=False)
    ex.head_hinge_e.connected = False
    ex.head_hinge = ex.head_hinge_e.name
    ex.head_hinge_e.head.y += head_length / 4.0
    ex.head_hinge_e.tail.y += head_length / 4.0

    # Insert the neck socket, the head copys this loation
    ex.neck_socket_e = arm.edit_bones.new("MCH-%s_socked" % neck_chain_basename)
    ex.neck_socket = ex.neck_socket_e.name
    ex.neck_socket_e.connected = False
    ex.neck_socket_e.parent = mt.body_e
    ex.neck_socket_e.head = mt.head_e.head
    ex.neck_socket_e.tail = mt.head_e.head - Vector(0.0, neck_chain_segment_length / 2.0, 0.0)
    ex.neck_socket_e.roll = 0.0


    # copy of the head for controling
    ex.head_ctrl_e = copy_bone_simple(arm, mt.head, base_names[mt.head])
    ex.head_ctrl = ex.head_ctrl_e.name
    ex.head_ctrl_e.parent = ex.head_hinge_e

    for i, attr in enumerate(ex_chain.attr_names):
        neck_e = getattr(ex_chain, attr + "_e")

        # dont store parent names, re-reference as each chain bones parent.
        neck_e_parent = arm.edit_bones.new("MCH-rot_%s" % base_names[getattr(mt_chain, attr)])
        neck_e_parent.head = neck_e.head
        neck_e_parent.tail = neck_e.head + (mt.head_e.vector.normalize() * neck_chain_segment_length / 2.0)
        neck_e_parent.roll = mt.head_e.roll

        orig_parent = neck_e.parent
        neck_e.connected = False
        neck_e.parent = neck_e_parent
        neck_e_parent.connected = False

        if i == 0:
            neck_e_parent.parent = mt.body_e
        else:
            neck_e_parent.parent = orig_parent

    deform(obj, bone_definition, base_names, options)

    bpy.ops.object.mode_set(mode='OBJECT')

    mt.update()
    mt_chain.update()
    ex_chain.update()
    ex.update()

    # Axis locks
    ex.head_ctrl_p.lock_location = True, True, True

    # Simple one off constraints, no drivers
    con = ex.head_ctrl_p.constraints.new('COPY_LOCATION')
    con.target = obj
    con.subtarget = ex.neck_socket

    con = ex.head_p.constraints.new('COPY_ROTATION')
    con.target = obj
    con.subtarget = ex.head_ctrl

    # driven hinge
    prop = rna_idprop_ui_prop_get(ex.head_ctrl_p, "hinge", create=True)
    ex.head_ctrl_p["hinge"] = 0.0
    prop["soft_min"] = 0.0
    prop["soft_max"] = 1.0

    con = ex.head_hinge_p.constraints.new('COPY_ROTATION')
    con.name = "hinge"
    con.target = obj
    con.subtarget = mt.body

    # add driver
    hinge_driver_path = ex.head_ctrl_p.path_to_id() + '["hinge"]'

    fcurve = con.driver_add("influence", 0)
    driver = fcurve.driver
    var = driver.variables.new()
    driver.type = 'AVERAGE'
    var.name = "var"
    var.targets[0].id_type = 'OBJECT'
    var.targets[0].id = obj
    var.targets[0].data_path = hinge_driver_path

    #mod = fcurve_driver.modifiers.new('GENERATOR')
    mod = fcurve.modifiers[0]
    mod.poly_order = 1
    mod.coefficients[0] = 1.0
    mod.coefficients[1] = -1.0

    head_driver_path = ex.head_ctrl_p.path_to_id()

    target_names = [("b%.2d" % (i + 1)) for i in range(len(neck_chain))]

    ex.head_ctrl_p["bend_tot"] = 0.0
    fcurve = ex.head_ctrl_p.driver_add('["bend_tot"]', 0)
    driver = fcurve.driver
    driver.type = 'SUM'
    fcurve.modifiers.remove(0) # grr dont need a modifier

    for i in range(len(neck_chain)):
        var = driver.variables.new()
        var.name = target_names[i]
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = obj
        var.targets[0].data_path = head_driver_path + ('["bend_%.2d"]' % (i + 1))


    for i, attr in enumerate(ex_chain.attr_names):
        neck_p = getattr(ex_chain, attr + "_p")
        neck_p.lock_location = True, True, True
        neck_p.lock_location = True, True, True
        neck_p.lock_rotations_4d = True

        # Add bend prop
        prop_name = "bend_%.2d" % (i + 1)
        prop = rna_idprop_ui_prop_get(ex.head_ctrl_p, prop_name, create=True)
        ex.head_ctrl_p[prop_name] = 1.0
        prop["soft_min"] = 0.0
        prop["soft_max"] = 1.0

        # add parent constraint
        neck_p_parent = neck_p.parent

        # add constraint
        con = neck_p_parent.constraints.new('COPY_ROTATION')
        con.name = "Copy Rotation"
        con.target = obj
        con.subtarget = ex.head
        con.owner_space = 'LOCAL'
        con.target_space = 'LOCAL'

        fcurve = con.driver_add("influence", 0)
        driver = fcurve.driver
        driver.type = 'SCRIPTED'
        driver.expression = "bend/bend_tot"

        fcurve.modifiers.remove(0) # grr dont need a modifier


        # add target
        var = driver.variables.new()
        var.name = "bend_tot"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = obj
        var.targets[0].data_path = head_driver_path + ('["bend_tot"]')

        var = driver.variables.new()
        var.name = "bend"
        var.targets[0].id_type = 'OBJECT'
        var.targets[0].id = obj
        var.targets[0].data_path = head_driver_path + ('["%s"]' % prop_name)


        # finally constrain the original bone to this one
        orig_neck_p = getattr(mt_chain, attr + "_p")
        con = orig_neck_p.constraints.new('COPY_ROTATION')
        con.target = obj
        con.subtarget = neck_p.name


    # Set the head control's custom shape to use the last
    # org neck bone for its transform
    ex.head_ctrl_p.custom_shape_transform = obj.pose.bones[bone_definition[len(bone_definition)-1]]


    # last step setup layers
    if "ex_layer" in options:
        layer = [n==options["ex_layer"] for n in range(0,32)]
    else:
        layer = list(arm.bones[bone_definition[1]].layer)
    for attr in ex_chain.attr_names:
        getattr(ex_chain, attr + "_b").layer = layer
    for attr in ex.attr_names:
        getattr(ex, attr + "_b").layer = layer

    layer = list(arm.bones[bone_definition[1]].layer)
    ex.head_ctrl_b.layer = layer


    # no blending the result of this
    return None

