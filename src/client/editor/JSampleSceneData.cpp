#include "client/editor/JSampleSceneData.h"

J_EDITOR_BEGIN

namespace
{
	constexpr float PLANE_SIZE = 5000.0f;
	constexpr float PLANE_Y = -0.1f;
}

Engine::JSceneData MakeSampleSceneData()
{
	Engine::JSceneData sceneData;
	sceneData.id = "sample_scene";
	sceneData.name = "Sample Scene";
	sceneData.version = 1;

	Engine::JSceneMaterialData pbrMaterial;
	pbrMaterial.id = "mat_sample_pbr";
	pbrMaterial.name = "Sample PBR";
	pbrMaterial.shaderPath = "shader/base.hlsl";
	pbrMaterial.constants.enabled = true;
	pbrMaterial.constants.baseColor = JVec4(1.0f, 1.0f, 1.0f, 1.0f);
	pbrMaterial.constants.roughness = 0.5f;
	pbrMaterial.constants.metallic = 1.0f;
	pbrMaterial.textures.push_back({ "BaseTexture", "texture/Base_color.png" });
	pbrMaterial.textures.push_back({ "NormalTexture", "texture/normal.png" });
	pbrMaterial.textures.push_back({ "RoughnessTexture", "texture/roughness.png" });
	pbrMaterial.textures.push_back({ "MetallicTexture", "texture/metallic.png" });
	sceneData.materials.push_back(pbrMaterial);

	Engine::JSceneMaterialData gridMaterial;
	gridMaterial.id = "mat_grid";
	gridMaterial.name = "Grid";
	gridMaterial.shaderPath = "shader/grid.hlsl";
	gridMaterial.enableAlphaBlend = true;
	sceneData.materials.push_back(gridMaterial);

	Engine::JSceneMeshData gridMesh;
	gridMesh.id = "mesh_grid_plane";
	gridMesh.name = "Grid Plane Mesh";
	gridMesh.type = Engine::JSceneMeshType::Plane;
	gridMesh.plane.size = PLANE_SIZE;
	gridMesh.plane.y = PLANE_Y;
	sceneData.meshes.push_back(gridMesh);

	Engine::JSceneMeshData cupMesh;
	cupMesh.id = "mesh_cup";
	cupMesh.name = "Cup Mesh";
	cupMesh.type = Engine::JSceneMeshType::External;
	cupMesh.path = "mesh/Cup.fbx";
	sceneData.meshes.push_back(cupMesh);

	Engine::JSceneEntityData camera;
	camera.stableID = "ent_main_camera";
	camera.name = "Main Camera";
	camera.hasTransform = true;
	camera.transform.position = { 0.0f, 0.0f, -8.0f };
	camera.hasCamera = true;
	camera.camera.primary = true;
	camera.camera.moveSpeed = 6.0f;
	camera.camera.rotateSpeed = 1.0f;
	sceneData.entities.push_back(camera);

	Engine::JSceneEntityData light;
	light.stableID = "ent_key_light";
	light.name = "Key Light";
	light.hasTransform = true;
	light.transform.position = { 0.0f, 4.0f, -4.0f };
	light.hasLight = true;
	light.light.color = { 1.0f, 0.9f, 0.65f };
	light.light.intensity = 3.0f;
	sceneData.entities.push_back(light);

	Engine::JSceneEntityData grid;
	grid.stableID = "ent_grid_plane";
	grid.name = "Grid Plane";
	grid.hasTransform = true;
	grid.hasRenderObject = true;
	grid.renderObject.materialID = "mat_grid";
	grid.renderObject.meshID = "mesh_grid_plane";
	grid.renderObject.transparent = true;
	sceneData.entities.push_back(grid);

	Engine::JSceneEntityData cup;
	cup.stableID = "ent_sample_cup";
	cup.name = "Cup";
	cup.hasTransform = true;
	cup.transform.pitch = 1.507f;
	cup.hasRenderObject = true;
	cup.renderObject.materialID = "mat_sample_pbr";
	cup.renderObject.meshID = "mesh_cup";
	cup.renderObject.transparent = false;
	sceneData.entities.push_back(cup);

	return sceneData;
}

J_EDITOR_END
