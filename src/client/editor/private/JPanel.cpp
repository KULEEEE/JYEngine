#include "client/editor/JPanel.h"
#include "engine/JSwapChain.h"
#include "engine/JRenderDefinition.h"
#include "engine/JRenderResource.h"
#include "engine/JRenderContext.h"
#include "engine/asset/JShader.h"
#include "engine/asset/JMesh.h"

#include "client/editor/JFBXLoader.h"

#include <iostream>

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
	if (renderContext == nullptr)
	{
		std::cerr << "JPanel::Init failed: render context is null." << std::endl;
		return;
	}

	std::string shaderPath = get_Engine_Shader_Path();

	std::string baseShaderPath = shaderPath + "\\base.hlsl";

	shader = renderContext->CreateShader(baseShaderPath.c_str());
	if (shader == nullptr)
	{
		std::cerr << "JPanel::Init failed: shader load failed." << std::endl;
		return;
	}

	pipeline = renderContext->CreatePipeline(shader);
	if (pipeline == nullptr)
	{
		std::cerr << "JPanel::Init failed: pipeline creation failed." << std::endl;
		return;
	}

	std::string meshPath = get_Engine_Mesh_Path();

	std::string cubeMeshPath = meshPath + "\\Car.fbx";
	std::cout << "Loading mesh: " << cubeMeshPath << std::endl;

	JFBXLoader fbxLoader;
	mesh = fbxLoader.LoadFBX(cubeMeshPath.c_str());
	if (mesh == nullptr)
	{
		std::cerr << "JPanel::Init failed: mesh load failed." << std::endl;
		return;
	}

	vertexBuffer = renderContext->CreateVertexBuffer(mesh->GetPositions(), mesh->GetVertexCount());
	indexBuffer = renderContext->CreateIndexBuffer(mesh->GetIndices());
	if (vertexBuffer == nullptr || indexBuffer == nullptr)
	{
		std::cerr << "JPanel::Init failed: buffer creation failed." << std::endl;
		return;
	}

	_isReady = true;
}

void JPanel::Update()
{
	if (!_isReady)
	{
		return;
	}

	Render::JSwapChain* swapChain = GetEngine()->GetSwapChain();
	if (swapChain == nullptr || pipeline == nullptr || shader == nullptr || mesh == nullptr || vertexBuffer == nullptr || indexBuffer == nullptr)
	{
		std::cerr << "JPanel::Update skipped: render resources are not ready." << std::endl;
		_isReady = false;
		return;
	}
	
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


