#pragma once
#ifndef __J_ROOTSIGNATURE_H__
#define __J_ROOTSIGNATURE_H__

#include "precompile.h"

/*#include "engine/JDevice.h"*/	namespace J { namespace Render { class JDevice; } }

J_RENDER_BEGIN

//class
class JRootSignature
{
public:

	JRootSignature();
	~JRootSignature();

	void Initialize(const JDevice* device);
	ComPtr<ID3D12RootSignature> GetSignature() { return _signature; }
private:
	
	ComPtr<ID3D12RootSignature> _signature;
};

J_RENDER_END
#endif