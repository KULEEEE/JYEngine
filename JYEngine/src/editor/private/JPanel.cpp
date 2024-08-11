#include "editor/JPanel.h"
#include "engine/JSwapChain.h"
#include "engine/JRenderDefinition.h"
#include "engine/JRenderResource.h"
#include "engine/JRenderContext.h"
#include "engine/asset/JShader.h"
#include "engine/asset/JMesh.h"

#include "editor/JFBXLoader.h"

J_EDITOR_BEGIN

JPanel::~JPanel()
{
	if (shader != nullptr)
	{
		delete shader;
		shader = nullptr;
	}

	if(pipeline != nullptr)
	{
		delete pipeline;
		pipeline = nullptr;
	}

	if(vertexBuffer != nullptr)
	{
		delete vertexBuffer;
		vertexBuffer = nullptr;
	}

	if(indexBuffer != nullptr)
	{
		delete indexBuffer;
		indexBuffer = nullptr;
	}

	if(mesh != nullptr)
	{
		delete mesh;
		mesh = nullptr;
	}
}

void JPanel::Init()
{
	_commandQueue = GetEngine()->GetCmdQueue();

	Render::JRenderContext* renderContext = GetEngine()->GetRenderContext();

	shader = renderContext->CreateShader("C:\\JYEngine\\JYEngine\\res\\shader\\test.hlsl");
	pipeline = renderContext->CreatePipeline(shader);

	JFBXLoader fbxLoader;
	mesh = fbxLoader.LoadFBX("C:\\JYEngine\\JYEngine\\res\\mesh\\Cube.fbx");

	vertexBuffer = renderContext->CreateVertexBuffer(mesh->GetPositions(), mesh->GetVertexCount());
	indexBuffer = renderContext->CreateIndexBuffer(mesh->GetIndices());
}

void JPanel::Update()
{
	Render::JSwapChain* swapChain = GetEngine()->GetSwapChain();
	
	_commandQueue->BeginRenderPass(swapChain->GetRenderTarget(), JColors::DarkGray, 0);

	Render::JViewport viewport = { 0, 0, 800, 600, 0, 1 };

	D3D12_RECT rect = CD3DX12_RECT(0, 0, 800, 600);

	Engine::JMeshResource meshResource;
	meshResource.soaBuffers.push_back(vertexBuffer->view);
	meshResource.indexBuffer = indexBuffer->view;
	meshResource.indexSize = mesh->GetIndices().size();

	_commandQueue->SetViewports(1, &viewport);
	_commandQueue->SetScissorRects(1, &rect);
	_commandQueue->SetPipeline(pipeline);
	_commandQueue->BindVertexBuffer(&meshResource);
	_commandQueue->DrawIndexed(meshResource.indexSize, 1, 0, 0, 0);
	_commandQueue->EndRenderPass();
}

J_EDITOR_END


