#include "usd_document.h"
#include "usd_state.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/primitive_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/light3d.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>

// USD headers
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/cube.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdLux/sphereLight.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace godot {

void UsdDocument::_bind_methods() {
    ClassDB::bind_method(D_METHOD("append_from_scene", "scene_root", "state", "flags"), &UsdDocument::append_from_scene, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("write_to_filesystem", "state", "path"), &UsdDocument::write_to_filesystem);
    ClassDB::bind_method(D_METHOD("get_file_extension_for_format", "binary"), &UsdDocument::get_file_extension_for_format);
}

UsdDocument::UsdDocument() {
    // Initialize USD-specific members
    // This will be expanded as we implement the USD integration
}

Error UsdDocument::append_from_scene(Node *p_scene_root, Ref<UsdState> p_state, int32_t p_flags) {
    // This method will convert the Godot scene to USD
    
    if (!p_scene_root) {
        UtilityFunctions::printerr("USD Export: No scene root provided");
        return ERR_INVALID_PARAMETER;
    }
    
    if (p_state.is_null()) {
        UtilityFunctions::printerr("USD Export: No state provided");
        return ERR_INVALID_PARAMETER;
    }
    
    UtilityFunctions::print("USD Export: Appending scene ", p_scene_root->get_name(), " to USD document");
    
    try {
        // Create a new USD stage in memory
        UsdStageRefPtr stage = UsdStage::CreateInMemory();
        if (!stage) {
            UtilityFunctions::printerr("USD Export: Failed to create USD stage");
            return ERR_CANT_CREATE;
        }
        
        // Set up the stage
        stage->SetStartTimeCode(1);
        stage->SetEndTimeCode(1);
        stage->SetTimeCodesPerSecond(p_state->get_bake_fps());
        
        // Create a root prim for the scene
        UsdGeomXform root = UsdGeomXform::Define(stage, SdfPath("/Root"));
        stage->SetDefaultPrim(root.GetPrim());
        
        // Add metadata (using custom layer data for copyright)
        if (!p_state->get_copyright().is_empty()) {
            auto rootLayer = stage->GetRootLayer();
            auto customData = rootLayer->GetCustomLayerData();
            customData["copyright"] = VtValue(std::string(p_state->get_copyright().utf8().get_data()));
            rootLayer->SetCustomLayerData(customData);
        }
        
        // Store the stage in the state for later use in write_to_filesystem
        p_state->set_stage(stage);
        
        // Traverse the scene and convert nodes to USD prims
        _convert_node_to_prim(p_scene_root, stage, SdfPath("/Root"), p_state);
        
        return OK;
    } catch (const std::exception& e) {
        UtilityFunctions::printerr("USD Export: Exception occurred: ", e.what());
        return ERR_CANT_CREATE;
    }
}

void UsdDocument::_convert_node_to_prim(Node *p_node, UsdStageRefPtr p_stage, const SdfPath &p_parent_path, Ref<UsdState> p_state) {
    if (!p_node) {
        return;
    }
    
    // Get node information
    String node_type = p_node->get_class();
    String node_name = p_node->get_name();
    
    // Create a valid USD prim name from the node name
    // Replace invalid characters with underscores
    std::string prim_name = node_name.utf8().get_data();
    for (char &c : prim_name) {
        if (!isalnum(c) && c != '_') {
            c = '_';
        }
    }
    
    // Ensure the prim name starts with a letter or underscore
    if (!prim_name.empty() && !isalpha(prim_name[0]) && prim_name[0] != '_') {
        prim_name = "_" + prim_name;
    }
    
    // If the prim name is empty, use a default name
    if (prim_name.empty()) {
        prim_name = "node";
    }
    
    // Create the prim path
    SdfPath prim_path = p_parent_path.AppendChild(TfToken(prim_name));
    
    // Create the appropriate prim based on node type
    if (Object::cast_to<Node3D>(p_node)) {
        Node3D *node_3d = Object::cast_to<Node3D>(p_node);
        
        // Create an Xform prim for the Node3D
        UsdGeomXform xform = UsdGeomXform::Define(p_stage, prim_path);
        
        // Set the transform
        Transform3D transform = node_3d->get_transform();
        
        // Convert Godot transform to USD matrix
        // Initialize with identity matrix to ensure all values are properly set
        GfMatrix4d matrix(1.0);
        
        // Extract basis (rotation and scale)
        Basis basis = transform.get_basis();
        Vector3 x_axis = basis.get_column(0);
        Vector3 y_axis = basis.get_column(1);
        Vector3 z_axis = basis.get_column(2);
        
        // Set the rotation and scale components (first 3x3 submatrix)
        matrix[0][0] = x_axis.x;
        matrix[0][1] = x_axis.y;
        matrix[0][2] = x_axis.z;
        matrix[0][3] = 0.0;  // Explicitly set to 0
        
        matrix[1][0] = y_axis.x;
        matrix[1][1] = y_axis.y;
        matrix[1][2] = y_axis.z;
        matrix[1][3] = 0.0;  // Explicitly set to 0
        
        matrix[2][0] = z_axis.x;
        matrix[2][1] = z_axis.y;
        matrix[2][2] = z_axis.z;
        matrix[2][3] = 0.0;  // Explicitly set to 0
        
        // Extract translation
        Vector3 origin = transform.get_origin();
        matrix[3][0] = origin.x;
        matrix[3][1] = origin.y;
        matrix[3][2] = origin.z;
        matrix[3][3] = 1.0;  // Homogeneous coordinate w=1
        
        // Set the matrix
        UsdGeomXformable xformable(xform);
        UsdGeomXformOp transformOp = xformable.AddTransformOp();
        transformOp.Set(matrix);
        
        // Handle specific node types
        if (Object::cast_to<MeshInstance3D>(p_node)) {
            MeshInstance3D *mesh_instance = Object::cast_to<MeshInstance3D>(p_node);
            Ref<Mesh> mesh = mesh_instance->get_mesh();
            
            if (mesh.is_valid()) {
                // Create a mesh prim
                UsdGeomMesh usd_mesh = UsdGeomMesh::Define(p_stage, prim_path.AppendChild(TfToken("Mesh")));
                
                // Get mesh data
                int surface_count = mesh->get_surface_count();
                
                if (surface_count > 0) {
                    // For simplicity, we'll just export the first surface
                    // In a full implementation, we would handle multiple surfaces
                    
                    // Get the mesh arrays for the first surface
                    Array arrays = mesh->surface_get_arrays(0);
                    
                    // Check if we have valid arrays
                    if (arrays.size() >= Mesh::ARRAY_MAX) {
                        // Get vertices
                        PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
                        
                        // Get indices (triangles)
                        PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
                        
                        // Get normals
                        PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
                        
                        // Get UVs
                        PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
                        
                        // Convert vertices to USD format
                        VtArray<GfVec3f> usd_points;
                        usd_points.reserve(vertices.size());
                        
                        for (int i = 0; i < vertices.size(); i++) {
                            Vector3 v = vertices[i];
                            usd_points.push_back(GfVec3f(v.x, v.y, v.z));
                        }
                        
                        // Set points
                        usd_mesh.GetPointsAttr().Set(usd_points);
                        
                        // Convert indices to USD format
                        // USD uses face counts and face indices
                        // Face counts is the number of vertices per face (3 for triangles)
                        // Face indices is the list of vertex indices
                        
                        // For triangles, each face has 3 vertices
                        VtArray<int> face_vertex_counts;
                        VtArray<int> face_vertex_indices;
                        
                        // If we have indices, use them
                        if (indices.size() > 0) {
                            // Assuming triangles
                            int triangle_count = indices.size() / 3;
                            face_vertex_counts.reserve(triangle_count);
                            face_vertex_indices.reserve(indices.size());
                            
                            for (int i = 0; i < triangle_count; i++) {
                                face_vertex_counts.push_back(3); // 3 vertices per triangle
                                
                                // Add the 3 indices for this triangle
                                face_vertex_indices.push_back(indices[i * 3]);
                                face_vertex_indices.push_back(indices[i * 3 + 1]);
                                face_vertex_indices.push_back(indices[i * 3 + 2]);
                            }
                        } else {
                            // If we don't have indices, create triangles from vertices
                            // Assuming vertices are already arranged as triangles
                            int triangle_count = vertices.size() / 3;
                            face_vertex_counts.reserve(triangle_count);
                            face_vertex_indices.reserve(vertices.size());
                            
                            for (int i = 0; i < triangle_count; i++) {
                                face_vertex_counts.push_back(3); // 3 vertices per triangle
                                
                                // Add the 3 indices for this triangle
                                face_vertex_indices.push_back(i * 3);
                                face_vertex_indices.push_back(i * 3 + 1);
                                face_vertex_indices.push_back(i * 3 + 2);
                            }
                        }
                        
                        // Set face counts and indices
                        usd_mesh.GetFaceVertexCountsAttr().Set(face_vertex_counts);
                        usd_mesh.GetFaceVertexIndicesAttr().Set(face_vertex_indices);
                        
                        // Set normals if available
                        if (normals.size() > 0) {
                            VtArray<GfVec3f> usd_normals;
                            usd_normals.reserve(normals.size());
                            
                            for (int i = 0; i < normals.size(); i++) {
                                Vector3 n = normals[i];
                                usd_normals.push_back(GfVec3f(n.x, n.y, n.z));
                            }
                            
                            usd_mesh.GetNormalsAttr().Set(usd_normals);
                            
                            // Set interpolation to "vertex" for per-vertex normals
                            usd_mesh.SetNormalsInterpolation(UsdGeomTokens->vertex);
                        }
                        
                        // Set UVs if available
                        if (uvs.size() > 0) {
                            VtArray<GfVec2f> usd_uvs;
                            usd_uvs.reserve(uvs.size());
                            
                            for (int i = 0; i < uvs.size(); i++) {
                                Vector2 uv = uvs[i];
                                usd_uvs.push_back(GfVec2f(uv.x, uv.y));
                            }
                            
                            // For now, we'll skip setting UVs as it requires additional USD API knowledge
                            // In a full implementation, we would set UVs using the appropriate USD API
                            UtilityFunctions::print("USD Export: UVs available but not exported for ", node_name);
                        }
                        
                        UtilityFunctions::print("USD Export: Added mesh with ", String::num_int64(vertices.size()), " vertices and ", String::num_int64(face_vertex_counts.size()), " faces for ", node_name);
                    } else {
                        // Fallback to a cube if we don't have valid arrays
                        UsdGeomCube cube = UsdGeomCube::Define(p_stage, prim_path.AppendChild(TfToken("Mesh")));
                        cube.GetSizeAttr().Set(1.0);
                        UtilityFunctions::print("USD Export: Added cube as fallback for ", node_name);
                    }
                } else {
                    // Fallback to a cube if we don't have any surfaces
                    UsdGeomCube cube = UsdGeomCube::Define(p_stage, prim_path.AppendChild(TfToken("Mesh")));
                    cube.GetSizeAttr().Set(1.0);
                    UtilityFunctions::print("USD Export: Added cube as fallback for ", node_name);
                }
            }
        } else if (Object::cast_to<Camera3D>(p_node)) {
            Camera3D *camera = Object::cast_to<Camera3D>(p_node);
            
            // Create a camera prim
            UsdGeomCamera usd_camera = UsdGeomCamera::Define(p_stage, prim_path.AppendChild(TfToken("Camera")));
            
            // Set camera properties
            float fov = camera->get_fov();
            float aspect_ratio = 1.0; // Default aspect ratio
            
            // Convert FOV to focal length (approximation)
            float focal_length = 0.5 / tan(fov * 0.5 * (M_PI / 180.0));
            
            // Set the focal length
            usd_camera.GetFocalLengthAttr().Set(focal_length);
            
            // Set the horizontal aperture (35mm full aperture is 24mm)
            usd_camera.GetHorizontalApertureAttr().Set(24.0);
            
            UtilityFunctions::print("USD Export: Added camera for ", node_name);
        } else if (Object::cast_to<Light3D>(p_node)) {
            Light3D *light = Object::cast_to<Light3D>(p_node);
            
            // Create a light prim (using a sphere light as a placeholder)
            UsdLuxSphereLight usd_light = UsdLuxSphereLight::Define(p_stage, prim_path.AppendChild(TfToken("Light")));
            
            // Set light properties
            float energy = light->get_param(Light3D::PARAM_ENERGY);
            
            // Set the intensity
            usd_light.GetIntensityAttr().Set(energy);
            
            UtilityFunctions::print("USD Export: Added light for ", node_name);
        }
    }
    
    // Recursively process children
    for (int i = 0; i < p_node->get_child_count(); i++) {
        _convert_node_to_prim(p_node->get_child(i), p_stage, prim_path, p_state);
    }
}

Error UsdDocument::write_to_filesystem(Ref<UsdState> p_state, const String &p_path) {
    // This method will write the USD document to a file using the USD API
    
    if (p_state.is_null()) {
        UtilityFunctions::printerr("USD Export: No state provided");
        return ERR_INVALID_PARAMETER;
    }
    
    if (p_path.is_empty()) {
        UtilityFunctions::printerr("USD Export: No file path provided");
        return ERR_INVALID_PARAMETER;
    }
    
    UtilityFunctions::print("USD Export: Writing USD document to ", p_path);
    
    try {
        // Get the stage from the state
        UsdStageRefPtr stage = p_state->get_stage();
        if (!stage) {
            UtilityFunctions::printerr("USD Export: No stage found in state");
            return ERR_INVALID_PARAMETER;
        }
        
        // Export the stage to the file
        stage->Export(p_path.utf8().get_data());
        
        UtilityFunctions::print("USD Export: Successfully exported scene to ", p_path);
        return OK;
    } catch (const std::exception& e) {
        UtilityFunctions::printerr("USD Export: Exception occurred: ", e.what());
        return ERR_CANT_CREATE;
    }
}

String UsdDocument::get_file_extension_for_format(bool p_binary) const {
    // Return the appropriate file extension based on the format
    return p_binary ? "usdc" : "usda";
}

}
