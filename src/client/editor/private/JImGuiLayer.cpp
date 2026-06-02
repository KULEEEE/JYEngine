#include "client/editor/JImGuiLayer.h"

#include "engine/render/JCommandQueue.h"
#include "engine/render/JRenderTarget.h"

#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

#include <cassert>
#include <iostream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

J_EDITOR_BEGIN

namespace
{
	// 현재는 font texture 정도만 필요하지만, 추후 editor texture preview용 여유분 둠
	constexpr uint32 IMGUI_DESCRIPTOR_COUNT = 64;
}

JImGuiLayer::~JImGuiLayer()
{
	Shutdown();
}

bool JImGuiLayer::Init(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue, DXGI_FORMAT rtvFormat)
{
	if (_initialized)
	{
		return true;
	}

	if (hwnd == nullptr || device == nullptr || commandQueue == nullptr)
	{
		std::cerr << "JImGuiLayer::Init failed: hwnd, device, or command queue is null." << std::endl;
		return false;
	}

	if (!createDescriptorHeap(device))
	{
		return false;
	}

	// ImGui context와 backend는 전부 client/editor 소유임
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	_contextCreated = true;
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 4.0f;
	style.FrameRounding = 3.0f;
	style.GrabRounding = 3.0f;

	if (!ImGui_ImplWin32_Init(hwnd))
	{
		std::cerr << "JImGuiLayer::Init failed: Win32 backend initialization failed." << std::endl;
		Shutdown();
		return false;
	}
	_win32BackendInitialized = true;

	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = device;
	initInfo.CommandQueue = commandQueue;
	initInfo.NumFramesInFlight = SWAP_CHAIN_BUFFER_COUNT;
	initInfo.RTVFormat = rtvFormat;
	initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
	initInfo.SrvDescriptorHeap = _srvDescriptorHeap.Get();
	initInfo.UserData = &_srvAllocator;
	// backend가 font/texture SRV를 필요로 할 때 에디터 heap에서 할당함
	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
	{
		static_cast<DescriptorAllocator*>(info->UserData)->Alloc(outCpuHandle, outGpuHandle);
	};
	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
	{
		static_cast<DescriptorAllocator*>(info->UserData)->Free(cpuHandle, gpuHandle);
	};

	if (!ImGui_ImplDX12_Init(&initInfo))
	{
		std::cerr << "JImGuiLayer::Init failed: DX12 backend initialization failed." << std::endl;
		Shutdown();
		return false;
	}
	_dx12BackendInitialized = true;

	_initialized = true;
	return true;
}

void JImGuiLayer::Shutdown()
{
	// 생성 역순으로 정리함
	if (_dx12BackendInitialized)
	{
		ImGui_ImplDX12_Shutdown();
		_dx12BackendInitialized = false;
	}

	if (_win32BackendInitialized)
	{
		ImGui_ImplWin32_Shutdown();
		_win32BackendInitialized = false;
	}

	if (_contextCreated)
	{
		ImGui::DestroyContext();
		_contextCreated = false;
	}

	_initialized = false;
	_srvAllocator.Destroy();
	_srvDescriptorHeap.Reset();
}

void JImGuiLayer::BeginFrame()
{
	if (!_initialized)
	{
		return;
	}

	// backend별 입력/렌더 상태를 갱신한 뒤 ImGui frame 시작함
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void JImGuiLayer::Render(Engine::JRenderTarget* renderTarget, Render::JCommandQueue* commandQueue)
{
	if (!_initialized || renderTarget == nullptr || commandQueue == nullptr || !renderTarget->IsValid())
	{
		return;
	}

	ImGui::Render();

	// Scene 렌더 결과 위에 clear 없이 ImGui만 덧그림
	commandQueue->BeginRenderPass(renderTarget, renderTarget->GetClearColor(), 0, false);
	ID3D12DescriptorHeap* descriptorHeaps[] = { _srvDescriptorHeap.Get() };
	commandQueue->GetCmdList()->SetDescriptorHeaps(1, descriptorHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandQueue->GetCmdList().Get());
	commandQueue->EndRenderPass();
}

LRESULT JImGuiLayer::HandleWin32Message(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam);
}

bool JImGuiLayer::createDescriptorHeap(ID3D12Device* device)
{
	// ImGui backend가 접근할 수 있게 shader-visible heap으로 생성함
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.NumDescriptors = IMGUI_DESCRIPTOR_COUNT;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NodeMask = 0;

	const HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_srvDescriptorHeap));
	if (FAILED(hr))
	{
		std::cerr << "JImGuiLayer::createDescriptorHeap failed. HRESULT=0x" << std::hex << hr << std::dec << std::endl;
		return false;
	}

	_srvAllocator.Create(device, _srvDescriptorHeap.Get());
	return true;
}

void JImGuiLayer::DescriptorAllocator::Create(ID3D12Device* device, ID3D12DescriptorHeap* descriptorHeap)
{
	heap = descriptorHeap;
	heapStartCpu = heap->GetCPUDescriptorHandleForHeapStart();
	heapStartGpu = heap->GetGPUDescriptorHandleForHeapStart();
	descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	freeIndices.clear();
	freeIndices.reserve(IMGUI_DESCRIPTOR_COUNT);
	// 뒤에서 pop_back 하려고 역순으로 free index 채움
	for (uint32 index = IMGUI_DESCRIPTOR_COUNT; index > 0; --index)
	{
		freeIndices.push_back(index - 1);
	}
}

void JImGuiLayer::DescriptorAllocator::Destroy()
{
	heap = nullptr;
	heapStartCpu = {};
	heapStartGpu = {};
	descriptorSize = 0;
	freeIndices.clear();
}

void JImGuiLayer::DescriptorAllocator::Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
	assert(outCpuHandle != nullptr && outGpuHandle != nullptr);
	assert(!freeIndices.empty());

	const uint32 index = freeIndices.back();
	freeIndices.pop_back();
	// 같은 index를 CPU/GPU handle 양쪽에 적용함
	outCpuHandle->ptr = heapStartCpu.ptr + static_cast<SIZE_T>(index) * descriptorSize;
	outGpuHandle->ptr = heapStartGpu.ptr + static_cast<UINT64>(index) * descriptorSize;
}

void JImGuiLayer::DescriptorAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
	const uint32 cpuIndex = static_cast<uint32>((cpuHandle.ptr - heapStartCpu.ptr) / descriptorSize);
	const uint32 gpuIndex = static_cast<uint32>((gpuHandle.ptr - heapStartGpu.ptr) / descriptorSize);
	assert(cpuIndex == gpuIndex);
	freeIndices.push_back(cpuIndex);
}

J_EDITOR_END
