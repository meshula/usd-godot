#include "usd_mesh_import_helper.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <pxr/usd/usdGeom/primvarsAPI.h>

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
        return import_cube(p_prim);
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
        return import_geom_mesh(mesh);
    }

    // Fallback for unsupported primitive types
    UtilityFunctions::printerr("USD Import: Unsupported primitive type");
    return Ref<Mesh>();
}

Ref<BoxMesh> UsdMeshImportHelper::import_cube(const UsdPrim &p_prim) {
    // Create a new BoxMesh
    Ref<BoxMesh> box_mesh;
    box_mesh.instantiate();

    UsdGeomCube usdCube(p_prim);
    double size = 2.0; // Default size (2.0 because UsdGeomCube size is the half-extent)
    if (usdCube) {
        UsdAttribute sizeAttr = usdCube.GetSizeAttr();
        if (sizeAttr) {
            sizeAttr.Get(&size);
            UtilityFunctions::print("USD Import: Cube size from USD: ", size);
        } else {
            UtilityFunctions::print("USD Import: No size attribute found for cube, using default size");
        }
    }
    
    box_mesh->set_size(Vector3(size, size, size));
    return box_mesh;
}

Ref<StandardMaterial3D> UsdMeshImportHelper::create_material(const UsdPrim &p_prim) {
    Ref<StandardMaterial3D> stdMaterial;
    // Check for displayColor attribute
    UsdPrim usdPrim(p_prim);
    if (usdPrim.HasAttribute(TfToken("primvars:displayColor"))) {
        // Get the display color
        UsdAttribute displayColorAttr = usdPrim.GetAttribute(TfToken("primvars:displayColor"));
        VtArray<GfVec3f> displayColors;
        if (displayColorAttr.Get(&displayColors) && !displayColors.empty()) {
            // Check for colorSpace metadata
            std::string colorSpace = "srgb";
            // In the example, colorSpace is a metadata on the displayColor attribute
            TfToken colorSpaceToken("colorSpace");
            if (displayColorAttr.HasMetadata(colorSpaceToken)) {
                std::string colorSpaceValue;
                displayColorAttr.GetMetadata(colorSpaceToken, &colorSpaceValue);
                colorSpace = colorSpaceValue;
            }
            // Create a simple material
            Ref<Material> material;
            material.instantiate();
            
            // Set the albedo color from the displayColor
            GfVec3f color = displayColors[0];

            // Create a StandardMaterial3D
            stdMaterial.instantiate();
            
            // Set the albedo color from the displayColor
            Color godotColor(color[0], color[1], color[2]);
            stdMaterial->set_albedo(godotColor);
        }
    }
    return stdMaterial;
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

// Helper to get the value of a primvar at a specific index
static bool _GetPrimvarVec3fAtIndex(const UsdGeomPrimvar& primvar, int index, GfVec3f* out) {
    VtArray<GfVec3f> array;
    if (!primvar.Get(&array) || index >= array.size()) return false;
    *out = array[index];
    return true;
}

static bool _GetPrimvarVec2fAtIndex(const UsdGeomPrimvar& primvar, int index, GfVec2f* out) {
    VtArray<GfVec2f> array;
    if (!primvar.Get(&array) || index >= array.size()) return false;
    *out = array[index];
    return true;
}

static bool _GetPrimvarColorAtIndex(const UsdGeomPrimvar& primvar, int index, GfVec4f* out) {
    VtArray<GfVec4f> array4;
    if (primvar.Get(&array4) && index < array4.size()) {
        *out = array4[index];
        return true;
    }
    VtArray<GfVec3f> array3;
    if (primvar.Get(&array3) && index < array3.size()) {
        GfVec3f v = array3[index];
        *out = GfVec4f(v[0], v[1], v[2], 1.0f);
        return true;
    }
    return false;
}

// Helper to resolve USD interpolation mode
static pxr::TfToken _GetInterpolation(const pxr::UsdAttribute &attr) {
    pxr::TfToken interp;
    if (!attr.GetMetadata(pxr::UsdGeomTokens->interpolation, &interp))
        return pxr::UsdGeomTokens->vertex; // fallback
    return interp;
}


Ref<Mesh> UsdMeshImportHelper::import_geom_mesh(const UsdGeomMesh& mesh) {
    VtArray<GfVec3f> points;
    if (!mesh.GetPointsAttr().Get(&points))
        return Ref<Mesh>();

    VtArray<int> faceVertexCounts;
    VtArray<int> faceVertexIndices;
    if (!mesh.GetFaceVertexCountsAttr().Get(&faceVertexCounts) ||
        !mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices))
        return Ref<ArrayMesh>();

    const UsdGeomPrimvarsAPI primvars(mesh);
    UsdGeomPrimvar normalPrimvar = primvars.GetPrimvar(TfToken("normals"));
    UsdGeomPrimvar uvPrimvar = primvars.GetPrimvar(TfToken("st"));
    UsdGeomPrimvar colorPrimvar = primvars.GetPrimvar(TfToken("displayColor"));

    // Get interpolation tokens
    TfToken normalInterp = normalPrimvar.IsDefined() ? normalPrimvar.GetInterpolation() : UsdGeomTokens->vertex;
    TfToken uvInterp = uvPrimvar.IsDefined() ? uvPrimvar.GetInterpolation() : UsdGeomTokens->vertex;
    TfToken colorInterp = colorPrimvar.IsDefined() ? colorPrimvar.GetInterpolation() : UsdGeomTokens->vertex;

    PackedVector3Array vertices;
    PackedVector3Array normals;
    PackedVector2Array uvs;
    PackedColorArray colors;
    PackedInt32Array indices;

    int vertexOffset = 0;
    int triangleIndex = 0;

    for (size_t face = 0; face < faceVertexCounts.size(); ++face) {
        int vertexCount = faceVertexCounts[face];
        if (vertexCount < 3) continue; // Skip degenerate faces

        for (int i = 0; i < vertexCount - 2; ++i) {
            int idx0 = faceVertexIndices[vertexOffset];
            int idx1 = faceVertexIndices[vertexOffset + i + 1];
            int idx2 = faceVertexIndices[vertexOffset + i + 2];

            int triIndices[3] = { idx2, idx1, idx0 };

            for (int corner = 0; corner < 3; ++corner) {
                int usdIndex = triIndices[corner];
                vertices.append(Vector3(points[usdIndex][0], points[usdIndex][1], points[usdIndex][2]));
                indices.append(vertices.size() - 1);

                // Normals
                GfVec3f n;
                int normalIndex =
                    normalInterp == UsdGeomTokens->constant ? 0 :
                    normalInterp == UsdGeomTokens->uniform ? face :
                    normalInterp == UsdGeomTokens->vertex ? usdIndex :
                    normalInterp == UsdGeomTokens->faceVarying ? triangleIndex * 3 + corner : -1;
                if (normalIndex >= 0 && _GetPrimvarVec3fAtIndex(normalPrimvar, normalIndex, &n))
                    normals.append(Vector3(n[0], n[1], n[2]));
                else
                    normals.append(Vector3());

                // UVs
                GfVec2f uv;
                int uvIndex =
                    uvInterp == UsdGeomTokens->constant ? 0 :
                    uvInterp == UsdGeomTokens->uniform ? face :
                    uvInterp == UsdGeomTokens->vertex ? usdIndex :
                    uvInterp == UsdGeomTokens->faceVarying ? triangleIndex * 3 + corner : -1;
                if (uvIndex >= 0 && _GetPrimvarVec2fAtIndex(uvPrimvar, uvIndex, &uv))
                    uvs.append(Vector2(uv[0], 1.0f - uv[1]));

                // Vertex color
                GfVec4f c;
                int colorIndex =
                    colorInterp == UsdGeomTokens->constant ? 0 :
                    colorInterp == UsdGeomTokens->uniform ? face :
                    colorInterp == UsdGeomTokens->vertex ? usdIndex :
                    colorInterp == UsdGeomTokens->faceVarying ? triangleIndex * 3 + corner : -1;
                if (colorIndex >= 0 && _GetPrimvarColorAtIndex(colorPrimvar, colorIndex, &c))
                    colors.append(Color(c[0], c[1], c[2], c[3]));
            }
            ++triangleIndex;
        }
        vertexOffset += vertexCount;
    }

    // Synthesize weighted normals if original normals were missing
    bool had_normals = false;
    for (int i = 0; i < normals.size(); ++i) {
        if (normals[i].length_squared() > 1e-5f) {
            had_normals = true;
            break;
        }
    }

    if (!had_normals) {
        normals.resize(vertices.size());
        for (int i = 0; i < normals.size(); ++i)
            normals[i] = Vector3(0, 0, 0);

        for (int i = 0; i < indices.size(); i += 3) {
            int i0 = indices[i];
            int i1 = indices[i + 1];
            int i2 = indices[i + 2];

            Vector3 v0 = vertices[i0];
            Vector3 v1 = vertices[i1];
            Vector3 v2 = vertices[i2];

            Vector3 e1 = v1 - v0;
            Vector3 e2 = v2 - v0;
            Vector3 face_normal = e1.cross(e2);
            float area = face_normal.length() * 0.5f;
            face_normal = face_normal.normalized();

            normals[i0] += face_normal * area;
            normals[i1] += face_normal * area;
            normals[i2] += face_normal * area;
        }

        for (int i = 0; i < normals.size(); ++i)
            normals[i] = normals[i].normalized();
    }

    // Ensure consistent attribute sizes
    int vertex_count = vertices.size();

    if (uvs.size() != vertex_count) {
        uvs.resize(vertex_count);
        for (int i = 0; i < vertex_count; ++i)
            uvs[i] = Vector2();
    }

    if (colors.size() != vertex_count) {
        colors.resize(vertex_count);
        for (int i = 0; i < vertex_count; ++i)
            colors[i] = Color(1, 1, 1, 1);
    }

    Array arrays;
    arrays.resize(Mesh::ARRAY_MAX);
    arrays[Mesh::ARRAY_VERTEX] = vertices;
    arrays[Mesh::ARRAY_NORMAL] = normals;
    arrays[Mesh::ARRAY_TEX_UV] = uvs;
    arrays[Mesh::ARRAY_COLOR] = colors;
    arrays[Mesh::ARRAY_INDEX] = indices;

    Ref<ArrayMesh> array_mesh = memnew(ArrayMesh);
    array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
    return array_mesh;
}


void UsdMeshImportHelper::apply_non_uniform_scale(Ref<Mesh> p_mesh, const pxr::GfVec3f& p_scale) {
    // This is a stub implementation for now
    // In a full implementation, we would apply non-uniform scaling to the mesh
    
    UtilityFunctions::print("USD Import: Non-uniform scale application not yet implemented");
}

} // namespace godot
