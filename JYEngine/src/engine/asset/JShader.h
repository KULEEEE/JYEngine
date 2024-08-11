#pragma once
#ifndef __J_SHADER_H__
#define __J_SHADER_H__

#include "engine/precompile.h"

/*#include "engine/JRenderContext.h"*/	namespace J { namespace Render { struct JRootSignature; } }

J_RENDER_BEGIN

class JShader
{
public:
	JShader() = delete;
	JShader(const std::string& path);
	~JShader();

	enum class ShaderType : uint8
	{
		VERTEX = 0,
		PIXEL = 1,
	};

	struct BindingInfo
	{
		struct Resource
		{
			std::string name;
			uint32		nameHash;
			uint32		slot;
		};

		std::vector<Resource> cBuffers;
		std::vector<Resource> textures;
		std::vector<Resource> samplers;
	}bindingInfo;

	D3D12_SHADER_BYTECODE* GetByteCode() { return  _byteCodes; }

	const JRootSignature* GetRootSignature() { return _rootSignature; }

	void CompileShader();
private:
	ComPtr<ID3DBlob>					_vsBlob;
	ComPtr<ID3DBlob>					_psBlob;
	ComPtr<ID3DBlob>					_errBlob;

	D3D12_SHADER_BYTECODE				_byteCodes[2];

	std::wstring					_path;

	JRootSignature* 				_rootSignature;
};

J_RENDER_END

#endif