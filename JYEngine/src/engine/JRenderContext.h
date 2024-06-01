#pragma once

#ifndef __J_RENDER_CONTEXT_H__
#define __J_RENDER_CONTEXT_H__

#include "precompile.h"
#include "engine/JDevice.h"
#include "engine/JRenderDefinition.h"
#include "engine/asset/JShader.h"

J_RENDER_BEGIN

class JRenderContext
{
public:
	JRenderContext() = delete;
	JRenderContext(JDevice* device);
	~JRenderContext();

	JVertexBuffer* CreateVertexBuffer(const void* data, size_t size, size_t vertexCount);
	void DestroyVertexBuffer(JVertexBuffer* buffer) { delete buffer; }

	JShader* CreateShader(const std::wstring& path);
private:
	JDevice* _device;
};

J_RENDER_END

#endif