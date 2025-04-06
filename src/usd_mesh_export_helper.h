#ifndef USD_MESH_EXPORT_HELPER_H
#define USD_MESH_EXPORT_HELPER_H

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/primitive_mesh.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/capsule_mesh.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/gprim.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/sphere.h>
#include <pxr/usd/usdGeom/cone.h>
#include <pxr/usd/usdGeom/cylinder.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace godot {

class UsdMeshExportHelper {
public:
    UsdMeshExportHelper();
    ~UsdMeshExportHelper();

    // Export a Godot mesh to a USD prim
    pxr::UsdPrim export_mesh_to_prim(const Ref<Mesh> p_mesh, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path);

private:
    // Helper methods for specific primitive types
    pxr::UsdGeomCube export_box(const Ref<BoxMesh> p_box, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path);
    pxr::UsdGeomSphere export_sphere(const Ref<SphereMesh> p_sphere, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path);
    pxr::UsdGeomCylinder export_cylinder(const Ref<CylinderMesh> p_cylinder, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path);
    pxr::UsdGeomCone export_cone(const Ref<CylinderMesh> p_cone, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path);
    pxr::UsdGeomCapsule export_capsule(const Ref<CapsuleMesh> p_capsule, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path);
    pxr::UsdGeomMesh export_geom_mesh(const Ref<Mesh> p_mesh, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path);

    // Helper method to handle non-uniform scaling
    void apply_non_uniform_scale(pxr::UsdGeomGprim& p_gprim, const pxr::GfVec3f& p_scale);
};

} // namespace godot

#endif // USD_MESH_EXPORT_HELPER_H
