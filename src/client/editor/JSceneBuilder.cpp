#include "client/editor/JSceneBuilder.h"

#include "engine/render/JMaterialFactory.h"
#include "engine/render/JRenderServer.h"

#include <iostream>

J_EDITOR_BEGIN

namespace
{
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

		for (const std::shared_ptr<JAssetManager::MaterialBundle>& bundle : materialBundles)
		{
			if (bundle != nullptr && bundle->material != nullptr)
			{
				renderServer->UnregisterMaterial(bundle->material->instanceID);
			}
		}

		for (const std::shared_ptr<Engine::JMesh>& mesh : meshes)
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
	materialBundles.clear();
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

		if (context.assetManager == nullptr)
		{
			std::cerr << "JSceneBuilder::Build failed: asset manager is null." << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		std::shared_ptr<JAssetManager::MaterialBundle> materialBundle = context.assetManager->AcquireMaterialBundle(materialData);
		if (!materialBundle || !materialBundle->material)
		{
			std::cerr << "JSceneBuilder::Build failed: material creation failed: " << materialData.id << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		context.renderServer->RegisterMaterial(materialBundle->material.get());
		result.materialIDs[materialData.id] = materialBundle->material->instanceID;
		materialLookup[materialData.id] = materialBundle->material.get();
		result.materialBundles.emplace_back(materialBundle);
		result.materials.emplace_back(materialBundle->material);
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

		if (context.assetManager == nullptr)
		{
			std::cerr << "JSceneBuilder::Build failed: asset manager is null." << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		std::shared_ptr<Engine::JMesh> mesh = context.assetManager->AcquireMesh(meshData);
		if (!mesh)
		{
			std::cerr << "JSceneBuilder::Build failed: mesh creation failed: " << meshData.id << std::endl;
			result.Release(context.renderServer);
			return false;
		}

		meshLookup[meshData.id] = mesh.get();
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

			Engine::JCameraHandle camera = result.scene->AddCamera(
				entity,
				transform,
				context.cameraAspectRatio,
				entityData.camera.nearP,
				entityData.camera.farP);
			Engine::JScene::CameraData* cameraData = result.scene->GetCamera(camera);
			if (!camera.IsValid() || cameraData == nullptr)
			{
				std::cerr << "JSceneBuilder::Build failed: camera creation failed: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			Engine::JScene::CameraData resolvedCameraData = *cameraData;
			resolvedCameraData.active = entityData.camera.active;
			resolvedCameraData.nearP = entityData.camera.nearP;
			resolvedCameraData.farP = entityData.camera.farP;
			result.scene->SetCameraData(camera, resolvedCameraData);
		
			result.cameras[entityKey] = camera;
			result.registeredCameras.push_back(camera);
			context.renderServer->RegisterCamera(camera);

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

			Engine::JLightHandle light = result.scene->AddLight(entity, lightData);
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

		if (entityData.hasRenderObjectComponent)
		{
			if (!transform.IsValid())
			{
				std::cerr << "JSceneBuilder::Build failed: render object component requires transform: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			const auto meshIter = meshLookup.find(entityData.renderObjectComponent.meshID);
			const auto materialIter = result.materialIDs.find(entityData.renderObjectComponent.materialID);
			if (meshIter == meshLookup.end() || materialIter == result.materialIDs.end())
			{
				std::cerr << "JSceneBuilder::Build failed: render object component asset reference is invalid: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			Engine::JRenderObjectComponentHandle renderObject = result.scene->AddRenderObjectComponent(
				entity,
				materialIter->second,
				meshIter->second,
				entityData.renderObjectComponent.transparent);

			Engine::JScene::RenderObjectComponentData* renderObjectData = result.scene->GetRenderObjectComponent(renderObject);
			if (!renderObject.IsValid() || renderObjectData == nullptr)
			{
				std::cerr << "JSceneBuilder::Build failed: render object component creation failed: " << entityData.stableID << std::endl;
				result.Release(context.renderServer);
				return false;
			}

			Engine::JScene::RenderObjectComponentData resolvedRenderObjectData = *renderObjectData;
			resolvedRenderObjectData.active = entityData.renderObjectComponent.active;
			resolvedRenderObjectData.visible = entityData.renderObjectComponent.visible;
			result.scene->SetRenderObjectComponentData(renderObject, resolvedRenderObjectData);
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
