#pragma once
#ifndef __J_SHADER_H__
#define __J_SHADER_H__

#include "engine/precompile.h"

J_RENDER_BEGIN

class JShader
{
public:
	JShader() = delete;
	JShader(const std::wstring& path);
	~JShader();

	enum class ShaderType : uint8
	{
		VERTEX = 0,
		PIXEL = 1,
	};

	D3D12_SHADER_BYTECODE* GetByteCode() { return  _byteCodes; }

	void CompileShader();
private:
	ComPtr<ID3DBlob>					_vsBlob;
	ComPtr<ID3DBlob>					_psBlob;
	ComPtr<ID3DBlob>					_errBlob;

	D3D12_SHADER_BYTECODE				_byteCodes[2];

	const std::wstring					_path;
};

J_RENDER_END
#endif