#include "engine/JDevice.h"

#include <iostream>

using namespace std;

J_RENDER_BEGIN

JDevice::JDevice()
{
	initialize();
}

JDevice::~JDevice()
{
	destroy();
}

void JDevice::initialize()
{
	UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
	// Enable the D3D12 debug layer when the SDK layers are available.
	const HRESULT debugHr = ::D3D12GetDebugInterface(IID_PPV_ARGS(&_debugController));
	if (SUCCEEDED(debugHr) && _debugController)
	{
		_debugController->EnableDebugLayer();
		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
	else
	{
		std::cerr << "D3D12 debug layer is unavailable. HRESULT=0x"
			<< std::hex << debugHr << std::dec << std::endl;
	}
#endif

	HRESULT factoryHr = ::CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&_dxgi));
	if (FAILED(factoryHr) && dxgiFactoryFlags != 0)
	{
		std::cerr << "CreateDXGIFactory2 with debug flag failed. HRESULT=0x"
			<< std::hex << factoryHr << std::dec << std::endl;
		factoryHr = ::CreateDXGIFactory2(0, IID_PPV_ARGS(&_dxgi));
	}

	if (FAILED(factoryHr))
	{
		std::cerr << "CreateDXGIFactory2 failed. Retrying with CreateDXGIFactory1. HRESULT=0x"
			<< std::hex << factoryHr << std::dec << std::endl;
		factoryHr = ::CreateDXGIFactory1(IID_PPV_ARGS(&_dxgi));
	}

	if (FAILED(factoryHr) || _dxgi == nullptr)
	{
		std::cerr << "DXGI factory creation failed. HRESULT=0x"
			<< std::hex << factoryHr << std::dec << std::endl;
		return;
	}

	ComPtr<IDXGIAdapter> adapter;
	const HRESULT adapterHr = _dxgi->EnumAdapters(0, adapter.GetAddressOf());
	if (FAILED(adapterHr) || adapter == nullptr)
	{
		std::cerr << "EnumAdapters failed. HRESULT=0x" << std::hex << adapterHr << std::dec << std::endl;
		return;
	}

	DXGI_ADAPTER_DESC desc;
	adapter->GetDesc(&desc);

	std::wcout << L"Description: " << desc.Description << std::endl;
	std::wcout << L"Vendor ID: " << desc.VendorId << std::endl;
	std::wcout << L"Device ID: " << desc.DeviceId << std::endl;
	std::wcout << L"SubSys ID: " << desc.SubSysId << std::endl;
	std::wcout << L"Revision: " << desc.Revision << std::endl;
	std::wcout << L"Dedicated Video Memory: " << desc.DedicatedVideoMemory / (1024 * 1024) << L" MB" << std::endl;
	std::wcout << L"Dedicated System Memory: " << desc.DedicatedSystemMemory / (1024 * 1024) << L" MB" << std::endl;
	std::wcout << L"Shared System Memory: " << desc.SharedSystemMemory / (1024 * 1024) << L" MB" << std::endl;
	std::wcout << std::endl;

	const HRESULT deviceHr = ::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device));
	if (FAILED(deviceHr) || _device == nullptr)
	{
		std::cerr << "D3D12CreateDevice failed. HRESULT=0x" << std::hex << deviceHr << std::dec << std::endl;
	}
}

void JDevice::destroy()
{

}

J_RENDER_END
