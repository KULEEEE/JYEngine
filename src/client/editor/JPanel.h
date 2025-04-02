#pragma once
#ifndef __JPANEL_H__
#define __JPANEL_H__

#include "Engine/precompile.h"
#include "Engine/JEngineContext.h"
#include "Engine/JCommandQueue.h"

/*#include "engine/asset/JShader.h*/ namespace J { namespace Render { class JShader; } }
/*#include "engine/asset/JMesh.h*/ namespace J { namespace Engine { class JMesh; } }
/*#include "engine/JRenderDefinition.h*/ namespace J { namespace Render { struct JPipeline; } }
/*#include "engine/JRenderDefinition.h*/ namespace J { namespace Render { struct JVertexBuffer; } }
/*#include "engine/JRenderDefinition.h*/ namespace J { namespace Render { struct JIndexBuffer; } }

J_EDITOR_BEGIN

class JPanel
{
public:

	JPanel() = default;
	~JPanel();
	void Init();
	void Update();

private:
	Render::JCommandQueue* _commandQueue;

	//TEMP: Test를 위한 임시 Resource를 저장해놓는다
	Render::JShader* shader = nullptr;
	Render::JPipeline* pipeline = nullptr;
	Engine::JMesh* mesh = nullptr;
	Render::JVertexBuffer* vertexBuffer = nullptr;
	Render::JIndexBuffer* indexBuffer = nullptr;	

};

J_EDITOR_END
#endif