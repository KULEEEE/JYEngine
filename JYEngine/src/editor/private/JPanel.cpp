#include "editor/JPanel.h"
#include "engine/JSwapChain.h"
#include "engine/JRenderDefinition.h"

J_EDITOR_BEGIN

JPanel::~JPanel()
{
}

void JPanel::Init()
{
	_commandQueue = GetEngine()->GetCmdQueue();
}

void JPanel::Update()
{
	Render::JSwapChain* swapChain = GetEngine()->GetSwapChain();
	
	_commandQueue->ClearRenderTargetView(swapChain->GetRenderTarget(), JColors::DarkGray, 1);
	_commandQueue->BeginRenderPass(swapChain->GetRenderTarget());
	Render::JViewport viewport = { 0, 0, 800, 600, 0, 1 };

	D3D12_RECT rect = CD3DX12_RECT(0, 0, 800, 600);

	_commandQueue->SetViewports(1, &viewport);
	_commandQueue->SetScissorRects(1, &rect);
	_commandQueue->EndRenderPass();
}

J_EDITOR_END


