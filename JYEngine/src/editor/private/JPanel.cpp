#include "editor/JPanel.h"
#include "engine/JSwapChain.h"
#include "engine/JRenderDefinition.h"
#include "engine/JRenderResource.h"
#include "engine/JRenderContext.h"
#include "engine/asset/JShader.h"
#include "engine/asset/JMesh.h"

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
	Render::JRenderContext* renderContext = GetEngine()->GetRenderContext();

	Render::JShader* shader = renderContext->CreateShader("C:\\JYEngine\\JYEngine\\src\\shader\\test.hlsl");
	Render::JPipeline* pipeline = renderContext->CreatePipeline(shader);

	
	Engine::JMeshResource meshResource;
	std::vector<float> positions = {
		-0.5f, -0.5f, 0.0f, 1.0f,
		0.0f,  0.5f, 0.0f, 1.0f,
		0.5f, -0.5f, 0.0f, 1.0f
	};

	std::vector<float> colors = {
		1.0f, 0.0f, 0.0f, 1.0f,
		0.0f, 1.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f, 1.0f
	};

	Engine::JMesh mesh;
	mesh.SetPositions(std::move(positions));
	mesh.SetColors(std::move(colors));

	Render::JVertexBuffer* vertexBuffer = renderContext->CreateVertexBuffer(mesh.GetPositions(), 3);
	Render::JVertexBuffer* colorBuffer = renderContext->CreateVertexBuffer(mesh.GetColors(), 3);
	
	meshResource.soaBuffers.push_back(vertexBuffer->view);
	meshResource.soaBuffers.push_back(colorBuffer->view);
	meshResource.vertexCount = 3;

	_commandQueue->BeginRenderPass(swapChain->GetRenderTarget(), JColors::DarkGray, 0);

	Render::JViewport viewport = { 0, 0, 800, 600, 0, 1 };

	D3D12_RECT rect = CD3DX12_RECT(0, 0, 800, 600);

	_commandQueue->SetViewports(1, &viewport);
	_commandQueue->SetScissorRects(1, &rect);
	_commandQueue->SetPipeline(pipeline);
	_commandQueue->BindVertexBuffer(&meshResource);
	_commandQueue->EndRenderPass();
}

J_EDITOR_END


