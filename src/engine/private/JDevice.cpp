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
	// D3D12 디버그층 활성화
	// - VC++ 출력창에 상세한 디버깅 메시지 출력
	// - riid : 디바이스의 COM ID
	// - ppDevice : 생성된 장치가 매개변수에 설정
#ifdef _DEBUG
	::D3D12GetDebugInterface(IID_PPV_ARGS(&_debugController));
	_debugController->EnableDebugLayer();
	if (_debugController) _debugController->Release();

#endif

	// DXGI(DirectX Graphics Infrastructure)
	// Direct3D와 함께 쓰이는 API
	// - 전체 화면 모드 전환
	// - 지원되는 디스플레이 모드 열거 등
	// CreateDXGIFactory
	// - riid : 디바이스의 COM ID
	// - ppDevice : 생성된 장치가 매개변수에 설정
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
	// - 디스플레이 어댑터(그래픽 카드)를 나타내는 객체
	// - pAdapter : nullptr 지정하면 시스템 기본 디스플레이 어댑터
	// - MinimumFeatureLevel : 응용 프로그램이 요구하는 최소 기능 수준 (구닥다리 걸러낸다)
	// - riid : 디바이스의 COM ID
	// - ppDevice : 생성된 장치가 매개변수에 설정
	::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device));
}

void JDevice::destroy()
{

}

J_RENDER_END