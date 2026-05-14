#include "client/editor/JSceneBuilder.h"

#include "client/editor/JFBXLoader.h"
#include "engine/render/JMaterialFactory.h"
#include "engine/render/JRenderServer.h"

#include <iostream>

J_EDITOR_BEGIN

namespace
{
	struct PerFrameConstants
	{
		XMFLOAT4X4 viewProjection;
	};

	struct MaterialConstants
	{
		JVec4 baseColor;
		float roughness = 0.5f;
		float metallic = 1.0f;
		JVec2 padding = {};
	};

	std::string resolveResourcePath(const std::string& path)
	{
		const std::filesystem::path sourcePath(path);
		if (sourcePath.is_absolute())
		{
			return sourcePath.string();
		}

		return (std::filesystem::path(get_Engine_Res_Path()) / sourcePath).string();
	}

	Engine::JMesh* createPlaneMesh(const Engine::JSceneMeshPlaneData& planeData)
	{
		const float size = planeData.size;
		const float y = planeData.y;

		std::vector<float> positions =
		{
			-size, y, -size, 1.0f,
			 size, y, -size, 1.0f,
			 size, y,  size, 1.0f,
			-size, y,  size, 1.0f,
		};

		std::vector<uint32> indices = { 0, 1, 2, 0, 2, 3 };

		Engine::JMesh* mesh = new Engine::JMesh();
		mesh->SetPositions(std::move(positions));
		mesh->SetIndices(std::move(indices));
		return mesh;
	}

	Engine::JScene::TransformData toRuntimeTransform(const Engine::JSceneTransformData& source)
	{
		Engine::JScene::TransformData transform{};
		transform.translation = source.translation;
		transform.rotation = source.rotation;
		transform.scale = source.scale;
		return transform;
	}
}

void JSceneBuildResult::Release(Engine::JRenderServer* renderServer)
{
	if (renderServer != nullptr)
	{
		for (Engine::JCameraHandle camera : registeredCameras)
		{
			renderServer->UnregisterCamera(camera);
		}

		for (const std::unique_ptr<Engine::JMaterial>& material : materials)
		{
			if (material != nullptr)
			{
				renderServer->UnregisterMaterial(material->instanceID);
			}
		}

		for (const std::unique_ptr<Engine::JMesh>& mesh : meshes)
		{
			if (mesh != nullptr)
			{
				renderServer->GetRenderDB().RemoveMeshResource(mesh.get());
			}
		}
	}

	registeredCameras.clear();
	renderObjects.clear();
	lights.clear();
	cameras.clear();
	transforms.clear();
	entities.clear();
	materialIDs.clear();
	primaryCamera = {};
	firstLight = {};

	scene.reset();
	materials.clear();
	constantBuffers.clear();
	textures.clear();
	meshes.clear();
}

bool JSceneBuilder::Build(const Engine::JSceneData& sceneData, const JSceneBuildContext& context, JSceneBuildResult& outResult)
{
	outResult.Release(context.renderServer);

	if (context.materialFactory == nullptr || context.renderServer == nullptr)
	{
		std::cerr << "JSceneBuilder::Build failed: material factory or render server is null." << std::endl;
		return false;
	}

	JSceneBuildResult result;
	result.scene = std::make_unique<Engine::JScene>();

	PerFrameConstants perFrameConstants{};
	XMStoreFloat4x4(&perFrameConstants.viewProjection, XMMatrixIdentity());
	Render::JConstantBuffer* perFrameBuffer = context.materialFactory->CreateConstantBuffer(&perFrameConstants, sizeof(perFrameConstants));
	if (perFrameBuffer == nullptr)
	{
		std::cerr << "JSceneBuilder::Build failed: per-frame buffer creation failed." << std::endl;
		return false;
	}
	result.constantBuffers.emplace_back(perFrameBuffer);

	std::unordered_map<std::string, Engine::JMaterial*> materialLookup;
	for (const Engine::JSceneMaterialData& materialData : sceneData.materials)
	{
		if (materialData.id.empty())
		{
			std::cerr << "JSceneBuilder::Build failed: material id is empty." << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		if (materialLookup.find(materialData.id) != materialLookup.end())
		{
			std::cerr << "JSceneBuilder::Build failed: duplicated material id: " << materialData.id << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		Engine::JMaterial* material = context.materialFactory->CreateMaterial(resolveResourcePath(materialData.shaderPath), materialData.enableAlphaBlend);
		if (material == nullptr)
		{
			std::cerr << "JSceneBuilder::Build failed: material creation failed: " << materialData.id << std::endl;
			result.Release(context.renderServer);
			return false;
		}
		material->SetName(!materialData.name.empty() ? materialData.name : materialData.id);
		material->SetConstantBuffer("PerFrame", perFrameBuffer);

		if (materialData.constants.enabled)
		{
			MaterialConstants materialConstants{};
			materialConstants.baseColor = materialData.constants.baseColor;
			materialConstants.roughness = materialData.constants.roughness;
			materialConstants.metallic = materialData.constants.metallic;

			Render::JConstantBuffer* materialBuffer = context.materialFactory->CreateAndSetConstantBuffer(material, "PerMaterial", &materialConstants, sizeof(materialConstants));
			if (materialBuffer == nullptr)
			{
				delete material;
				std::cerr << "JSceneBuilder::Build failed: material constant buffer creation failed: " << materialData.id << std::endl;
				result.Release(context.renderServer);
				return false;
			}
			result.constantBuffers.emplace_back(materialBuffer);
		}

		for (const Engine::JSceneMaterialTextureData& textureData : materialData.textures)
		{
			Render::JTexture* texture = context.materialFactory->CreateAndSetTextureFromFile(material, textureData.name, resolveResourcePath(textureData.path));
			if (texture == nullptr)
			{
				delete material;
				std::cerr << "JSceneBuilder::Build failed: texture creation failed: " << textureData.path << std::endl;
				result.Release(context.renderServer);
				return false;
			}
			result.textures.emplace_back(texture);
		}

		context.renderServer->RegisterMaterial(material);
		result.materialIDs[materialData.id] = material->instanceID;
		materialLookup[materialData.id] = material;
		result.materials.emplace_back(material);
	}

	context.renderServer->Sync();

	std::unordered_map<std::string, Engine::JMesh*> meshLookup;
	for (const Engine::JSceneMeshData& meshData : sceneData.meshes)
	{
		if (meshData.id.empty())
		{
			std::cerr << "JSceneBuilder::Build failed: mesh id is empty." << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		if (meshLookup.find(meshData.id) != meshLookup.end())
		{
			std::cerr << "JSceneBuilder::Build failed: duplicated mesh id: " << meshData.id << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		Engine::JMesh* mesh = nullptr;
		if (meshData.type == Engine::JSceneMeshType::Plane)
		{
			mesh = createPlaneMesh(meshData.plane);
		}
		else
		{
			const std::string meshPath = resolveResourcePath(meshData.path);
			std::cout << "Loading mesh: " << meshPath << std::endl;
			JFBXLoader fbxLoader;
			mesh = fbxLoader.LoadFBX(meshPath.c_str());
		}

		if (mesh == nullptr)
		{
			std::cerr << "JSceneBuilder::Build failed: mesh creation failed: " << meshData.id << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		meshLookup[meshData.id] = mesh;
		result.meshes.emplace_back(mesh);
	}

	for (const Engine::JSceneEntityData& entityData : sceneData.entities)
	{
		Engine::JEntityHandle entity = result.scene->CreateEntity(entityData.stableID, entityData.name, entityData.tags);
		if (!entity.IsValid())
		{
			std::cerr << "JSceneBuilder::Build failed: entity creation failed: " << entityData.stableID << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		if (Engine::JScene::EntityData* runtimeEntity = result.scene->GetEntity(entity))
		{
			runtimeEntity->active = entityData.active;
		}

		const Engine::JEntityMetadata* entityMetadata = result.scene->GetEntityMetadata(entity);
		const std::string entityKey = entityMetadata != nullptr ? entityMetadata->stableID : entityData.stableID;
		result.entities[entityKey] = entity;

		Engine::JTransformHandle transform = {};
		if (entityData.hasTransform)
		{
			transform = result.scene->AddTransform(entity, toRuntimeTransform(entityData.transform));
			if (!transform.IsValid())
			{
				std::cerr << "JSceneBuilder::Build failed: transform creation failed: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}
			result.transforms[entityKey] = transform;
		}

		if (entityData.hasCamera)
		{
			if (!transform.IsValid())
			{
				std::cerr << "JSceneBuilder::Build failed: camera requires transform: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			Engine::JCameraHandle camera = result.scene->AddCamera(entity, transform, context.cameraAspectRatio);
			Engine::JScene::CameraData* cameraData = result.scene->GetCamera(camera);
			if (!camera.IsValid() || cameraData == nullptr)
			{
				std::cerr << "JSceneBuilder::Build failed: camera creation failed: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			cameraData->active = entityData.camera.active;
			cameraData->moveSpeed = entityData.camera.moveSpeed;
			cameraData->rotateSpeed = entityData.camera.rotateSpeed;
			result.cameras[entityKey] = camera;
			result.registeredCameras.push_back(camera);
			context.renderServer->RegisterCamera(camera, perFrameBuffer);

			if (entityData.camera.primary || !result.primaryCamera.IsValid())
			{
				result.primaryCamera = camera;
				result.scene->SetPrimaryCamera(camera);
			}
		}

		if (entityData.hasLight)
		{
			if (!transform.IsValid())
			{
				std::cerr << "JSceneBuilder::Build failed: light requires transform: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			Engine::JScene::LightData lightData{};
			lightData.active = entityData.light.active;
			lightData.color = entityData.light.color;
			lightData.intensity = entityData.light.intensity;

			Engine::JLightHandle light = result.scene->AddLight(entity, transform, lightData);
			if (!light.IsValid())
			{
				std::cerr << "JSceneBuilder::Build failed: light creation failed: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			result.lights[entityKey] = light;
			if (!result.firstLight.IsValid())
			{
				result.firstLight = light;
			}
		}

		if (entityData.hasRenderObject)
		{
			if (!transform.IsValid())
			{
				std::cerr << "JSceneBuilder::Build failed: render object requires transform: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			const auto meshIter = meshLookup.find(entityData.renderObject.meshID);
			const auto materialIter = result.materialIDs.find(entityData.renderObject.materialID);
			if (meshIter == meshLookup.end() || materialIter == result.materialIDs.end())
			{
				std::cerr << "JSceneBuilder::Build failed: render object asset reference is invalid: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			Engine::JRenderObjectHandle renderObject = result.scene->AddRenderObject(
				entity,
				transform,
				materialIter->second,
				meshIter->second,
				entityData.renderObject.transparent);

			Engine::JScene::RenderObjectData* renderObjectData = result.scene->GetRenderObject(renderObject);
			if (!renderObject.IsValid() || renderObjectData == nullptr)
			{
				std::cerr << "JSceneBuilder::Build failed: render object creation failed: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			renderObjectData->active = entityData.renderObject.active;
			renderObjectData->visible = entityData.renderObject.visible;
			result.renderObjects[entityKey] = renderObject;
		}
	}

	if (!result.primaryCamera.IsValid())
	{
		std::cerr << "JSceneBuilder::Build failed: scene has no camera." << std::endl;
		result.Release(context.renderServer);
		return false;
	}

	context.renderServer->Sync();
	context.renderServer->SyncScene(*result.scene);
	outResult = std::move(result);
	return true;
}

J_EDITOR_END
