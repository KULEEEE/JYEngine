#include "engine/render/JDebugOverlayPass.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JRenderContext.h"
#include "engine/render/JRenderResource.h"
#include "engine/render/JRenderTarget.h"
#include "engine/asset/JShader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iostream>

J_ENGINE_BEGIN

namespace
{
	constexpr float GLYPH_PIXEL = 3.0f;
	constexpr float GLYPH_SPACING = 2.0f;
	constexpr float LINE_SPACING = 5.0f;

	const std::array<uint8, 7>& glyphRows(char value)
	{
		static const std::array<uint8, 7> empty = { 0, 0, 0, 0, 0, 0, 0 };
		static const std::array<uint8, 7> space = { 0, 0, 0, 0, 0, 0, 0 };
		static const std::array<uint8, 7> colon = { 0, 4, 4, 0, 4, 4, 0 };
		static const std::array<uint8, 7> dot = { 0, 0, 0, 0, 0, 6, 6 };
		static const std::array<uint8, 7> comma = { 0, 0, 0, 0, 0, 4, 8 };
		static const std::array<uint8, 7> minus = { 0, 0, 0, 31, 0, 0, 0 };
		static const std::array<uint8, 7> slash = { 1, 2, 2, 4, 8, 8, 16 };
		static const std::array<uint8, 7> zero = { 14, 17, 19, 21, 25, 17, 14 };
		static const std::array<uint8, 7> one = { 4, 12, 4, 4, 4, 4, 14 };
		static const std::array<uint8, 7> two = { 14, 17, 1, 2, 4, 8, 31 };
		static const std::array<uint8, 7> three = { 30, 1, 1, 14, 1, 1, 30 };
		static const std::array<uint8, 7> four = { 2, 6, 10, 18, 31, 2, 2 };
		static const std::array<uint8, 7> five = { 31, 16, 30, 1, 1, 17, 14 };
		static const std::array<uint8, 7> six = { 6, 8, 16, 30, 17, 17, 14 };
		static const std::array<uint8, 7> seven = { 31, 1, 2, 4, 8, 8, 8 };
		static const std::array<uint8, 7> eight = { 14, 17, 17, 14, 17, 17, 14 };
		static const std::array<uint8, 7> nine = { 14, 17, 17, 15, 1, 2, 12 };
		static const std::array<uint8, 7> a = { 14, 17, 17, 31, 17, 17, 17 };
		static const std::array<uint8, 7> b = { 30, 17, 17, 30, 17, 17, 30 };
		static const std::array<uint8, 7> c = { 14, 17, 16, 16, 16, 17, 14 };
		static const std::array<uint8, 7> d = { 30, 17, 17, 17, 17, 17, 30 };
		static const std::array<uint8, 7> e = { 31, 16, 16, 30, 16, 16, 31 };
		static const std::array<uint8, 7> f = { 31, 16, 16, 30, 16, 16, 16 };
		static const std::array<uint8, 7> g = { 14, 17, 16, 23, 17, 17, 14 };
		static const std::array<uint8, 7> h = { 17, 17, 17, 31, 17, 17, 17 };
		static const std::array<uint8, 7> i = { 14, 4, 4, 4, 4, 4, 14 };
		static const std::array<uint8, 7> j = { 7, 2, 2, 2, 2, 18, 12 };
		static const std::array<uint8, 7> k = { 17, 18, 20, 24, 20, 18, 17 };
		static const std::array<uint8, 7> l = { 16, 16, 16, 16, 16, 16, 31 };
		static const std::array<uint8, 7> m = { 17, 27, 21, 21, 17, 17, 17 };
		static const std::array<uint8, 7> n = { 17, 25, 21, 19, 17, 17, 17 };
		static const std::array<uint8, 7> o = { 14, 17, 17, 17, 17, 17, 14 };
		static const std::array<uint8, 7> p = { 30, 17, 17, 30, 16, 16, 16 };
		static const std::array<uint8, 7> q = { 14, 17, 17, 17, 21, 18, 13 };
		static const std::array<uint8, 7> r = { 30, 17, 17, 30, 20, 18, 17 };
		static const std::array<uint8, 7> s = { 15, 16, 16, 14, 1, 1, 30 };
		static const std::array<uint8, 7> t = { 31, 4, 4, 4, 4, 4, 4 };
		static const std::array<uint8, 7> u = { 17, 17, 17, 17, 17, 17, 14 };
		static const std::array<uint8, 7> v = { 17, 17, 17, 17, 17, 10, 4 };
		static const std::array<uint8, 7> w = { 17, 17, 17, 21, 21, 21, 10 };
		static const std::array<uint8, 7> x = { 17, 17, 10, 4, 10, 17, 17 };
		static const std::array<uint8, 7> y = { 17, 17, 10, 4, 4, 4, 4 };
		static const std::array<uint8, 7> z = { 31, 1, 2, 4, 8, 16, 31 };

		const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
		switch (upper)
		{
		case ' ': return space;
		case ':': return colon;
		case '.': return dot;
		case ',': return comma;
		case '-': return minus;
		case '/': return slash;
		case '0': return zero;
		case '1': return one;
		case '2': return two;
		case '3': return three;
		case '4': return four;
		case '5': return five;
		case '6': return six;
		case '7': return seven;
		case '8': return eight;
		case '9': return nine;
		case 'A': return a;
		case 'B': return b;
		case 'C': return c;
		case 'D': return d;
		case 'E': return e;
		case 'F': return f;
		case 'G': return g;
		case 'H': return h;
		case 'I': return i;
		case 'J': return j;
		case 'K': return k;
		case 'L': return l;
		case 'M': return m;
		case 'N': return n;
		case 'O': return o;
		case 'P': return p;
		case 'Q': return q;
		case 'R': return r;
		case 'S': return s;
		case 'T': return t;
		case 'U': return u;
		case 'V': return v;
		case 'W': return w;
		case 'X': return x;
		case 'Y': return y;
		case 'Z': return z;
		default: return empty;
		}
	}

	void appendRect(std::vector<float>& positions, std::vector<uint32>& indices, float x, float y, float size, float width, float height)
	{
		const float left = (x / width) * 2.0f - 1.0f;
		const float right = ((x + size) / width) * 2.0f - 1.0f;
		const float top = 1.0f - (y / height) * 2.0f;
		const float bottom = 1.0f - ((y + size) / height) * 2.0f;
		const uint32 base = static_cast<uint32>(positions.size() / 4);

		positions.insert(positions.end(), {
			left, top, 0.0f, 1.0f,
			right, top, 0.0f, 1.0f,
			right, bottom, 0.0f, 1.0f,
			left, bottom, 0.0f, 1.0f,
		});
		indices.insert(indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
	}

	void appendText(std::vector<float>& positions, std::vector<uint32>& indices, const std::string& text, float x, float y, float width, float height)
	{
		float cursorX = x;
		for (char ch : text)
		{
			const std::array<uint8, 7>& rows = glyphRows(ch);
			for (uint32 row = 0; row < 7; ++row)
			{
				for (uint32 col = 0; col < 5; ++col)
				{
					const uint8 mask = static_cast<uint8>(1u << (4 - col));
					if ((rows[row] & mask) == 0)
					{
						continue;
					}

					appendRect(
						positions,
						indices,
						cursorX + static_cast<float>(col) * GLYPH_PIXEL,
						y + static_cast<float>(row) * GLYPH_PIXEL,
						GLYPH_PIXEL,
						width,
						height);
				}
			}
			cursorX += 5.0f * GLYPH_PIXEL + GLYPH_SPACING;
		}
	}
}

JDebugOverlayPass::~JDebugOverlayPass()
{
	releaseOwnedBuffers();
	delete _pipeline;
	delete _shader;
	_pipeline = nullptr;
	_shader = nullptr;
}

void JDebugOverlayPass::releaseOwnedBuffers()
{
	auto destroyVertexBuffer = [](Render::JVertexBuffer*& buffer)
		{
			if (buffer == nullptr)
			{
				return;
			}

			buffer->Destroy();
			delete buffer;
			buffer = nullptr;
		};

	auto destroyIndexBuffer = [](Render::JIndexBuffer*& buffer)
		{
			if (buffer == nullptr)
			{
				return;
			}

			buffer->Destroy();
			delete buffer;
			buffer = nullptr;
		};

	destroyVertexBuffer(_vertexBuffer);
	destroyIndexBuffer(_indexBuffer);
	for (Render::JVertexBuffer*& buffer : _retiredVertexBuffers)
	{
		destroyVertexBuffer(buffer);
	}
	for (Render::JIndexBuffer*& buffer : _retiredIndexBuffers)
	{
		destroyIndexBuffer(buffer);
	}

	_retiredVertexBuffers.clear();
	_retiredIndexBuffers.clear();
	_vertexFloatCapacity = 0;
	_indexCapacity = 0;
}

bool JDebugOverlayPass::ensureResources(const JRenderPassContext& context)
{
	if (_shader != nullptr && _pipeline != nullptr)
	{
		return true;
	}

	if (context.renderContext == nullptr)
	{
		return false;
	}

	const std::string shaderPath = get_Engine_Shader_Path() + "\\debug_overlay.hlsl";
	_shader = context.renderContext->CreateShader(shaderPath);
	if (_shader == nullptr)
	{
		std::cerr << "JDebugOverlayPass failed: shader creation failed." << std::endl;
		return false;
	}

	_pipeline = context.renderContext->CreatePipeline(_shader, true, true, false, false, {}, false);
	if (_pipeline == nullptr)
	{
		std::cerr << "JDebugOverlayPass failed: pipeline creation failed." << std::endl;
		return false;
	}

	return true;
}

bool JDebugOverlayPass::ensureOverlayBuffers(const JRenderPassContext& context, const std::vector<float>& positions, const std::vector<uint32>& indices)
{
	if (context.renderContext == nullptr || positions.empty() || indices.empty())
	{
		return false;
	}

	if (_vertexBuffer == nullptr || _vertexFloatCapacity < positions.size())
	{
		if (_vertexBuffer != nullptr)
		{
			_retiredVertexBuffers.push_back(_vertexBuffer);
			_vertexBuffer = nullptr;
		}

		_vertexBuffer = context.renderContext->CreateVertexBuffer(positions, positions.size() / 4);
		_vertexFloatCapacity = positions.size();
	}
	else
	{
		void* mappedData = nullptr;
		const HRESULT hr = _vertexBuffer->buffer->Map(0, nullptr, &mappedData);
		if (FAILED(hr) || mappedData == nullptr)
		{
			std::cerr << "JDebugOverlayPass vertex buffer map failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
			return false;
		}

		std::memcpy(mappedData, positions.data(), positions.size() * sizeof(float));
		_vertexBuffer->buffer->Unmap(0, nullptr);
		_vertexBuffer->view.SizeInBytes = static_cast<uint32>(positions.size() * sizeof(float));
		_vertexBuffer->view.StrideInBytes = sizeof(float) * 4;
	}

	if (_indexBuffer == nullptr || _indexCapacity < indices.size())
	{
		if (_indexBuffer != nullptr)
		{
			_retiredIndexBuffers.push_back(_indexBuffer);
			_indexBuffer = nullptr;
		}

		_indexBuffer = context.renderContext->CreateIndexBuffer(indices);
		_indexCapacity = indices.size();
	}
	else
	{
		void* mappedData = nullptr;
		const HRESULT hr = _indexBuffer->buffer->Map(0, nullptr, &mappedData);
		if (FAILED(hr) || mappedData == nullptr)
		{
			std::cerr << "JDebugOverlayPass index buffer map failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
			return false;
		}

		std::memcpy(mappedData, indices.data(), indices.size() * sizeof(uint32));
		_indexBuffer->buffer->Unmap(0, nullptr);
		_indexBuffer->view.SizeInBytes = static_cast<uint32>(indices.size() * sizeof(uint32));
	}

	return _vertexBuffer != nullptr && _indexBuffer != nullptr;
}

void JDebugOverlayPass::Execute(const JRenderPassContext& context, const JFrameDesc& frameDesc)
{
	_lastStats = {};
	_lastStats.name = GetName();

	if (frameDesc.debugOverlayLines.empty() || context.commandQueue == nullptr || context.renderContext == nullptr || frameDesc.renderTarget == nullptr || !ensureResources(context))
	{
		return;
	}

	const float width = std::max(frameDesc.viewport.Width, 1.0f);
	const float height = std::max(frameDesc.viewport.Height, 1.0f);
	std::vector<float> positions;
	std::vector<uint32> indices;
	positions.reserve(frameDesc.debugOverlayLines.size() * 512);
	indices.reserve(frameDesc.debugOverlayLines.size() * 768);

	float y = 14.0f;
	for (const std::string& line : frameDesc.debugOverlayLines)
	{
		appendText(positions, indices, line, 14.0f, y, width, height);
		y += 7.0f * GLYPH_PIXEL + LINE_SPACING;
	}

	if (positions.empty() || indices.empty())
	{
		return;
	}

	JMeshResource meshResource;
	meshResource.indexSize = indices.size();
	if (!ensureOverlayBuffers(context, positions, indices))
	{
		return;
	}

	meshResource.vertexBuffers.push_back(_vertexBuffer);
	meshResource.soaBuffers.push_back(_vertexBuffer->view);
	meshResource.indexBufferResource = _indexBuffer;
	meshResource.indexBuffer = _indexBuffer->view;

	context.commandQueue->BeginRenderPass(frameDesc.renderTarget, frameDesc.clearColor, 0, false);
	context.commandQueue->SetViewports(1, &frameDesc.viewport);
	context.commandQueue->SetScissorRects(1, &frameDesc.scissorRect);
	context.commandQueue->SetPipeline(_pipeline);
	context.commandQueue->SetGraphicResources(_shader);
	context.commandQueue->BindVertexBuffer(&meshResource);
	context.commandQueue->DrawIndexed(static_cast<uint32>(meshResource.indexSize), 1, 0, 0, 0);
	context.commandQueue->EndRenderPass();

	++_lastStats.drawCallCount;
}

J_ENGINE_END
