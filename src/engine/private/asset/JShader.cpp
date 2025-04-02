#include "engine/asset/JShader.h"
#include "engine/JHashFunction.h"

#include "engine/JRenderContext.h"
#include "engine/JEngineContext.h"

J_RENDER_BEGIN

JShader::JShader(const std::string& path)
{
	size_t size_needed = std::mbstowcs(NULL, path.c_str(), 0) + 1;
	std::vector<wchar_t> wstr(size_needed);
	std::mbstowcs(&wstr[0], path.c_str(), size_needed);
	_path = std::wstring(wstr.begin(), wstr.end() - 1);
}

JShader::~JShader()
{
	GetEngine()->GetRenderContext()->DestroyRootSignature(_rootSignature);
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
		std::string errorMessage = static_cast<const char*>(_errBlob->GetBufferPointer());
		::MessageBoxA(nullptr, errorMessage.c_str(), nullptr, MB_OK);
		return;
	}

	// pixel shader
	name = "pMain"; // TODO: 따로 파싱해서 얻어오자
	version = "ps_5_0";

	if (FAILED(::D3DCompileFromFile(_path.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE
		, name.c_str(), version.c_str(), compileFlag, 0, &_psBlob, &_errBlob)))
	{
		std::string errorMessage = static_cast<const char*>(_errBlob->GetBufferPointer());
		::MessageBoxA(nullptr, errorMessage.c_str(), nullptr, MB_OK);
		return;
	}

	_byteCodes[0] = {_vsBlob->GetBufferPointer(), _vsBlob->GetBufferSize()};
	_byteCodes[1] = {_psBlob->GetBufferPointer(), _psBlob->GetBufferSize()};

	auto setReflection = [&](ComPtr<ID3DBlob> blob)
		{
			ComPtr<ID3D12ShaderReflection> shaderReflection;
			HRESULT hr = D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&shaderReflection));
			if (FAILED(hr))
			{
				return;
			}
			D3D12_SHADER_DESC shaderDesc;
			shaderReflection->GetDesc(&shaderDesc);

			for (uint32 i = 0; i < shaderDesc.BoundResources; ++i)
			{
				D3D12_SHADER_INPUT_BIND_DESC bindDesc;
				shaderReflection->GetResourceBindingDesc(i, &bindDesc);

				BindingInfo::Resource resource;
				resource.name = bindDesc.Name;
				resource.nameHash = JHashFunction::StrCrc32(resource.name.c_str());
				resource.slot = bindDesc.BindPoint;

				switch (bindDesc.Type)
				{
				case D3D_SIT_CBUFFER:
				{
					bindingInfo.cBuffers.push_back(resource);
					break;
				}
				case D3D_SIT_TEXTURE:
				{
					bindingInfo.textures.push_back(resource);
					break;
				}
				case D3D_SIT_SAMPLER:
				{
					bindingInfo.samplers.push_back(resource);
					break;
				}
				default:
				{
					// 구현 필요
				}
				}
			}
		};
	setReflection(_vsBlob);
	setReflection(_psBlob);	

	_rootSignature = GetEngine()->GetRenderContext()->CreateRootSignature(this);
}

J_RENDER_END