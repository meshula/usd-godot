#include "usd_scene_builder.h"
#include "usd_mesh_builder.h"   // transplanted: build_polygon_mesh / build_primitive_mesh_instance / build_points_instance
#include "usd_curves.h"         // transplanted: build_basis_curves_node
#include "usd_lights.h"         // ported: build_light_node
#include "usd_skel.h"           // transplanted: build_skeleton_node / append_skin_bindings / append_baked_skeleton_animations
#include "usd_materials.h"      // transplanted: build_material_from_usd_material
#include "usd_metadata.h"       // transplanted: store_composition_arcs / store_unmapped_properties
#include "usd_stage_utils.h"    // transplanted: set_usd_metadata (merges into a node's "usd" dict)

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>

#include <cmath>

// USD headers
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/points.h>
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdSkel/skeleton.h>
#include <pxr/usd/usdSkel/animation.h>
#include <pxr/usd/usdSkel/blendShape.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/base/gf/matrix4d.h>

namespace godot {

void UsdSceneBuilder::_tag_prim_path(const pxr::UsdPrim &p_prim, Node *p_node) {
    if (!p_node) {
        return;
    }
    // Merge into any existing "usd" dict (e.g. material metadata set in the mesh branch) rather
    // than overwrite it.
    godot_usd::set_usd_metadata(p_node, "usd:prim_path", String(p_prim.GetPath().GetString().c_str()));
}

void UsdSceneBuilder::_apply_transform(const pxr::UsdPrim &p_prim, Node3D *p_node) {
    if (!p_node || !p_prim.IsA<pxr::UsdGeomXformable>()) {
        return;
    }
    pxr::UsdGeomXformable xformable(p_prim);
    pxr::GfMatrix4d matrix;
    bool reset_xform_stack = false;
    xformable.GetLocalTransformation(&matrix, &reset_xform_stack);

    Basis basis;
    basis.set_column(0, Vector3(matrix[0][0], matrix[0][1], matrix[0][2]));
    basis.set_column(1, Vector3(matrix[1][0], matrix[1][1], matrix[1][2]));
    basis.set_column(2, Vector3(matrix[2][0], matrix[2][1], matrix[2][2]));
    Vector3 origin(matrix[3][0], matrix[3][1], matrix[3][2]);

    Transform3D transform;
    transform.set_basis(basis);
    transform.set_origin(origin);
    p_node->set_transform(transform);
}

Node *UsdSceneBuilder::build_node(const pxr::UsdPrim &p_prim, Node *p_parent, Node *p_scene_root) {
    // Pseudo-root has no node of its own; recurse its children into the parent.
    if (p_prim.IsPseudoRoot()) {
        for (const pxr::UsdPrim &child : p_prim.GetChildren()) {
            build_node(child, p_parent, p_scene_root);
        }
        return p_parent;
    }

    // Shading prims are consumed by the material translator on their bound gprim; they do not map
    // to nodes and have no node-worthy descendants. Skip the subtree.
    if (p_prim.IsA<pxr::UsdShadeMaterial>() || p_prim.IsA<pxr::UsdShadeShader>()) {
        return nullptr;
    }

    const String prim_name = String(p_prim.GetName().GetString().c_str());
    Node3D *node = nullptr;
    Dictionary node_notes;   // mapping notes from curves/lights, applied to the node after dispatch

    if (p_prim.IsA<pxr::UsdGeomCamera>()) {
        Camera3D *camera = memnew(Camera3D);
        camera->set_name(prim_name);

        pxr::UsdGeomCamera usd_camera(p_prim);
        float focal_length = 50.0f;
        usd_camera.GetFocalLengthAttr().Get(&focal_length);
        float horizontal_aperture = 24.0f;
        usd_camera.GetHorizontalApertureAttr().Get(&horizontal_aperture);
        if (focal_length > 0.0f) {
            float fov = 2.0f * atan(horizontal_aperture / (2.0f * focal_length)) * (180.0f / M_PI);
            camera->set_fov(fov);
        }
        node = camera;
    } else if (Node *light_node = godot_usd::build_light_node(p_prim.GetStage(), p_prim, pxr::UsdTimeCode::Default(), &node_notes)) {
        // UsdLux* -> DirectionalLight3D / OmniLight3D / SpotLight3D / AreaLight3D (proxy fallback).
        node = Object::cast_to<Node3D>(light_node);
        if (!node) {
            node = memnew(Node3D);
        }
    } else if (p_prim.IsA<pxr::UsdGeomBasisCurves>()) {
        // Checked before the generic Gprim branch (BasisCurves is a Gprim subclass).
        node = godot_usd::build_basis_curves_node(p_prim, pxr::UsdTimeCode::Default(), &node_notes);
        if (!node) {
            node = memnew(Node3D);
        }
    } else if (p_prim.IsA<pxr::UsdSkelSkeleton>()) {
        // Skeleton3D; skinned meshes are bound to it by the append_skin_bindings post-pass, and
        // UsdSkelAnimation is baked by append_baked_skeleton_animations (see build_scene/build_into).
        node = godot_usd::build_skeleton_node(p_prim.GetStage(), pxr::UsdTimeCode::Default(), Transform3D(), p_prim, &node_notes);
        if (!node) {
            node = memnew(Node3D);
        }
    } else if (p_prim.IsA<pxr::UsdGeomMesh>()) {
        // Polygon mesh: transplanted builder produces a multi-surface ArrayMesh (one surface per
        // material subset) with full UsdPreviewSurface + UsdUVTexture materials baked into surfaces.
        MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
        Dictionary mapping_notes;
        godot_usd::MeshBuildResult built = godot_usd::build_polygon_mesh(
                p_prim.GetStage(), pxr::UsdTimeCode::Default(), pxr::UsdGeomMesh(p_prim), &mapping_notes);
        if (built.mesh.is_valid()) {
            mesh_instance->set_mesh(built.mesh);
        }
        // Attach the same node metadata godot-usd's loader does, so downstream code (and the corpus)
        // can see material bindings/subsets and mapping notes.
        if (!built.material_paths.is_empty()) {
            godot_usd::set_usd_metadata(mesh_instance, "usd:material_bindings", built.material_paths);
        }
        if (!built.material_subsets.is_empty()) {
            godot_usd::set_usd_metadata(mesh_instance, "usd:material_subsets", built.material_subsets);
        }
        const Array note_keys = mapping_notes.keys();
        for (int i = 0; i < note_keys.size(); i++) {
            godot_usd::set_usd_metadata(mesh_instance, String(note_keys[i]), mapping_notes[note_keys[i]]);
        }
        node = mesh_instance;
    } else if (p_prim.IsA<pxr::UsdGeomPoints>()) {
        // Pass node_notes so points-mapping + skinning metadata (usd:points_mapping, usd:point_count,
        // usd:skel_skeleton_path) is captured and applied to the node after dispatch.
        Node *pts = godot_usd::build_points_instance(p_prim.GetStage(), pxr::UsdTimeCode::Default(), p_prim, &node_notes);
        node = Object::cast_to<Node3D>(pts);
        if (!node) {
            node = memnew(Node3D);
        }
    } else if (p_prim.IsA<pxr::UsdGeomGprim>()) {
        // Analytic primitives (Cube/Sphere/Cone/Cylinder/Capsule/Plane).
        Node *prim_node = godot_usd::build_primitive_mesh_instance(pxr::UsdTimeCode::Default(), p_prim);
        node = Object::cast_to<Node3D>(prim_node);
        if (!node) {
            node = memnew(Node3D);
        }
        // build_primitive_mesh_instance builds only geometry; apply any bound material here (the
        // polygon-mesh path bakes materials into surfaces, but primitives need it applied directly).
        MeshInstance3D *prim_mesh = Object::cast_to<MeshInstance3D>(node);
        if (prim_mesh != nullptr) {
            pxr::UsdShadeMaterial bound = pxr::UsdShadeMaterialBindingAPI(p_prim).ComputeBoundMaterial();
            if (bound) {
                Ref<Material> mat = godot_usd::build_material_from_usd_material(p_prim.GetStage(), pxr::UsdTimeCode::Default(), bound);
                if (mat.is_valid()) {
                    prim_mesh->set_surface_override_material(0, mat);
                }
            }
        }
    } else {
        // Xform / Scope / typeless organizational prims -> Node3D.
        node = memnew(Node3D);
    }
    node->set_name(prim_name);

    // Apply mapping notes gathered by curves/lights builders (light-mapping caveats, etc.).
    const Array note_keys = node_notes.keys();
    for (int i = 0; i < note_keys.size(); i++) {
        godot_usd::set_usd_metadata(node, String(note_keys[i]), node_notes[note_keys[i]]);
    }

    _apply_transform(p_prim, node);
    _tag_prim_path(p_prim, node);
    // Capture unmapped USD properties + composition arcs (references/payloads/inherits/specializes,
    // and usd:composition_preservation_mode) into node metadata for round-tripping.
    godot_usd::store_unmapped_properties(p_prim, pxr::UsdTimeCode::Default(), node);
    godot_usd::store_composition_arcs(p_prim, node);

    if (p_parent) {
        p_parent->add_child(node);
    }
    if (p_scene_root) {
        node->set_owner(p_scene_root);
    }

    // If no scene root was supplied, this node is the root of its own subtree (owner stays null so
    // it can be handed to PackedScene::pack as the scene root).
    Node *child_root = p_scene_root ? p_scene_root : node;
    for (const pxr::UsdPrim &child : p_prim.GetChildren()) {
        // Skip prims that are consumed by their parent (subsets) or handled out-of-band
        // (materials/shaders/skel animation/blend shapes) rather than mapped to their own nodes.
        if (child.IsA<pxr::UsdGeomSubset>() || child.IsA<pxr::UsdShadeMaterial>() || child.IsA<pxr::UsdShadeShader>() ||
                child.IsA<pxr::UsdSkelAnimation>() || child.IsA<pxr::UsdSkelBlendShape>()) {
            continue;
        }
        build_node(child, node, child_root);
    }
    return node;
}

// Post-passes that must run after the whole subtree exists: bind skinned meshes to their
// Skeleton3D (Skin + skeleton_path) and bake UsdSkelAnimation into an AnimationPlayer.
void UsdSceneBuilder::_finalize_skeletons(const pxr::UsdStageRefPtr &p_stage, Node *p_root) {
    Node3D *root3d = Object::cast_to<Node3D>(p_root);
    if (!root3d) {
        return;
    }
    godot_usd::append_skin_bindings(root3d);
    godot_usd::append_baked_skeleton_animations(p_stage, pxr::UsdTimeCode::Default(), root3d);
}

void UsdSceneBuilder::build_into(const pxr::UsdStageRefPtr &p_stage, Node *p_parent, Node *p_scene_root) {
    if (!p_stage || !p_parent) {
        return;
    }
    pxr::UsdPrim start = p_stage->GetDefaultPrim();
    if (!start) {
        start = p_stage->GetPseudoRoot();
    }
    build_node(start, p_parent, p_scene_root);
    _finalize_skeletons(p_stage, p_scene_root ? p_scene_root : p_parent);
}

Node *UsdSceneBuilder::build_scene(const pxr::UsdStageRefPtr &p_stage) {
    if (!p_stage) {
        return nullptr;
    }
    // Always wrap the stage's top-level prims under a synthetic scene root (matches godot-usd's
    // loader). This keeps the USD prim hierarchy — including a top-level prim named e.g. "root" —
    // as CHILDREN of the scene root, so node paths and baked AnimationPlayer track paths read as
    // "root/Plane/Plane:Key_1" (relative to the wrapper), which the corpus asserts.
    Node3D *root = memnew(Node3D);
    root->set_name("Root");
    for (const pxr::UsdPrim &child : p_stage->GetPseudoRoot().GetChildren()) {
        build_node(child, root, root);
    }

    _finalize_skeletons(p_stage, root);
    // The skel post-passes may add nodes (e.g. an AnimationPlayer) after the owner-assigning
    // build_node walk. Re-assign owners so those nodes are included when this tree is packed into a
    // PackedScene (root itself keeps owner=null as the scene root).
    if (root) {
        godot_usd::mark_owner_recursive(root, root);
    }
    return root;
}

} // namespace godot
