#include "usd_mesh_export_helper.h"
#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

UsdMeshExportHelper::UsdMeshExportHelper() {
    // Constructor
}

UsdMeshExportHelper::~UsdMeshExportHelper() {
    // Destructor
}

pxr::UsdPrim UsdMeshExportHelper::export_mesh_to_prim(const Ref<Mesh> p_mesh, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path) {
    // Check if the mesh is valid
    if (p_mesh.is_null()) {
        UtilityFunctions::printerr("USD Export: Invalid mesh");
        return pxr::UsdPrim();
    }

    // Check if the stage is valid
    if (!p_stage) {
        UtilityFunctions::printerr("USD Export: Invalid stage");
        return pxr::UsdPrim();
    }

    // Handle different mesh types
    if (p_mesh->get_class() == "BoxMesh") {
        Ref<BoxMesh> box_mesh = p_mesh;
        pxr::UsdGeomCube cube = export_box(box_mesh, p_stage, p_path);
        return cube.GetPrim();
    } else if (p_mesh->get_class() == "SphereMesh") {
        Ref<SphereMesh> sphere_mesh = p_mesh;
        pxr::UsdGeomSphere sphere = export_sphere(sphere_mesh, p_stage, p_path);
        return sphere.GetPrim();
    } else if (p_mesh->get_class() == "CylinderMesh") {
        Ref<CylinderMesh> cylinder_mesh = p_mesh;
        
        // Check if it's a cone (top radius = 0)
        if (cylinder_mesh->get_top_radius() < 0.0001) {
            pxr::UsdGeomCone cone = export_cone(cylinder_mesh, p_stage, p_path);
            return cone.GetPrim();
        } else {
            pxr::UsdGeomCylinder cylinder = export_cylinder(cylinder_mesh, p_stage, p_path);
            return cylinder.GetPrim();
        }
    } else if (p_mesh->get_class() == "CapsuleMesh") {
        Ref<CapsuleMesh> capsule_mesh = p_mesh;
        pxr::UsdGeomCapsule capsule = export_capsule(capsule_mesh, p_stage, p_path);
        return capsule.GetPrim();
    } else {
        // Generic mesh
        pxr::UsdGeomMesh mesh = export_geom_mesh(p_mesh, p_stage, p_path);
        return mesh.GetPrim();
    }

    // Fallback for unsupported mesh types
    UtilityFunctions::printerr("USD Export: Unsupported mesh type: ", p_mesh->get_class());
    return pxr::UsdPrim();
}

pxr::UsdGeomCube UsdMeshExportHelper::export_box(const Ref<BoxMesh> p_box, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path) {
    // Create a new USD cube
    pxr::UsdGeomCube cube = pxr::UsdGeomCube::Define(p_stage, p_path);
    
    // Get the size from the Godot box mesh
    Vector3 size = p_box->get_size(); // do not compensate the size, usd and godot agree
    cube.GetSizeAttr().Set((double) size.x);
    
    // If the box is not uniform, we need to apply non-uniform scaling
    if (size.x != size.y || size.x != size.z) {
        // Calculate the scale factors relative to the x dimension
        double scale_y = size.y / size.x;
        double scale_z = size.z / size.x;
        
        // Add a scale op to handle the non-uniform scaling
        pxr::UsdGeomXformOp scaleOp = cube.AddScaleOp();
        scaleOp.Set(pxr::GfVec3d(1.0, scale_y, scale_z));
        
        UtilityFunctions::print("USD Export: Applied non-uniform scale (1.0, ", scale_y, ", ", scale_z, ") to cube");
    }
    
    // Check if the box mesh has a material with a color
    Ref<godot::Material> material = p_box->get_material();
    if (material.is_valid()) {
        // Try to get the color from the material
        if (material->is_class("StandardMaterial3D")) {
            Ref<godot::StandardMaterial3D> std_material = material;
            Color color = std_material->get_albedo();
            
            // Create a displayColor attribute
            pxr::UsdAttribute displayColorAttr = cube.CreateDisplayColorPrimvar(pxr::UsdGeomTokens->constant);
            
            // Create a VtArray with a single color value
            pxr::VtArray<pxr::GfVec3f> displayColors;
            displayColors.push_back(pxr::GfVec3f(color.r, color.g, color.b));
            
            // Set the color value
            displayColorAttr.Set(displayColors);
            
            // Set the color space metadata to linear
            displayColorAttr.SetColorSpace(pxr::TfToken("linear"));
            
            UtilityFunctions::print("USD Export: Applied material color (", color.r, ", ", color.g, ", ", color.b, ") to cube");
        }
    }
    
    UtilityFunctions::print("USD Export: Exported box with size ", size);
    
    return cube;
}

pxr::UsdGeomSphere UsdMeshExportHelper::export_sphere(const Ref<SphereMesh> p_sphere, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path) {
    // Create a new USD sphere
    pxr::UsdGeomSphere sphere = pxr::UsdGeomSphere::Define(p_stage, p_path);
    
    // Get the radius from the Godot sphere mesh
    double radius = p_sphere->get_radius();
    
    // Set the radius
    sphere.GetRadiusAttr().Set(radius);
    
    UtilityFunctions::print("USD Export: Exported sphere with radius ", radius);
    
    return sphere;
}

pxr::UsdGeomCylinder UsdMeshExportHelper::export_cylinder(const Ref<CylinderMesh> p_cylinder, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path) {
    // Create a new USD cylinder
    pxr::UsdGeomCylinder cylinder = pxr::UsdGeomCylinder::Define(p_stage, p_path);
    
    // Get the radius and height from the Godot cylinder mesh
    double radius = p_cylinder->get_bottom_radius(); // Use bottom radius for now
    double height = p_cylinder->get_height();
    
    // Set the radius and height
    cylinder.GetRadiusAttr().Set(radius);
    cylinder.GetHeightAttr().Set(height);
    
    // Handle non-uniform scaling if top and bottom radii are different
    if (p_cylinder->get_top_radius() != p_cylinder->get_bottom_radius()) {
        UtilityFunctions::print("USD Export: Warning - USD cylinders don't support different top and bottom radii. Using bottom radius.");
    }
    
    UtilityFunctions::print("USD Export: Exported cylinder with radius ", radius, " and height ", height);
    
    return cylinder;
}

pxr::UsdGeomCone UsdMeshExportHelper::export_cone(const Ref<CylinderMesh> p_cone, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path) {
    // Create a new USD cone
    pxr::UsdGeomCone cone = pxr::UsdGeomCone::Define(p_stage, p_path);
    
    // Get the radius and height from the Godot cone mesh (which is a cylinder with top radius = 0)
    double radius = p_cone->get_bottom_radius();
    double height = p_cone->get_height();
    
    // Set the radius and height
    cone.GetRadiusAttr().Set(radius);
    cone.GetHeightAttr().Set(height);
    
    UtilityFunctions::print("USD Export: Exported cone with radius ", radius, " and height ", height);
    
    return cone;
}

pxr::UsdGeomCapsule UsdMeshExportHelper::export_capsule(const Ref<CapsuleMesh> p_capsule, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path) {
    // Create a new USD capsule
    pxr::UsdGeomCapsule capsule = pxr::UsdGeomCapsule::Define(p_stage, p_path);
    
    // Get the radius and height from the Godot capsule mesh
    double radius = p_capsule->get_radius();
    double height = p_capsule->get_height();
    
    // Set the radius and height
    capsule.GetRadiusAttr().Set(radius);
    capsule.GetHeightAttr().Set(height);
    
    UtilityFunctions::print("USD Export: Exported capsule with radius ", radius, " and height ", height);
    
    return capsule;
}

pxr::UsdGeomMesh UsdMeshExportHelper::export_geom_mesh(const Ref<Mesh> p_mesh, pxr::UsdStageRefPtr p_stage, const pxr::SdfPath& p_path) {
    // Create a new USD mesh
    pxr::UsdGeomMesh mesh = pxr::UsdGeomMesh::Define(p_stage, p_path);
    
    // Get the mesh data from Godot
    if (p_mesh.is_null()) {
        UtilityFunctions::printerr("USD Export: Invalid mesh");
        return mesh;
    }
    
    // Get the surface count
    int surface_count = p_mesh->get_surface_count();
    if (surface_count == 0) {
        UtilityFunctions::printerr("USD Export: Mesh has no surfaces");
        return mesh;
    }
    
    // For now, we'll just export the first surface
    // In a full implementation, we would handle multiple surfaces
    Array arrays = p_mesh->surface_get_arrays(0);
    if (arrays.size() == 0) {
        UtilityFunctions::printerr("USD Export: Failed to get surface arrays");
        return mesh;
    }
    
    // Get the vertices
    PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
    if (vertices.size() == 0) {
        UtilityFunctions::printerr("USD Export: Mesh has no vertices");
        return mesh;
    }
    
    // Get the indices
    PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
    bool has_indices = indices.size() > 0;
    
    // Get the normals
    PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
    bool has_normals = normals.size() > 0;
    
    // Get the UVs
    PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
    bool has_uvs = uvs.size() > 0;
    
    // Convert vertices to USD format
    pxr::VtArray<pxr::GfVec3f> usd_points;
    usd_points.reserve(vertices.size());
    for (int i = 0; i < vertices.size(); i++) {
        Vector3 v = vertices[i];
        usd_points.push_back(pxr::GfVec3f(v.x, v.y, v.z));
    }
    
    // Set the points attribute
    mesh.GetPointsAttr().Set(usd_points);
    
    // Convert indices to USD format
    if (has_indices) {
        // USD expects face vertex counts and face vertex indices
        // Face vertex counts is the number of vertices per face (e.g., 3 for triangles)
        // Face vertex indices is the list of vertex indices for each face
        
        // Assuming triangles for now
        int face_count = indices.size() / 3;
        
        pxr::VtArray<int> face_vertex_counts;
        face_vertex_counts.reserve(face_count);
        
        for (int i = 0; i < face_count; i++) {
            face_vertex_counts.push_back(3); // 3 vertices per triangle
        }
        
        pxr::VtArray<int> face_vertex_indices;
        face_vertex_indices.reserve(indices.size());
        
        for (int i = 0; i < indices.size(); i++) {
            face_vertex_indices.push_back(indices[i]);
        }
        
        // Set the face vertex counts and indices
        mesh.GetFaceVertexCountsAttr().Set(face_vertex_counts);
        mesh.GetFaceVertexIndicesAttr().Set(face_vertex_indices);
    } else {
        // If there are no indices, assume each set of 3 vertices forms a triangle
        int face_count = vertices.size() / 3;
        
        pxr::VtArray<int> face_vertex_counts;
        face_vertex_counts.reserve(face_count);
        
        for (int i = 0; i < face_count; i++) {
            face_vertex_counts.push_back(3); // 3 vertices per triangle
        }
        
        pxr::VtArray<int> face_vertex_indices;
        face_vertex_indices.reserve(vertices.size());
        
        for (int i = 0; i < vertices.size(); i++) {
            face_vertex_indices.push_back(i);
        }
        
        // Set the face vertex counts and indices
        mesh.GetFaceVertexCountsAttr().Set(face_vertex_counts);
        mesh.GetFaceVertexIndicesAttr().Set(face_vertex_indices);
    }
    
    // Convert normals to USD format
    if (has_normals) {
        pxr::VtArray<pxr::GfVec3f> usd_normals;
        usd_normals.reserve(normals.size());
        
        for (int i = 0; i < normals.size(); i++) {
            Vector3 n = normals[i];
            usd_normals.push_back(pxr::GfVec3f(n.x, n.y, n.z));
        }
        
        // Set the normals attribute
        mesh.GetNormalsAttr().Set(usd_normals);
        
        // Set the interpolation to "vertex" for per-vertex normals
        // Note: Normals are already per-vertex in USD by default
        mesh.SetNormalsInterpolation(pxr::UsdGeomTokens->vertex);
    }
    
    // Convert UVs to USD format
    if (has_uvs) {
        pxr::VtArray<pxr::GfVec2f> usd_uvs;
        usd_uvs.reserve(uvs.size());
        
        for (int i = 0; i < uvs.size(); i++) {
            Vector2 uv = uvs[i];
            usd_uvs.push_back(pxr::GfVec2f(uv.x, uv.y));
        }
        
        // Create a primvar for the UVs
        // Use the standard "st" name for texture coordinates
        pxr::UsdAttribute st_attr = mesh.GetPrim().CreateAttribute(
            pxr::TfToken("primvars:st"), 
            pxr::SdfValueTypeNames->Float2Array
        );
        
        // Set the UVs
        st_attr.Set(usd_uvs);
        
        // Set the interpolation to "vertex" for per-vertex UVs
        pxr::UsdAttribute st_interp_attr = mesh.GetPrim().CreateAttribute(
            pxr::TfToken("primvars:st:interpolation"),
            pxr::SdfValueTypeNames->Token
        );
        st_interp_attr.Set(pxr::UsdGeomTokens->vertex);
    }
    
    UtilityFunctions::print("USD Export: Exported mesh with ", vertices.size(), " vertices and ", (has_indices ? indices.size() / 3 : vertices.size() / 3), " triangles");
    
    return mesh;
}

void UsdMeshExportHelper::apply_non_uniform_scale(pxr::UsdGeomGprim& p_gprim, const pxr::GfVec3f& p_scale) {
    // This is a stub implementation for now
    // In a full implementation, we would apply non-uniform scaling to the mesh
    
    UtilityFunctions::print("USD Export: Non-uniform scale application not yet implemented");
}

} // namespace godot
