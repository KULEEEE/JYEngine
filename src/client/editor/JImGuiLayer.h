#pragma once
#ifndef __J_IMGUI_LAYER_H__
#define __J_IMGUI_LAYER_H__

#include "engine/precompile.h"

namespace J::Render
{
	class JCommandQueue;
}

namespace J::Engine
{
	class JRenderTarget;
}

J_EDITOR_BEGIN

class JImGuiLayer
{
public:
	JImGuiLayer() = default;
	~JImGuiLayer();

	JImGuiLayer(const JImGuiLayer&) = delete;
	JImGuiLayer& operator=(const JImGuiLayer&) = delete;

	bool Init(HWND hwnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue, DXGI_FORMAT rtvFormat);
	void Shutdown();
	void BeginFrame();
	void Render(Engine::JRenderTarget* renderTarget, Render::JCommandQueue* commandQueue);
	bool IsInitialized() const { return _initialized; }

	// Win32 입력을 ImGui backend로 넘기는 입구임
	static LRESULT HandleWin32Message(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	// ImGui 전용 SRV descriptor heap에서 descriptor를 빌려주는 간단한 free-list임
	struct DescriptorAllocator
	{
		ID3D12DescriptorHeap* heap = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE heapStartCpu = {};
		D3D12_GPU_DESCRIPTOR_HANDLE heapStartGpu = {};
		uint32 descriptorSize = 0;
		std::vector<uint32> freeIndices;

		void Create(ID3D12Device* device, ID3D12DescriptorHeap* descriptorHeap);
		void Destroy();
		void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle);
		void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);
	};

	bool createDescriptorHeap(ID3D12Device* device);

	// 엔진 descriptor heap이랑 섞지 않는 에디터 전용 heap임
	ComPtr<ID3D12DescriptorHeap> _srvDescriptorHeap;
	DescriptorAllocator _srvAllocator;

	// 실패 경로에서도 생성된 것만 정리하려고 backend별 상태를 따로 둠
	bool _contextCreated = false;
	bool _win32BackendInitialized = false;
	bool _dx12BackendInitialized = false;
	bool _initialized = false;
};

J_EDITOR_END

#endif
