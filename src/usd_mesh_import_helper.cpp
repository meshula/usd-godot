#include "usd_mesh_import_helper.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

UsdMeshImportHelper::UsdMeshImportHelper() {
    // Constructor
}

UsdMeshImportHelper::~UsdMeshImportHelper() {
    // Destructor
}

Ref<Mesh> UsdMeshImportHelper::import_mesh_from_prim(const pxr::UsdPrim& p_prim) {
    // Check if the prim is valid
    if (!p_prim.IsValid()) {
        UtilityFunctions::printerr("USD Import: Invalid prim");
        return Ref<Mesh>();
    }

    // Check if the prim is a geometric prim
    if (!p_prim.IsA<pxr::UsdGeomGprim>()) {
        UtilityFunctions::printerr("USD Import: Prim is not a geometric prim");
        return Ref<Mesh>();
    }

    // Handle different primitive types
    if (p_prim.IsA<pxr::UsdGeomCube>()) {
        pxr::UsdGeomCube cube(p_prim);
        return import_cube(cube);
    } else if (p_prim.IsA<pxr::UsdGeomSphere>()) {
        pxr::UsdGeomSphere sphere(p_prim);
        return import_sphere(sphere);
    } else if (p_prim.IsA<pxr::UsdGeomCylinder>()) {
        pxr::UsdGeomCylinder cylinder(p_prim);
        return import_cylinder(cylinder);
    } else if (p_prim.IsA<pxr::UsdGeomCone>()) {
        pxr::UsdGeomCone cone(p_prim);
        return import_cone(cone);
    } else if (p_prim.IsA<pxr::UsdGeomCapsule>()) {
        pxr::UsdGeomCapsule capsule(p_prim);
        return import_capsule(capsule);
    } else if (p_prim.IsA<pxr::UsdGeomMesh>()) {
        pxr::UsdGeomMesh mesh(p_prim);
        return import_generic_mesh(mesh);
    }

    // Fallback for unsupported primitive types
    UtilityFunctions::printerr("USD Import: Unsupported primitive type");
    return Ref<Mesh>();
}

Ref<BoxMesh> UsdMeshImportHelper::import_cube(const pxr::UsdGeomCube& p_cube) {
    // Create a new BoxMesh
    Ref<BoxMesh> box_mesh;
    box_mesh.instantiate();

    // Get the size from the USD cube (USD uses half-extent, Godot uses full size)
    double size = 1.0;
    p_cube.GetSizeAttr().Get(&size);
    
    // Set the size (multiply by 2 to convert from half-extent to full size)
    box_mesh->set_size(Vector3(size * 2.0, size * 2.0, size * 2.0));

    UtilityFunctions::print("USD Import: Imported cube with size ", size * 2.0);
    
    return box_mesh;
}

Ref<SphereMesh> UsdMeshImportHelper::import_sphere(const pxr::UsdGeomSphere& p_sphere) {
    // Create a new SphereMesh
    Ref<SphereMesh> sphere_mesh;
    sphere_mesh.instantiate();

    // Get the radius from the USD sphere
    double radius = 1.0;
    p_sphere.GetRadiusAttr().Get(&radius);
    
    // Set the radius
    sphere_mesh->set_radius(radius);
    sphere_mesh->set_height(radius * 2.0); // Height is diameter in Godot

    UtilityFunctions::print("USD Import: Imported sphere with radius ", radius);
    
    return sphere_mesh;
}

Ref<CylinderMesh> UsdMeshImportHelper::import_cylinder(const pxr::UsdGeomCylinder& p_cylinder) {
    // Create a new CylinderMesh
    Ref<CylinderMesh> cylinder_mesh;
    cylinder_mesh.instantiate();

    // Get the radius and height from the USD cylinder
    double radius = 1.0;
    double height = 2.0;
    p_cylinder.GetRadiusAttr().Get(&radius);
    p_cylinder.GetHeightAttr().Get(&height);
    
    // Set the radius and height
    cylinder_mesh->set_top_radius(radius);
    cylinder_mesh->set_bottom_radius(radius);
    cylinder_mesh->set_height(height);

    UtilityFunctions::print("USD Import: Imported cylinder with radius ", radius, " and height ", height);
    
    return cylinder_mesh;
}

Ref<CylinderMesh> UsdMeshImportHelper::import_cone(const pxr::UsdGeomCone& p_cone) {
    // Create a new CylinderMesh (Godot uses CylinderMesh for cones too)
    Ref<CylinderMesh> cone_mesh;
    cone_mesh.instantiate();

    // Get the radius and height from the USD cone
    double radius = 1.0;
    double height = 2.0;
    p_cone.GetRadiusAttr().Get(&radius);
    p_cone.GetHeightAttr().Get(&height);
    
    // Set the radius and height (top radius = 0 for a cone)
    cone_mesh->set_top_radius(0.0);
    cone_mesh->set_bottom_radius(radius);
    cone_mesh->set_height(height);

    UtilityFunctions::print("USD Import: Imported cone with radius ", radius, " and height ", height);
    
    return cone_mesh;
}

Ref<CapsuleMesh> UsdMeshImportHelper::import_capsule(const pxr::UsdGeomCapsule& p_capsule) {
    // Create a new CapsuleMesh
    Ref<CapsuleMesh> capsule_mesh;
    capsule_mesh.instantiate();

    // Get the radius and height from the USD capsule
    double radius = 1.0;
    double height = 2.0;
    p_capsule.GetRadiusAttr().Get(&radius);
    p_capsule.GetHeightAttr().Get(&height);
    
    // Set the radius and height
    capsule_mesh->set_radius(radius);
    capsule_mesh->set_height(height);

    UtilityFunctions::print("USD Import: Imported capsule with radius ", radius, " and height ", height);
    
    return capsule_mesh;
}

Ref<Mesh> UsdMeshImportHelper::import_generic_mesh(const pxr::UsdGeomMesh& p_mesh) {
    // This is a stub implementation for now
    // In a full implementation, we would convert the USD mesh to a Godot mesh
    
    UtilityFunctions::print("USD Import: Generic mesh import not yet implemented");
    
    // Return an empty mesh for now
    Ref<Mesh> mesh;
    return mesh;
}

void UsdMeshImportHelper::apply_non_uniform_scale(Ref<Mesh> p_mesh, const pxr::GfVec3f& p_scale) {
    // This is a stub implementation for now
    // In a full implementation, we would apply non-uniform scaling to the mesh
    
    UtilityFunctions::print("USD Import: Non-uniform scale application not yet implemented");
}

} // namespace godot
