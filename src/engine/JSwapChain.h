#pragma once
#ifndef __J_SWAPCHAIN_H__
#define	__J_SWAPCHAIN_H__

#include "engine/JRenderDefinition.h"
#include "engine/JRenderTarget.h"

/*#include "engine/JDevice.h"*/ namespace J { namespace Render { class JDevice; } }

// ��ȯ �罽
// [���� ����]
// - ���� ���� ���� �ִ� ��Ȳ�� ����
// - � �������� ��� ������� ������
// - GPU�� ������ ��� (����)
// - ����� �޾Ƽ� ȭ�鿡 �׷��ش�

// [���� �����]�� ��� ����?
// - � ����(Buffer)�� �׷��� �ǳ��޶�� ��Ź�غ���
// - Ư�� ���̸� ���� -> ó���� �ǳ��ְ� -> ������� �ش� ���̿� �޴´� OK
// - �츮 ȭ�鿡 Ư�� ����(���� �����) ������ش�

// [?]
// - �׷��� ȭ�鿡 ���� ����� ����ϴ� ���߿�, ���� ȭ�鵵 ���ָ� �ðܾ� ��
// - ���� ȭ�� ������� �̹� ȭ�� ��¿� �����
// - Ư�� ���̸� 2�� ����, �ϳ��� ���� ȭ���� �׷��ְ�, �ϳ��� ���� �ñ��...
// - Double Buffering!

// - [0] [1]
// ���� ȭ�� [1]  <-> GPU �۾��� [1] BackBuffer

J_RENDER_BEGIN

class JSwapChain
{
public:
	JSwapChain();
	~JSwapChain();

	void Initialize(const JWindowInfo& info, const JDevice* device, ComPtr<ID3D12CommandQueue> cmdQueue);
	
	void Present();
	void SwapIndex();

	ComPtr<IDXGISwapChain> GetSwapChain() { return _swapChain; }
	Engine::JRenderTarget* GetRenderTarget() { return _renderTargets[_backBufferIndex]; }

private:
	void createSwapChain(const JWindowInfo& info, ComPtr<IDXGIFactory> dxgi, ComPtr<ID3D12CommandQueue> cmdQueue);

	ComPtr<IDXGISwapChain>	_swapChain;

	Engine::JRenderTarget*			_renderTargets[SWAP_CHAIN_BUFFER_COUNT];

	uint32					_backBufferIndex = 0;
};

J_RENDER_END
#endif