#pragma once
#ifndef __J_DEVICE_H__
#define __J_DEVICE_H__

#include "precompile.h"

J_RENDER_BEGIN

class JDevice
{
public:
	JDevice();
	~JDevice();

	
	ComPtr<IDXGIFactory> GetDXGI() const { return _dxgi; }
	ComPtr<ID3D12Device> GetDevice() const { return _device; }

private:

	void initialize();
	void destroy();

	ComPtr<ID3D12Debug>		_debugController;
	ComPtr<IDXGIFactory>	_dxgi; // ȭ�� ���� ���
	ComPtr<ID3D12Device>	_device; // ���� ��ü ����
};

J_RENDER_END

#endif