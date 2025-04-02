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
	// D3D12 ������� Ȱ��ȭ
	// - VC++ ���â�� ���� ����� �޽��� ���
	// - riid : ����̽��� COM ID
	// - ppDevice : ������ ��ġ�� �Ű������� ����
#ifdef _DEBUG
	::D3D12GetDebugInterface(IID_PPV_ARGS(&_debugController));
	_debugController->EnableDebugLayer();
	if (_debugController) _debugController->Release();

#endif

	// DXGI(DirectX Graphics Infrastructure)
	// Direct3D�� �Բ� ���̴� API
	// - ��ü ȭ�� ��� ��ȯ
	// - �����Ǵ� ���÷��� ��� ���� ��
	// CreateDXGIFactory
	// - riid : ����̽��� COM ID
	// - ppDevice : ������ ��ġ�� �Ű������� ����
	::CreateDXGIFactory(IID_PPV_ARGS(&_dxgi));

	IDXGIAdapter* adapter = nullptr;
	_dxgi->EnumAdapters(0, &adapter);
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
	
	// CreateDevice
	// - ���÷��� �����(�׷��� ī��)�� ��Ÿ���� ��ü
	// - pAdapter : nullptr �����ϸ� �ý��� �⺻ ���÷��� �����
	// - MinimumFeatureLevel : ���� ���α׷��� �䱸�ϴ� �ּ� ��� ���� (���ڴٸ� �ɷ�����)
	// - riid : ����̽��� COM ID
	// - ppDevice : ������ ��ġ�� �Ű������� ����
	::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device));
}

void JDevice::destroy()
{

}

J_RENDER_END