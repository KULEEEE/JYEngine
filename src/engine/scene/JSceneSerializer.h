#pragma once
#ifndef __J_SCENE_SERIALIZER_H__
#define __J_SCENE_SERIALIZER_H__

#include "engine/scene/JSceneData.h"

J_ENGINE_BEGIN

class JSceneSerializer
{
public:
	static bool LoadFromFile(const std::filesystem::path& filePath, JSceneData& outData);
	static bool LoadFromString(const std::string& jsonText, JSceneData& outData);
	static bool SaveToFile(const std::filesystem::path& filePath, const JSceneData& sceneData);
	static bool SaveToString(const JSceneData& sceneData, std::string& outJson);
};

J_ENGINE_END

#endif
