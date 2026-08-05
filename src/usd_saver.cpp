#include "usd_saver.h"
#include "usd_mesh_export_helper.h"
#include "usd_materials.h"     // transplanted: write_preview_material
#include "usd_stage_utils.h"   // transplanted: get_usd_metadata

#include <fstream>

#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/xformCommonAPI.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/base/gf/matrix4d.h>

namespace godot {

namespace {

const char *USD_EXTS[] = {"usd", "usda", "usdc", "usdz"};

bool is_usd_ext(const String &p_ext) {
    const String e = p_ext.to_lower();
    for (const char *x : USD_EXTS) {
        if (e == x) {
            return true;
        }
    }
    return false;
}

// Sanitize a Godot node name into a valid USD prim name component.
String sanitize_prim_name(const String &p_name) {
    String out = p_name.replace(":", "_").replace(".", "_").replace("-", "_").replace(" ", "_");
    if (out.is_empty()) {
        out = "Prim";
    }
    return out;
}

pxr::GfMatrix4d to_gf_matrix(const Transform3D &p_t) {
    const Basis b = p_t.get_basis();
    const Vector3 o = p_t.get_origin();
    pxr::GfMatrix4d m;
    m.SetRow(0, pxr::GfVec4d(b.get_column(0).x, b.get_column(0).y, b.get_column(0).z, 0.0));
    m.SetRow(1, pxr::GfVec4d(b.get_column(1).x, b.get_column(1).y, b.get_column(1).z, 0.0));
    m.SetRow(2, pxr::GfVec4d(b.get_column(2).x, b.get_column(2).y, b.get_column(2).z, 0.0));
    m.SetRow(3, pxr::GfVec4d(o.x, o.y, o.z, 1.0));
    return m;
}

// Write the mesh schema for a MeshInstance3D, matching the transplanted importer's convention
// (build_primitive_mesh_instance): Cube.size is uniform, Sphere.radius direct, etc. Any extra
// scale that can't be expressed in the schema (e.g. a non-uniform BoxMesh) is returned in
// r_extra_scale so the caller can fold it into the node transform. Returns the created prim.
pxr::UsdPrim export_mesh_prim(const Ref<Mesh> &p_mesh, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath &p_path, Vector3 *r_extra_scale) {
    *r_extra_scale = Vector3(1, 1, 1);

    Ref<BoxMesh> box = p_mesh;
    if (box.is_valid()) {
        // Importer reads Cube.size -> uniform BoxMesh(size,size,size); represent a (possibly
        // non-uniform) box as a unit cube plus a node scale equal to the box size.
        pxr::UsdGeomCube cube = pxr::UsdGeomCube::Define(p_stage, p_path);
        cube.GetSizeAttr().Set(1.0);
        *r_extra_scale = box->get_size();
        return cube.GetPrim();
    }
    Ref<SphereMesh> sphere = p_mesh;
    if (sphere.is_valid()) {
        pxr::UsdGeomSphere s = pxr::UsdGeomSphere::Define(p_stage, p_path);
        s.GetRadiusAttr().Set((double)sphere->get_radius());
        return s.GetPrim();
    }
    Ref<CapsuleMesh> capsule = p_mesh;
    if (capsule.is_valid()) {
        pxr::UsdGeomCapsule c = pxr::UsdGeomCapsule::Define(p_stage, p_path);
        c.GetRadiusAttr().Set((double)capsule->get_radius());
        c.GetHeightAttr().Set((double)capsule->get_height());
        return c.GetPrim();
    }
    Ref<CylinderMesh> cyl = p_mesh;
    if (cyl.is_valid()) {
        pxr::UsdGeomCylinder c = pxr::UsdGeomCylinder::Define(p_stage, p_path);
        c.GetRadiusAttr().Set((double)cyl->get_top_radius());
        c.GetHeightAttr().Set((double)cyl->get_height());
        return c.GetPrim();
    }
    // Fallback: generic polygon mesh via the existing helper (points/faces/normals/uv).
    UsdMeshExportHelper helper;
    return helper.export_mesh_to_prim(p_mesh, p_stage, p_path);
}

// ---- source-preserving save (4c): copy the original layer, patch only changed edits -------------

bool copy_file_bytes(const String &p_src_abs, const String &p_dst_abs) {
    std::ifstream in(p_src_abs.utf8().get_data(), std::ios::binary);
    if (!in) {
        return false;
    }
    std::ofstream out(p_dst_abs.utf8().get_data(), std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out << in.rdbuf();
    return true;
}

Transform3D gf_to_transform(const pxr::GfMatrix4d &m) {
    Basis b;
    b.set_column(0, Vector3(m[0][0], m[0][1], m[0][2]));
    b.set_column(1, Vector3(m[1][0], m[1][1], m[1][2]));
    b.set_column(2, Vector3(m[2][0], m[2][1], m[2][2]));
    return Transform3D(b, Vector3(m[3][0], m[3][1], m[3][2]));
}

// Author a node's transform onto a prim as separate translate/rotate/scale ops (what the corpus
// asserts), replacing any prior common-API ops.
void author_transform(const pxr::UsdPrim &p_prim, const Transform3D &p_t) {
    pxr::UsdGeomXformCommonAPI api(p_prim);
    const Vector3 o = p_t.origin;
    const Vector3 s = p_t.basis.get_scale();
    const Vector3 e = p_t.basis.get_euler();
    api.SetTranslate(pxr::GfVec3d(o.x, o.y, o.z));
    api.SetRotate(pxr::GfVec3f((float)(e.x * 180.0 / M_PI), (float)(e.y * 180.0 / M_PI), (float)(e.z * 180.0 / M_PI)));
    api.SetScale(pxr::GfVec3f(s.x, s.y, s.z));
}

// Walk the loaded scene; for each node mapped to a prim whose local transform changed, author the
// new transform onto that prim in the (copied) source stage. Leaves everything else untouched.
void author_changed_transforms(Node *p_node, pxr::UsdStageRefPtr p_stage) {
    Node3D *n3d = Object::cast_to<Node3D>(p_node);
    if (n3d != nullptr) {
        const Dictionary meta = godot_usd::get_usd_metadata(p_node);
        const String prim_path = meta.get("usd:prim_path", String());
        if (!prim_path.is_empty()) {
            pxr::UsdPrim prim = p_stage->GetPrimAtPath(pxr::SdfPath(prim_path.utf8().get_data()));
            if (prim && prim.IsA<pxr::UsdGeomXformable>()) {
                pxr::GfMatrix4d m;
                bool resets = false;
                pxr::UsdGeomXformable(prim).GetLocalTransformation(&m, &resets);
                if (!gf_to_transform(m).is_equal_approx(n3d->get_transform())) {
                    author_transform(prim, n3d->get_transform());
                }
            }
        }
    }
    for (int i = 0; i < p_node->get_child_count(); i++) {
        author_changed_transforms(p_node->get_child(i), p_stage);
    }
}

// Save a scene that was loaded from USD (has usd:source_path) by copying that source layer and
// authoring only the supported edits into the copy — preserving all authored structure. This first
// increment handles local transform edits.
Error save_source_preserving(Node *p_root, const String &p_source_path, const String &p_dest_path) {
    String src_abs = p_source_path;
    if (p_source_path.begins_with("res://") || p_source_path.begins_with("user://")) {
        src_abs = ProjectSettings::get_singleton()->globalize_path(p_source_path);
    }
    String dst_abs = p_dest_path;
    if (p_dest_path.begins_with("res://") || p_dest_path.begins_with("user://")) {
        dst_abs = ProjectSettings::get_singleton()->globalize_path(p_dest_path);
    }
    if (!copy_file_bytes(src_abs, dst_abs)) {
        UtilityFunctions::printerr("USD source-aware save: could not copy source ", p_source_path, " -> ", p_dest_path);
        return ERR_CANT_CREATE;
    }
    pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(dst_abs.utf8().get_data());
    if (!stage) {
        return ERR_CANT_OPEN;
    }
    author_changed_transforms(p_root, stage);
    stage->GetRootLayer()->Save();
    return OK;
}

// Effective material for surface p_surface: instance override -> per-surface override -> mesh material.
Ref<Material> resolve_surface_material(MeshInstance3D *p_mi, int p_surface) {
    Ref<Material> m = p_mi->get_material_override();
    if (m.is_null()) {
        m = p_mi->get_surface_override_material(p_surface);
    }
    if (m.is_null() && p_mi->get_mesh().is_valid()) {
        m = p_mi->get_mesh()->surface_get_material(p_surface);
    }
    return m;
}

// Export a multi-surface Godot ArrayMesh as a single UsdGeomMesh partitioned into GeomSubsets
// (family "materialBind"), one per surface, each with its material bound — the round-trip
// counterpart to the importer's subset -> multi-surface handling. Returns the mesh prim.
pxr::UsdPrim export_multisurface_mesh(MeshInstance3D *p_mi, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath &p_path, const String &p_save_path) {
    Ref<ArrayMesh> mesh = p_mi->get_mesh();
    pxr::UsdGeomMesh usd_mesh = pxr::UsdGeomMesh::Define(p_stage, p_path);

    pxr::VtArray<pxr::GfVec3f> points;
    pxr::VtArray<pxr::GfVec3f> normals;
    pxr::VtArray<pxr::GfVec2f> uvs;
    pxr::VtArray<int> face_counts;
    pxr::VtArray<int> face_indices;
    std::vector<pxr::VtArray<int>> subset_faces(mesh->get_surface_count());
    bool any_normals = false;
    bool any_uvs = false;

    int point_offset = 0;
    int face_cursor = 0;
    for (int s = 0; s < mesh->get_surface_count(); s++) {
        const Array arrays = mesh->surface_get_arrays(s);
        const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
        const PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
        const PackedVector3Array snorm = arrays[Mesh::ARRAY_NORMAL];
        const PackedVector2Array suv = arrays[Mesh::ARRAY_TEX_UV];

        for (int i = 0; i < verts.size(); i++) {
            points.push_back(pxr::GfVec3f(verts[i].x, verts[i].y, verts[i].z));
        }
        for (int i = 0; i < snorm.size(); i++) {
            normals.push_back(pxr::GfVec3f(snorm[i].x, snorm[i].y, snorm[i].z));
            any_normals = true;
        }
        for (int i = 0; i < suv.size(); i++) {
            uvs.push_back(pxr::GfVec2f(suv[i].x, suv[i].y)); // importer applies the V-flip
            any_uvs = true;
        }
        // Triangles, winding reversed to match the importer's rightHanded (CCW) reversal.
        for (int i = 0; i + 2 < indices.size(); i += 3) {
            face_counts.push_back(3);
            face_indices.push_back(indices[i] + point_offset);
            face_indices.push_back(indices[i + 2] + point_offset);
            face_indices.push_back(indices[i + 1] + point_offset);
            subset_faces[s].push_back(face_cursor);
            face_cursor++;
        }
        point_offset += verts.size();
    }

    usd_mesh.GetPointsAttr().Set(points);
    usd_mesh.GetFaceVertexCountsAttr().Set(face_counts);
    usd_mesh.GetFaceVertexIndicesAttr().Set(face_indices);
    if (any_normals) {
        usd_mesh.GetNormalsAttr().Set(normals);
        usd_mesh.SetNormalsInterpolation(pxr::UsdGeomTokens->vertex);
    }
    if (any_uvs) {
        pxr::UsdGeomPrimvarsAPI(usd_mesh.GetPrim())
            .CreatePrimvar(pxr::TfToken("st"), pxr::SdfValueTypeNames->TexCoord2fArray, pxr::UsdGeomTokens->vertex)
            .Set(uvs);
    }

    // One GeomSubset (family "materialBind") per surface, with its material bound.
    for (int s = 0; s < mesh->get_surface_count(); s++) {
        const std::string subset_name = "surface_" + std::to_string(s);
        pxr::UsdGeomSubset subset = pxr::UsdGeomSubset::CreateGeomSubset(
            usd_mesh, pxr::TfToken(subset_name), pxr::UsdGeomTokens->face,
            subset_faces[s], pxr::TfToken("materialBind"), pxr::UsdGeomTokens->partition);

        Ref<Material> material = resolve_surface_material(p_mi, s);
        if (material.is_valid()) {
            pxr::UsdShadeMaterial usd_material;
            if (godot_usd::write_preview_material(p_stage, material, subset.GetPrim().GetPath(), p_save_path, ("Material_" + itos(s)), &usd_material) && usd_material) {
                pxr::UsdShadeMaterialBindingAPI::Apply(subset.GetPrim()).Bind(usd_material);
            }
        }
    }
    return usd_mesh.GetPrim();
}

// Recursively author p_node (and children) into p_stage at p_prim_path. p_save_path is the target
// USD path (used by the material writer to resolve/relativize texture assets).
void export_node(Node *p_node, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath &p_prim_path, const String &p_save_path) {
    if (!p_node) {
        return;
    }
    pxr::UsdPrim prim;
    Vector3 extra_scale(1, 1, 1);

    MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_node);
    if (mesh_instance != nullptr && mesh_instance->get_mesh().is_valid()) {
        Ref<ArrayMesh> array_mesh = mesh_instance->get_mesh();
        if (array_mesh.is_valid() && array_mesh->get_surface_count() > 1) {
            // Multi-surface -> single UsdGeomMesh + per-surface GeomSubsets (materials bound inside).
            prim = export_multisurface_mesh(mesh_instance, p_stage, p_prim_path, p_save_path);
        } else {
            prim = export_mesh_prim(mesh_instance->get_mesh(), p_stage, p_prim_path, &extra_scale);

            // Bind the (single) material as a UsdPreviewSurface network. Resolution order matches
            // Godot's effective material: instance override -> per-surface override -> mesh material.
            Ref<Material> material = resolve_surface_material(mesh_instance, 0);
            if (prim && material.is_valid()) {
                pxr::UsdShadeMaterial usd_material;
                if (godot_usd::write_preview_material(p_stage, material, p_prim_path, p_save_path, "Material", &usd_material) && usd_material) {
                    pxr::UsdShadeMaterialBindingAPI::Apply(prim).Bind(usd_material);
                }
            }
        }
    }
    if (!prim) {
        prim = pxr::UsdGeomXform::Define(p_stage, p_prim_path).GetPrim();
    }

    // Author the node's local transform, folding in any schema-inexpressible primitive scale as a
    // single transform op (one op only -> unambiguous decomposition on reload).
    Node3D *node3d = Object::cast_to<Node3D>(p_node);
    if (node3d != nullptr && prim && prim.IsA<pxr::UsdGeomXformable>()) {
        Transform3D t = node3d->get_transform();
        if (!extra_scale.is_equal_approx(Vector3(1, 1, 1))) {
            t.basis = t.basis.scaled(extra_scale);
        }
        if (!t.is_equal_approx(Transform3D())) {
            pxr::UsdGeomXformable(prim).AddTransformOp().Set(to_gf_matrix(t));
        }
    }

    for (int i = 0; i < p_node->get_child_count(); i++) {
        Node *child = p_node->get_child(i);
        const pxr::SdfPath child_path = p_prim_path.AppendChild(pxr::TfToken(sanitize_prim_name(child->get_name()).utf8().get_data()));
        export_node(child, p_stage, child_path, p_save_path);
    }
}

} // namespace

bool UsdResourceFormatSaver::_recognize(const Ref<Resource> &p_resource) const {
    return Ref<PackedScene>(p_resource).is_valid();
}

PackedStringArray UsdResourceFormatSaver::_get_recognized_extensions(const Ref<Resource> &p_resource) const {
    PackedStringArray exts;
    if (Ref<PackedScene>(p_resource).is_valid()) {
        for (const char *x : USD_EXTS) {
            exts.push_back(x);
        }
    }
    return exts;
}

bool UsdResourceFormatSaver::_recognize_path(const Ref<Resource> &p_resource, const String &p_path) const {
    return is_usd_ext(p_path.get_extension());
}

Error UsdResourceFormatSaver::_save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
    Ref<PackedScene> packed = p_resource;
    if (packed.is_null()) {
        return ERR_INVALID_PARAMETER;
    }

    Node *root = packed->instantiate();
    if (root == nullptr) {
        UtilityFunctions::printerr("USD save: could not instantiate PackedScene for ", p_path);
        return ERR_CANT_CREATE;
    }

    // If this scene was loaded from USD (loader stamped usd:source_path), take the source-preserving
    // path: copy the original layer and author only supported edits. Otherwise export an authored
    // scene to a fresh layer.
    const String source_path = godot_usd::get_usd_metadata(root).get("usd:source_path", String());
    if (!source_path.is_empty()) {
        const Error sp_result = save_source_preserving(root, source_path, p_path);
        root->queue_free();
        return sp_result;
    }

    String abs_path = p_path;
    if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
        abs_path = ProjectSettings::get_singleton()->globalize_path(p_path);
    }

    Error result = OK;
    try {
        pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateNew(abs_path.utf8().get_data());
        if (!stage) {
            UtilityFunctions::printerr("USD save: UsdStage::CreateNew failed for ", p_path);
            root->queue_free();
            return ERR_CANT_CREATE;
        }

        const pxr::SdfPath root_path("/" + std::string(sanitize_prim_name(root->get_name()).utf8().get_data()));
        export_node(root, stage, root_path, p_path);

        pxr::UsdPrim root_prim = stage->GetPrimAtPath(root_path);
        if (root_prim) {
            stage->SetDefaultPrim(root_prim);
        }
        stage->GetRootLayer()->Save();
    } catch (const std::exception &e) {
        UtilityFunctions::printerr("USD save: exception: ", e.what());
        result = ERR_CANT_CREATE;
    }

    root->queue_free();
    return result;
}

} // namespace godot
