#pragma once
#ifndef __J_RENDER_FRAME_H__
#define __J_RENDER_FRAME_H__

#include "engine/precompile.h"
#include "engine/render/JRenderDefinition.h"
#include "engine/scene/JScene.h"

/*#include "engine/render/JRenderTarget.h"*/ namespace J { namespace Engine { class JRenderTarget; } }
/*#include "engine/asset/JMesh.h"*/ namespace J { namespace Engine { class JMesh; } }
/*#include "engine/asset/JMaterial.h"*/ namespace J { namespace Engine { class JMaterial; } }
namespace J { namespace Engine { struct JDrawItemCache; } }

J_ENGINE_BEGIN

template <typename T>
class JArrayView
{
public:
	JArrayView() = default;
	JArrayView(const T* data, size_t size) : _data(data), _size(size) {}

	template <typename Allocator>
	JArrayView(const std::vector<typename std::remove_const<T>::type, Allocator>& values)
		: _data(values.data()), _size(values.size())
	{
	}

	template <typename Allocator>
	JArrayView(std::vector<typename std::remove_const<T>::type, Allocator>& values)
		: _data(values.data()), _size(values.size())
	{
	}

	T* begin() const { return _data; }
	T* end() const { return _data + _size; }
	T& operator[](size_t index) const { return _data[index]; }
	T* data() const { return _data; }
	size_t size() const { return _size; }
	bool empty() const { return _size == 0; }

private:
	T* _data = nullptr;
	size_t _size = 0;
};

struct JDrawItem
{
	JEntityHandle entity = {};
	JRenderObjectComponentHandle renderObject = {};
	JTransformHandle transform = {};
	JMaterialHandle material = {};
	const JMesh* mesh = nullptr;
	uint32 meshResourceIndex = static_cast<uint32>(-1);
	uint32 materialResourceIndex = static_cast<uint32>(-1);
	uint32 transformResourceIndex = static_cast<uint32>(-1);
	uint32 subMeshIndex = 0;
	uint32 indexCount = 0;
	uint32 startIndex = 0;
	bool transparent = false;
};

struct JFrameTransformSnapshot
{
	JTransformHandle transform = {};
	XMMATRIX world = XMMatrixIdentity();
	bool valid = false;
	bool dirty = false;
};

struct JFrameLightSnapshot
{
	struct Item
	{
		JVec4 colorIntensity = { 1.0f, 1.0f, 1.0f, 0.35f };
		JVec4 position = { 0.0f, 4.0f, -4.0f, 1.0f };
	};

	std::vector<Item> items;
};

struct JFrameMaterialSnapshot
{
	JMaterialHandle material = {};
	JMaterial* source = nullptr;
};

struct JFrameDesc
{
	JCameraHandle camera = {};
	JRenderTarget* renderTarget = nullptr;
	JColor clearColor = JColors::DarkGray;
	Render::JViewport viewport = {};
	D3D12_RECT scissorRect = {};
	JDrawItemCache* drawItemCache = nullptr;
	XMMATRIX cameraViewProjection = XMMatrixIdentity();
	XMMATRIX cameraInverseViewProjection = XMMatrixIdentity();
	JVec4 cameraWorldPosition = { 0.0f, 0.0f, 0.0f, 1.0f };
	JArrayView<JFrameTransformSnapshot> transformSnapshots;
	JArrayView<const JFrameMaterialSnapshot> materialSnapshots;
	JArrayView<const JFrameLightSnapshot::Item> lightItems;
	JArrayView<const uint32> opaqueDrawItemIndices;
	JArrayView<const uint32> transparentDrawItemIndices;
	uint32 cullingTestedDrawItemCount = 0;
	uint32 culledDrawItemCount = 0;

	// Reflection probe 캡처용. captureMode=true면 LightingPass가 톤매핑을 건너뛰고
	// renderTarget의 renderTargetSlice 면(큐브 face)에 linear HDR을 출력한다.
	bool captureMode = false;
	uint32 renderTargetSlice = 0;
};

J_ENGINE_END

#endif
