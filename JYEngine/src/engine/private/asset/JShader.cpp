#include "engine/asset/JShader.h"

J_RENDER_BEGIN

JShader::JShader(const std::wstring& path)
	: _path(path)
{
}

JShader::~JShader()
{
}

void JShader::CompileShader()
{
	uint32 compileFlag = 0;
#ifdef _DEBUG
	compileFlag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	// vertex shader
	std::string name = "vMain"; // TODO: 따로 파싱해서 얻어오자
	std::string version = "vs_5_0";
	
	if (FAILED(::D3DCompileFromFile(_path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE
		, name.c_str(), version.c_str(), compileFlag, 0, &_vsBlob, &_errBlob)))
	{
		::MessageBoxA(nullptr, "Shader Create Failed !", nullptr, MB_OK);
	}

	// pixel shader
	name = "pMain"; // TODO: 따로 파싱해서 얻어오자
	version = "ps_5_0";

	if (FAILED(::D3DCompileFromFile(_path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE
		, name.c_str(), version.c_str(), compileFlag, 0, &_psBlob, &_errBlob)))
	{
		::MessageBoxA(nullptr, "Shader Create Failed !", nullptr, MB_OK);
	}

	_byteCodes[0] = {_vsBlob->GetBufferPointer(), _vsBlob->GetBufferSize()};
	_byteCodes[1] = {_psBlob->GetBufferPointer(), _psBlob->GetBufferSize()};
}

J_RENDER_END