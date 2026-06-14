#pragma once
#ifndef __J_REFLECTION_PROBE_H__
#define __J_REFLECTION_PROBE_H__

#include "engine/precompile.h"

/*#include "engine/render/JRenderContext.h"*/ namespace J { namespace Render { class JRenderContext; class JCommandQueue; class JShader; struct JPipeline; struct JTexture; } }

J_ENGINE_BEGIN

class JRenderTarget;

// 정적 씬용 baked IBL 리소스 모음.
// 씬 로드 후 1회 bake하고, 결과 텍스처를 런타임 내내 들고 LightingPass에서 샘플만 한다.
// 구성: BRDF LUT(환경 무관) + 씬 캡처 HDR 큐브 → irradiance(diffuse) / prefiltered(specular) 큐브.
class JReflectionProbe
{
public:
	JReflectionProbe();
	~JReflectionProbe();

	// lazy 1회 bake(환경 무관 LUT). 이미 구워졌으면 즉시 true.
	bool EnsureBaked(Render::JRenderContext* renderContext);

	// 씬 캡처용 HDR 큐브 타겟을 lazy 생성. JRenderer가 6면을 여기에 렌더한다.
	bool EnsureCaptureCube();
	JRenderTarget* GetCaptureCube() const { return _captureCube.get(); }
	uint32 GetCaptureCubeSize() const;
	bool IsCaptured() const { return _captured; }
	void MarkCaptured() { _captured = true; }

	// 캡처 큐브를 코사인 적분해 irradiance 큐브를 기록한다. (capture 직후, 같은 frame command list)
	bool RecordIrradiance(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext);

	// 캡처 큐브를 roughness별로 prefilter해 specular IBL 큐브(밉체인)를 기록한다.
	bool RecordPrefiltered(Render::JCommandQueue* commandQueue, Render::JRenderContext* renderContext);
	uint32 GetPrefilteredMipCount() const;

	// 라이팅이 샘플하는 IBL 맵들. (준비 전이면 nullptr)
	const Render::JTexture* GetBRDFLUT() const;     // split-sum specular의 F항 적분 LUT
	const Render::JTexture* GetIrradiance() const;  // diffuse IBL
	const Render::JTexture* GetPrefiltered() const; // specular IBL

private:
	bool bakeBRDFLUT(Render::JRenderContext* renderContext);
	bool ensureIrradianceResources(Render::JRenderContext* renderContext);
	bool ensurePrefilteredResources(Render::JRenderContext* renderContext);

	std::unique_ptr<JRenderTarget> _brdfLUT;
	Render::JShader* _brdfShader = nullptr;
	Render::JPipeline* _brdfPipeline = nullptr;
	bool _baked = false;

	std::unique_ptr<JRenderTarget> _captureCube;
	bool _captured = false;

	std::unique_ptr<JRenderTarget> _irradianceCube;
	Render::JShader* _irradianceShader = nullptr;
	Render::JPipeline* _irradiancePipeline = nullptr;
	bool _irradianceReady = false;

	std::unique_ptr<JRenderTarget> _prefilteredCube;
	Render::JShader* _prefilterShader = nullptr;
	Render::JPipeline* _prefilterPipeline = nullptr;
	bool _prefilteredReady = false;
};

J_ENGINE_END

#endif
