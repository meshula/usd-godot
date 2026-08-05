#ifndef USD_SCENE_BUILDER_H
#define USD_SCENE_BUILDER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/prim.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace godot {

/**
 * UsdSceneBuilder - the single USD-prim -> Godot-node translation path.
 *
 * This replaces the three previously divergent converters (UsdDocument::_import_prim_hierarchy,
 * USDPlugin::_convert_prim_to_node, and the unimplemented UsdStageProxy::import_to_scene) with one
 * schema-dispatched (IsA<>) builder. Per-schema decoding is delegated to helper modules
 * (currently UsdMeshImportHelper); richer translators (materials, skel, curves, lights) are folded
 * in here in later phases.
 *
 * Every generated node carries a "usd" metadata dictionary with "usd:prim_path" so downstream
 * code (and the fidelity corpus) can map nodes back to their source prims.
 */
class UsdSceneBuilder {
public:
    // Build a standalone scene tree from a stage and return its root node (owner == null, ready to
    // hand to PackedScene::pack). Uses the stage's default prim as the root, or a synthetic "Root"
    // Node3D wrapping the pseudo-root's children when there is no default prim.
    static Node *build_scene(const pxr::UsdStageRefPtr &p_stage);

    // Append the stage's content under an existing parent node (used by the explicit import API and
    // the editor reflect path). p_scene_root is the owner assigned to every generated node.
    static void build_into(const pxr::UsdStageRefPtr &p_stage, Node *p_parent, Node *p_scene_root);

    // Recursive per-prim dispatcher. Creates the node for p_prim, parents it under p_parent (if
    // non-null), assigns ownership, recurses into children, and returns the created node (or null
    // for skipped prims such as Material/Shader). If p_scene_root is null, the node created for
    // p_prim becomes the scene root for its own descendants.
    static Node *build_node(const pxr::UsdPrim &p_prim, Node *p_parent, Node *p_scene_root);

private:
    static void _apply_transform(const pxr::UsdPrim &p_prim, Node3D *p_node);
    static void _tag_prim_path(const pxr::UsdPrim &p_prim, Node *p_node);
    // Skinning + animation post-passes, run after the full subtree is built.
    static void _finalize_skeletons(const pxr::UsdStageRefPtr &p_stage, Node *p_root);
};

} // namespace godot

#endif // USD_SCENE_BUILDER_H
