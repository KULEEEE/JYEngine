#include "client/editor/JPanel.h"
#include "engine/JSwapChain.h"
#include "engine/JRenderDefinition.h"
#include "engine/JRenderResource.h"
#include "engine/JRenderContext.h"
#include "engine/asset/JShader.h"
#include "engine/asset/JMesh.h"

#include "client/editor/JFBXLoader.h"

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

	std::string shaderPath = get_Engine_Shader_Path();

	std::string baseShaderPath = shaderPath + "\\base.hlsl";

	shader = renderContext->CreateShader(baseShaderPath.c_str());
	pipeline = renderContext->CreatePipeline(shader);

	std::string meshPath = get_Engine_Mesh_Path();

	std::string cubeMeshPath = meshPath + "\\Cube.fbx";

	JFBXLoader fbxLoader;
	mesh = fbxLoader.LoadFBX(cubeMeshPath.c_str());

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
	_commandQueue->SetGraphicResources(shader);
	_commandQueue->BindVertexBuffer(&meshResource);
	_commandQueue->DrawIndexed(meshResource.indexSize, 1, 0, 0, 0);
	_commandQueue->EndRenderPass();
}

J_EDITOR_END


