#include "engine/scene/JSceneSerializer.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

J_ENGINE_BEGIN

namespace
{
	using json = nlohmann::json;

	// Scene JSON은 벡터를 [x, y, z] 형태로 저장한다.
	JVec3 readVec3(const json& value)
	{
		return { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>() };
	}

	JVec4 readVec4(const json& value)
	{
		return { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), value.at(3).get<float>() };
	}

	json vec3ToJson(const JVec3& value)
	{
		return json::array({ value.x, value.y, value.z });
	}

	json vec4ToJson(const JVec4& value)
	{
		return json::array({ value.x, value.y, value.z, value.w });
	}

	JSceneMaterialTextureData loadTexture(const json& value)
	{
		JSceneMaterialTextureData data;
		data.name = value.value("name", "");
		data.path = value.value("path", "");
		return data;
	}

	json saveTexture(const JSceneMaterialTextureData& data)
	{
		return json
		{
			{ "name", data.name },
			{ "path", data.path },
		};
	}

	JSceneMaterialData loadMaterial(const json& value)
	{
		JSceneMaterialData data;
		data.id = value.value("id", "");
		data.path = value.value("path", "");
		data.name = value.value("name", "");
		data.shaderPath = value.value("shaderPath", "");
		data.enableAlphaBlend = value.value("enableAlphaBlend", false);

		if (value.contains("constants") && value.at("constants").is_object())
		{
			const json& constants = value.at("constants");
			data.constants.enabled = constants.value("enabled", false);
			if (constants.contains("baseColor"))
			{
				data.constants.baseColor = readVec4(constants.at("baseColor"));
			}
			data.constants.roughness = constants.value("roughness", data.constants.roughness);
			data.constants.metallic = constants.value("metallic", data.constants.metallic);
		}

		if (value.contains("textures") && value.at("textures").is_array())
		{
			for (const json& textureValue : value.at("textures"))
			{
				data.textures.emplace_back(loadTexture(textureValue));
			}
		}

		return data;
	}

	json saveMaterial(const JSceneMaterialData& data)
	{
		json value =
		{
			{ "id", data.id },
		};

		if (!data.path.empty())
		{
			value["path"] = data.path;
			return value;
		}

		value["name"] = data.name;
		value["shaderPath"] = data.shaderPath;
		value["enableAlphaBlend"] = data.enableAlphaBlend;
		value["constants"] =
		{
			{ "enabled", data.constants.enabled },
			{ "baseColor", vec4ToJson(data.constants.baseColor) },
			{ "roughness", data.constants.roughness },
			{ "metallic", data.constants.metallic },
		};

		if (!data.textures.empty())
		{
			value["textures"] = json::array();
			for (const JSceneMaterialTextureData& texture : data.textures)
			{
				value["textures"].push_back(saveTexture(texture));
			}
		}

		return value;
	}

	JSceneMeshData loadMesh(const json& value)
	{
		JSceneMeshData data;
		data.id = value.value("id", "");
		data.name = value.value("name", "");

		const std::string typeName = value.value("type", "External");
		data.type = (typeName == "Plane") ? JSceneMeshType::Plane : JSceneMeshType::External;

		if (data.type == JSceneMeshType::External)
		{
			data.path = value.value("path", "");
		}
		else if (value.contains("plane") && value.at("plane").is_object())
		{
			const json& plane = value.at("plane");
			data.plane.size = plane.value("size", data.plane.size);
			data.plane.y = plane.value("y", data.plane.y);
		}

		return data;
	}

	json saveMesh(const JSceneMeshData& data)
	{
		json value =
		{
			{ "id", data.id },
			{ "name", data.name },
			{ "type", data.type == JSceneMeshType::Plane ? "Plane" : "External" },
		};

		if (data.type == JSceneMeshType::External)
		{
			value["path"] = data.path;
		}
		else
		{
			value["plane"] =
			{
				{ "size", data.plane.size },
				{ "y", data.plane.y },
			};
		}

		return value;
	}

	JSceneEntityData loadEntity(const json& value)
	{
		JSceneEntityData data;
		data.stableID = value.value("stableID", "");
		data.name = value.value("name", "");
		data.active = value.value("active", true);

		if (value.contains("tags") && value.at("tags").is_array())
		{
			for (const json& tag : value.at("tags"))
			{
				data.tags.push_back(tag.get<std::string>());
			}
		}

		if (value.contains("transform") && value.at("transform").is_object())
		{
			data.hasTransform = true;
			const json& transform = value.at("transform");
			if (transform.contains("translation"))
			{
				data.transform.translation = readVec3(transform.at("translation"));
			}
			else if (transform.contains("position"))
			{
				// 예전 scene 파일의 position 필드도 계속 읽기 위한 호환 처리.
				data.transform.translation = readVec3(transform.at("position"));
			}

			if (transform.contains("rotation"))
			{
				data.transform.rotation = readVec3(transform.at("rotation"));
			}
			else
			{
				data.transform.rotation.x = transform.value("pitch", data.transform.rotation.x);
				data.transform.rotation.y = transform.value("yaw", data.transform.rotation.y);
				data.transform.rotation.z = transform.value("roll", data.transform.rotation.z);
			}

			if (transform.contains("scale"))
			{
				data.transform.scale = readVec3(transform.at("scale"));
			}
		}

		if (value.contains("camera") && value.at("camera").is_object())
		{
			data.hasCamera = true;
			const json& camera = value.at("camera");
			data.camera.active = camera.value("active", data.camera.active);
			data.camera.primary = camera.value("primary", data.camera.primary);
			data.camera.nearP = camera.value("nearP", data.camera.nearP);
			data.camera.farP = camera.value("farP", data.camera.farP);
		}

		if (value.contains("light") && value.at("light").is_object())
		{
			data.hasLight = true;
			const json& light = value.at("light");
			data.light.active = light.value("active", data.light.active);
			if (light.contains("color"))
			{
				data.light.color = readVec3(light.at("color"));
			}
			data.light.intensity = light.value("intensity", data.light.intensity);
		}

		const json* renderObjectComponent = nullptr;
		if (value.contains("renderObjectComponent") && value.at("renderObjectComponent").is_object())
		{
			renderObjectComponent = &value.at("renderObjectComponent");
		}
		if (renderObjectComponent != nullptr)
		{
			data.hasRenderObjectComponent = true;
			data.renderObjectComponent.active = renderObjectComponent->value("active", data.renderObjectComponent.active);
			data.renderObjectComponent.visible = renderObjectComponent->value("visible", data.renderObjectComponent.visible);
			data.renderObjectComponent.transparent = renderObjectComponent->value("transparent", data.renderObjectComponent.transparent);
			data.renderObjectComponent.materialID = renderObjectComponent->value("materialID", data.renderObjectComponent.materialID);
			if (renderObjectComponent->contains("subMeshMaterialIDs") && renderObjectComponent->at("subMeshMaterialIDs").is_array())
			{
				for (const json& materialID : renderObjectComponent->at("subMeshMaterialIDs"))
				{
					data.renderObjectComponent.subMeshMaterialIDs.push_back(materialID.get<std::string>());
				}
			}
			data.renderObjectComponent.meshID = renderObjectComponent->value("meshID", data.renderObjectComponent.meshID);
		}

		return data;
	}

	json saveEntity(const JSceneEntityData& data)
	{
		json value =
		{
			{ "stableID", data.stableID },
			{ "name", data.name },
			{ "active", data.active },
		};

		if (!data.tags.empty())
		{
			value["tags"] = data.tags;
		}

		if (data.hasTransform)
		{
			value["transform"] =
			{
				{ "translation", vec3ToJson(data.transform.translation) },
				{ "rotation", vec3ToJson(data.transform.rotation) },
				{ "scale", vec3ToJson(data.transform.scale) },
			};
		}

		if (data.hasCamera)
		{
			value["camera"] =
			{
				{ "active", data.camera.active },
				{ "primary", data.camera.primary },
				{ "nearP", data.camera.nearP },
				{ "farP", data.camera.farP },
			};
		}

		if (data.hasLight)
		{
			value["light"] =
			{
				{ "active", data.light.active },
				{ "color", vec3ToJson(data.light.color) },
				{ "intensity", data.light.intensity },
			};
		}

		if (data.hasRenderObjectComponent)
		{
			value["renderObjectComponent"] =
			{
				{ "active", data.renderObjectComponent.active },
				{ "visible", data.renderObjectComponent.visible },
				{ "transparent", data.renderObjectComponent.transparent },
				{ "materialID", data.renderObjectComponent.materialID },
				{ "meshID", data.renderObjectComponent.meshID },
			};
			if (!data.renderObjectComponent.subMeshMaterialIDs.empty())
			{
				value["renderObjectComponent"]["subMeshMaterialIDs"] = data.renderObjectComponent.subMeshMaterialIDs;
			}
		}

		return value;
	}
}

bool JSceneSerializer::LoadFromString(const std::string& jsonText, JSceneData& outData)
{
	try
	{
		const json root = json::parse(jsonText);
		if (!root.is_object())
		{
			return false;
		}

		JSceneData sceneData;
		sceneData.id = root.value("id", "");
		sceneData.name = root.value("name", "");
		sceneData.version = root.value("version", sceneData.version);

		// 파일 포맷은 resource 목록을 먼저 읽고, entity는 id로 resource를 참조한다.
		if (root.contains("materials") && root.at("materials").is_array())
		{
			for (const json& materialValue : root.at("materials"))
			{
				sceneData.materials.emplace_back(loadMaterial(materialValue));
			}
		}

		if (root.contains("meshes") && root.at("meshes").is_array())
		{
			for (const json& meshValue : root.at("meshes"))
			{
				sceneData.meshes.emplace_back(loadMesh(meshValue));
			}
		}

		if (root.contains("entities") && root.at("entities").is_array())
		{
			for (const json& entityValue : root.at("entities"))
			{
				sceneData.entities.emplace_back(loadEntity(entityValue));
			}
		}

		outData = std::move(sceneData);
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool JSceneSerializer::LoadFromFile(const std::filesystem::path& filePath, JSceneData& outData)
{
	std::ifstream input(filePath, std::ios::in | std::ios::binary);
	if (!input.is_open())
	{
		return false;
	}

	std::ostringstream stream;
	stream << input.rdbuf();
	return LoadFromString(stream.str(), outData);
}

bool JSceneSerializer::SaveToString(const JSceneData& sceneData, std::string& outJson)
{
	json root;
	root["id"] = sceneData.id;
	root["name"] = sceneData.name;
	root["version"] = sceneData.version;

	root["materials"] = json::array();
	for (const JSceneMaterialData& material : sceneData.materials)
	{
		root["materials"].push_back(saveMaterial(material));
	}

	root["meshes"] = json::array();
	for (const JSceneMeshData& mesh : sceneData.meshes)
	{
		root["meshes"].push_back(saveMesh(mesh));
	}

	root["entities"] = json::array();
	for (const JSceneEntityData& entity : sceneData.entities)
	{
		root["entities"].push_back(saveEntity(entity));
	}

	outJson = root.dump(2);
	return true;
}

bool JSceneSerializer::SaveToFile(const std::filesystem::path& filePath, const JSceneData& sceneData)
{
	std::string jsonText;
	if (!SaveToString(sceneData, jsonText))
	{
		return false;
	}

	const std::filesystem::path directory = filePath.parent_path();
	if (!directory.empty())
	{
		std::error_code errorCode;

		// 저장 경로의 폴더가 없으면 생성한다. 실패 시 파일을 만들지 않는다.
		std::filesystem::create_directories(directory, errorCode);
		if (errorCode)
		{
			return false;
		}
	}

	std::ofstream output(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!output.is_open())
	{
		return false;
	}

	output << jsonText;
	return output.good();
}

J_ENGINE_END
