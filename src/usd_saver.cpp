#include "usd_saver.h"
#include "usd_mesh_export_helper.h"
#include "usd_materials.h"   // transplanted: write_preview_material

#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/transform3d.hpp>

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/cylinder.h>
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
        prim = export_mesh_prim(mesh_instance->get_mesh(), p_stage, p_prim_path, &extra_scale);

        // Bind surface 0's material as a UsdPreviewSurface network (transplanted writer).
        Ref<Material> material = mesh_instance->get_surface_override_material(0);
        if (material.is_null()) {
            material = mesh_instance->get_mesh()->surface_get_material(0);
        }
        if (prim && material.is_valid()) {
            pxr::UsdShadeMaterial usd_material;
            if (godot_usd::write_preview_material(p_stage, material, p_prim_path, p_save_path, "Material", &usd_material) && usd_material) {
                pxr::UsdShadeMaterialBindingAPI::Apply(prim).Bind(usd_material);
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
