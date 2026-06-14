# JYEngine
<img width="2551" height="1367" alt="image" src="https://github.com/user-attachments/assets/91c430fd-7e6a-4080-a908-efbb584b1e93" />

Data-Oriented Design을 기반으로 구현한 DirectX 12 커스텀 렌더링 엔진입니다. Scene/Component 데이터를 렌더링 전용 **Frame Snapshot**과 **Draw Item Cache**로 변환하고, 렌더러는 원본 Scene이 아닌 캐시된 **불변(immutable) 프레임 데이터(`JFrameDesc`) + `JRenderDB`** 만 참조하는 단방향 데이터 흐름 위에서 동작합니다. ECS-lite 컴포넌트 풀과 SoA 기반 Transform, 병렬 frustum culling, 셰이더 리플렉션 기반 root signature, Forward/Deferred 양쪽 경로(Cascade Shadow + Depth Prepass 포함)를 결합한 구조입니다.

```
Windows 10+  ·  Direct3D 12 (FL 11.0+)  ·  C++17  ·  Visual Studio + Win10 SDK
```

> GitHub: https://github.com/KULEEEE/JYEngine

---

## 동작 개요

엔진은 한 프레임을 **Authoring → Render Frontend → Render Backend** 세 단계로 처리합니다.

```
▣ Authoring · mutable
   JScene
    · Entity / Component pools
    · dirty transform / camera / light
        │  events
        ▼
▣ Render Frontend · CPU
   JRenderServer::SyncScene
    (consume events · update draw item cache · build snapshot)
        │
        ├──▶ JRenderSnapshotBuilder (카메라 view·projection, dirty transform → world, light snapshot)
        └──▶ JCameraRenderQueueBuilder (frustum 6-plane cull, visible opaque / transparent 인덱스)
        │
        ▼
   ── JFrameDesc (immutable) ──
        │
        ▼
▣ Render Backend · GPU
   JRenderDB                       material / mesh / camera / transform / light GPU mirror
        │
        ▼
   JRenderer::Render(JFrameDesc)
    · Forward  : SceneColorPass
    · Deferred : Shadow → Depth → GBuffer → SSAO → Lighting → ForwardOverlay
```

`JScene`의 변경사항은 이벤트(`ConsumeRenderObjectEvents`)와 dirty 인덱스(`ConsumeDirtyTransformIndices`, `ConsumeDirtyCameras`, `ConsumeDirtyLights`)로 누적되며, `JRenderServer::SyncScene`이 이를 소비해 `JFrameDesc`를 만듭니다. 이후 백엔드는 `JFrameDesc`를 `const`로 받아 mutable한 Scene 상태를 직접 참조하지 않고, GPU 리소스는 `JRenderDB`에 캐시된 mirror만 사용합니다.

---

## 주요 기능

### Data-Oriented Design

데이터 접근 패턴을 기준으로 메모리 배치를 설계해 cache hit를 높이고 불필요한 처리를 줄이는 것이 핵심입니다. 객체 단위 추상화 대신 매 프레임 반복되는 처리 흐름을 기준으로 데이터를 분리하고, 같은 계산에 필요한 데이터를 연속된 구조로 묶어 순차 접근이 가능하도록 구성했습니다. 모든 데이터를 매 프레임 다시 처리하지 않고, **변경된 데이터와 렌더링에 실제로 필요한 hot data만** 선별해 Snapshot / Draw Item Cache로 변환하므로, Scene의 mutable 데이터와 렌더러의 immutable 데이터가 분리됩니다.

### 단방향 데이터 흐름

`JScene`은 권위(authoritative) 모델이고, 백엔드 패스(`JRenderPass`)는 immutable `JFrameDesc`만 받습니다.

```cpp
// JRenderPass.h
virtual void Execute(const JRenderPassContext&, const JFrameDesc&) = 0;
```

Scene의 mutable 상태가 렌더 코드로 새지 않으므로 디버깅 경계가 명확하고, sorting queue 생성·snapshot 빌드 같은 단계의 병렬화가 쉬우며, 패스를 다른 스레드에서 record하거나 같은 `JFrameDesc`로 반복 재생(리플레이·헤드리스 렌더링)하는 확장이 구조적으로 가능합니다.

### ECS-lite 핸들 모델

엔티티와 컴포넌트는 `{index, generation}` 핸들로만 식별합니다. 풀에서 슬롯을 제거하면 generation이 증가하므로, 같은 인덱스를 가리키는 옛 핸들은 `IsValid()`에서 거짓이 됩니다.

```cpp
// JPool.h
bool IsValid(JEntityHandle handle) const {
    return handle.IsValid()
        && handle.index < _slots.size()
        && _slots[handle.index].active
        && _slots[handle.index].generation == handle.generation;
}
```

엔티티는 `JEntityPool<TData>`(free-list 재사용)가 관리하고, 각 컴포넌트는 `JEntityComponentPool<THandle, TData>`가 **entity index를 그대로 컴포넌트 슬롯 인덱스로** 사용해 관리합니다. 즉 한 엔티티당 같은 종류의 컴포넌트는 하나만 가질 수 있고, 컴포넌트 lookup은 해시 없이 인덱싱만으로 수행됩니다.

### SoA Transform + dirty index ring

`JTransformPool`은 translation / rotation / scale을 각각 `std::vector<JVec3>` 세 개로 분리해 저장합니다. 변경된 인덱스만 `_dirtyIndices`에 누적되며, `_dirtyFlags`로 중복 push를 방지합니다.

```cpp
// JTransformPool.h
std::vector<JVec3>  _translations;
std::vector<JVec3>  _rotations;
std::vector<JVec3>  _scales;
std::vector<uint8>  _dirtyFlags;
std::vector<uint32> _dirtyIndices;
```

스냅샷 빌더는 `ConsumeDirtyTransformIndices()`로 변경된 transform만 가져와 world matrix를 계산합니다.

### 병렬 frustum culling (chunk-local merge)

카메라마다 view·projection으로부터 6개의 평면을 추출하고, draw item에 대해 AABB 컬링을 수행합니다. worker 간 lock 경합을 피하기 위해, 각 청크는 **자기만의 로컬 결과 버퍼**에 push하고 메인 스레드가 마지막에 병합합니다.

```cpp
// JCameraRenderQueueBuilder.cpp
const uint32 chunkSize = std::max<uint32>(
    MIN_CHUNK_SIZE,
    (activeCount + targetChunkCount - 1) / targetChunkCount);

jobSystem.ParallelFor(activeCount, chunkSize, [&](uint32 begin, uint32 end) {
    ChunkResult& result = chunkResults[begin / chunkSize];
    // 로컬 버퍼에만 push
});
```

활성 draw item 수가 임계치(`MIN_CHUNK_SIZE` = 256) 이하이면 직렬 경로(`buildSerial`)로 자동 폴백합니다.

### Draw item incremental cache

`JDrawItemCache`는 한 엔티티가 만든 모든 sub-mesh draw item을 `JDrawRange { start, count, generation }`로 묶어 추적합니다. Scene의 이벤트를 받아:

- `Added`    → 새 entity의 draw item을 append
- `Modified` → 기존 range를 in-place로 patch
- `Removed`  → 전체 rebuild

매 프레임 전체 재구축을 피하기 위한 구조입니다.

### DirectX 12 추상화 레이어

DirectX 12의 explicit한 제어권은 유지하되, 상위 RenderPass가 Command Allocator, Descriptor Heap, Resource Upload, Root Binding을 직접 다루지 않도록 렌더링 abstraction layer를 구성했습니다.

- **`JRenderContext`** — GPU 리소스 생성 담당. `CreateVertexBuffer` / `CreateIndexBuffer` / `CreateConstantBuffer` / `CreateTextureFromFile` / `CreateShader` / `CreatePipeline` / `CreateRootSignature` 등을 제공하며, 내부에서 DEFAULT heap 생성, upload buffer 복사, resource barrier 전환, descriptor 생성 같은 DX12 절차를 처리합니다.
- **`JCommandQueue`** — 렌더링 명령 기록을 단순화. `RenderBegin` / `BeginRenderPass` / `BeginDepthPass` / `SetPipeline` / `SetGraphicResources` / `BindVertexBuffer` / `DrawIndexed` / `EndRenderPass` / `RenderEnd` 를 제공하고, render target 상태 전환·frame resource별 upload buffer·descriptor heap·fence 동기화를 내부에서 관리합니다.
- **`JGraphicResource`** — `SetConstantBuffer("PerObject", buffer)` / `SetTexture("AlbedoMap", texture)` 처럼 셰이더의 이름만 맞춰 바인딩을 연결합니다.

```cpp
commandQueue->BeginRenderPass(renderTarget, clearColor, 1);
commandQueue->SetPipeline(materialResource->pipeline);
commandQueue->SetGraphicResources(&graphicResource);
commandQueue->BindVertexBuffer(meshResource);
commandQueue->DrawIndexed(indexCount, 1, startIndex, 0, 0);
commandQueue->EndRenderPass();
```

### 셰이더 리플렉션 기반 root signature

`JShader::CompileShader`가 VS/PS 양쪽에 `D3DReflect`를 적용해 cbuffer / texture / sampler 바인딩을 수집하고, 이를 바탕으로 `JRenderContext::CreateRootSignature`가 root signature를 자동 생성합니다. 이름은 CRC32 해시로 보관됩니다.

```cpp
// JShader.cpp
for (uint32 i = 0; i < shaderDesc.BoundResources; ++i) {
    D3D12_SHADER_INPUT_BIND_DESC bindDesc;
    shaderReflection->GetResourceBindingDesc(i, &bindDesc);
    // D3D_SIT_CBUFFER / D3D_SIT_TEXTURE / D3D_SIT_SAMPLER 별로 분류
}
```

cbuffer는 root CBV로, texture는 descriptor table로 묶이며, sampler는 static sampler로 root signature에 박힙니다. 이름에 `Shadow`가 들어가면 비교(SampleCmp) 샘플러로 만들어집니다. 따라서 슬롯 인덱스를 코드로 지정하지 않고도 셰이더와 동일한 이름만 맞추면 바인딩이 연결됩니다.

### Per-frame upload ring + descriptor cache

`JCommandQueue`는 swap-chain 버퍼 수만큼 `FrameResource`를 보유합니다. 각 frame resource는

- 자체 command allocator
- **16 MB** 상수 버퍼 업로드 링 (CPU 매핑, `FRAME_UPLOAD_BUFFER_SIZE`)
- shader-visible CBV/SRV/UAV descriptor heap
- fence value

를 들고 있으며, `RenderBegin(frameIndex)`이 이전 프레임 fence를 기다린 뒤 offset을 0으로 리셋합니다. 같은 머터리얼이 한 프레임 안에서 여러 번 그려질 때를 위해 `(shader, texture set)` 키로 GPU descriptor table을 캐시합니다.

```cpp
// JCommandQueue.cpp
const auto cached = frameResource.descriptorTableCache.find(tableKey);
if (cached != frameResource.descriptorTableCache.end()) {
    return cached->second;
}
```

### Forward / Deferred 양쪽 지원

`JRenderer::SetRenderPath(RenderPath::Forward | Deferred)`로 런타임 전환됩니다.

- **Forward**  : `JSceneColorPass`가 opaque → transparent 순으로 draw
- **Deferred** : `JShadowPass` → `JDepthPass`(reverse-Z pre-pass) → `JGBufferPass` (Albedo / Normal / Material MRT) → `JSSAOPass` → `JLightingPass` (full-screen triangle로 G-buffer 샘플링 + IBL) → `JForwardOverlayPass`

### Cascade Shadow Map (CSM)

`JShadowMap`은 directional light용 **4-cascade** shadow map을 `Texture2DArray` 한 장(cascade별 DSV)으로 관리합니다. `JShadowPass`가 카메라 프러스텀을 거리 비율(`{0.05, 0.16, 0.42, 1.0}`)로 분할해 cascade별 view·projection을 매 프레임 채우고, `JLightingPass`가 이를 cbuffer로 올려 PCF로 샘플링합니다. 전 구간 **reverse-Z**(near=1, far=0)를 사용하며, 비교 샘플러 + border 처리로 맵 밖은 항상 lit로 처리됩니다. cascade별 depth bias는 world 단위 값을 reverse-Z depth 단위로 환산해 적용합니다.

### 셰이딩 (PBR / ORM / ACES)

- **ORM 패킹 머터리얼** : `R=Occlusion, G=Roughness, B=Metalness` (Bistro/glTF 컨벤션) 텍스처를 G-buffer의 Material MRT에 기록합니다.
- **ACES filmic tonemapping** (Narkowicz 2015 근사) 으로 lighting 결과를 톤매핑합니다.
- **derivative 기반 normal mapping** : 화면 미분으로 TBN을 구성해 별도 tangent 입력 없이 노멀 맵을 적용합니다.
- 간접광(ambient)은 아래 **IBL**로, 차폐는 **SSAO**로 계산해 간접광에만 적용합니다.

### 이미지 기반 조명 (IBL) — 씬 캡처 reflection probe

정적 씬의 간접광을 위해 **씬을 직접 캡처해 split-sum IBL**을 굽습니다. `JReflectionProbe`가 로드 후 1회 수행합니다.

1. **씬 캡처** — probe(주 카메라) 위치에서 6면을 reverse-Z 90° FOV로 HDR 큐브(`R16G16B16A16F`)에 렌더합니다. immutable `JFrameDesc`를 면마다 재구성해 같은 deferred 경로를 재사용하며, 톤매핑 전 **linear HDR**로 기록합니다.
2. **irradiance 큐브** — 캡처 큐브를 코사인 적분 → diffuse GI
3. **prefiltered 큐브** — roughness별 GGX 프리필터(밉체인) → specular 반사
4. **BRDF LUT** — split-sum의 F항 적분 (환경 무관, full-screen 1회 bake)

`JLightingPass`의 ambient를 `kD·irradiance·albedo + prefiltered·(F·brdf.x + brdf.y)`로 계산합니다. compute 파이프라인이 없어 모든 컨볼루션은 **render-to-cube-face 패스**로 처리하고, 결과 큐브는 런타임 내내 상주해 매 프레임 샘플만 합니다(정적 씬 1회 bake). 지오메트리가 없는 픽셀에는 절차적 하늘을 그려 **skybox 배경 겸 IBL 상반구 소스**로 활용합니다. 이름 규약 샘플러에 `LUT`(clamp) 규칙을 추가해 BRDF LUT 가장자리 누수를 막습니다.

### SSAO

`JSSAOPass`가 G-buffer의 depth·normal로 화면 공간 차폐를 단일 채널 타겟에 그리고, `JLightingPass`가 이를 박스 블러로 읽어 **간접광에만** 곱합니다. 샘플 반경은 **월드 공간 기준**으로 화면 픽셀에 환산해, 거리와 무관하게 일정한 차폐 footprint를 유지합니다(원거리 얼룩 방지).

### JSON 직렬화 / 프로젝트 포맷

`JSceneSerializer`가 nlohmann/json으로 씬을 사람이 읽기 쉬운 명시적 JSON으로 저장 / 로드합니다. 알 수 없는 필드는 무시되고, 누락된 필드는 기본값으로 채워집니다(`value("key", default)` 패턴). 씬은 `materials` / `meshes` / `entities` 배열로 분리되며, 각 엔티티는 `stableID`, `name`, `transform`, `camera`, `light`, `renderObjectComponent` 같은 의미 있는 필드명을 가집니다.

```json
{
  "stableID": "ent_camera_001",
  "name": "MainCamera",
  "transform": { "translation": [0, 3, -5], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
  "camera": { "primary": true, "nearP": 0.1, "farP": 1000.0 }
}
```

프로젝트는 `project.json`(`name`, `startupScene`)으로 묶이며, 씬 파일은 `*.scene.json`으로 저장됩니다. 이 저장 포맷은 런타임의 Pool/Handle 기반 Data-Oriented 구조와 분리되어 있습니다 — 실행 시에는 cache 효율을 위한 데이터 배치를, 저장·편집 시에는 가독성과 자동화 가능성을 우선합니다.

### AI 친화적 설계

추후 AI Agent를 엔진에 연결하는 것을 고려해, AI가 이해·확장하기 쉬운 형태로 아키텍처를 설계했습니다. Scene / Snapshot / RenderDB / Renderer / RenderPass의 역할을 명확히 분리하고 데이터 흐름을 단방향에 가깝게 구성해 각 단계의 입력·출력을 추적하기 쉽게 했고, 저수준 DX12 API를 `JRenderContext` / `JCommandQueue` / `JGraphicResource` 같은 자체 API로 래핑해 엔진 레벨 인터페이스만으로 기능을 추가·수정할 수 있게 했습니다. 또한 씬을 binary가 아닌 의미 중심의 JSON으로 저장해, AI가 특정 Entity의 transform이나 mesh/material 연결을 분석·편집하기 쉽도록 했습니다.

### FBX 임포터 + 텍스처 쿠킹 (별도 실행 파일)

`JAssetImporter`는 OpenFBX를 사용해 FBX → 메시 / 머터리얼 / 텍스처 / 씬 JSON으로 변환하는 별도 실행 파일입니다. 텍스처는 임포트 시 `.jtex` 컨테이너(밉체인·압축 포맷 포함)로 구워지고, 런타임은 전용 리더로 그대로 GPU에 올립니다. 런타임에 FBX 의존을 두지 않기 위해 빌드와 임포트를 분리했습니다.

```
JAssetImporter <source.fbx> <projectDir>
```

---

## 모듈 구성

```
src/
├── engine/
│   ├── core/        JEngine, JEngineContext, JJobSystem, JPool, JHashFunction, JObject
│   ├── scene/       JScene, JTransformPool, JCameraPool, JLightPool,
│   │                JRenderObjectComponentPool, JSceneSerializer, JSceneHandle
│   ├── render/      JRenderer, JRenderServer, JRenderDB, JRenderSnapshotBuilder,
│   │                JCameraRenderQueueBuilder, JDrawItemCache, JMaterialFactory,
│   │                JCommandQueue, JSwapChain, JRenderContext, JRenderResource,
│   │                JGraphicResource, JRenderTarget, JGBuffer,
│   │                패스: SceneColor / Shadow / Depth / GBuffer / SSAO / Lighting / ForwardOverlay,
│   │                JShadowMap (CSM), JReflectionProbe (IBL)
│   ├── asset/       JMaterial, JMesh, JShader, JTextureFile (.jtex)
│   ├── platform/    JDevice
│   └── dx12/        JDx12Helper, d3dx12 wrapper
├── client/
│   ├── editor/      JSceneManager, JScenePanel, JSceneBuilder, JImGuiLayer,
│   │                JFBXLoader, JAssetManager
│   └── OpenFBX/     vendored OpenFBX
tools/
└── JAssetImporter/  FBX → 프로젝트 JSON/.jtex 변환기
samples/
└── GaussianSmoke/   파티클 스모크 데모 (별도 실행 파일)
SampleProjects/
└── BistroExterior/  project.json 기반 샘플 프로젝트 (deferred + CSM)
benchmark/           OOP / AoS / SoA / WorldMatrix 데이터 레이아웃 비교 (옵션)
third_party/
├── nlohmann/json
└── imgui            에디터 UI
```

빌드 타겟은 다음과 같습니다.

| 타겟                  | 형태          | 비고                                       |
|----------------------|--------------|-------------------------------------------|
| `Engine`             | static lib   | DX12 / DXGI / D3DCompiler / WindowsCodecs |
| `Client`             | WIN32 exe    | 에디터/뷰어를 겸하는 호스트 응용 (ImGui)      |
| `JAssetImporter`     | console exe  | FBX → 프로젝트/.jtex 변환 도구              |
| `GaussianSmokeSample`| WIN32 exe    | 파티클 스모크 데모                           |
| `Benchmark`          | console exe  | `BUILD_BENCHMARK=ON` 옵션. oneTBB FetchContent |

---

## 빌드

### 요구 사항

- Windows 10 이상
- Direct3D 12 지원 GPU (`D3D_FEATURE_LEVEL_11_0`+)
- Visual Studio 2019/2022 + C++ desktop workload, Windows 10 SDK
- 선택: *Graphics Tools* (D3D12 debug layer)

### 빌드 절차

```bat
cmake -S . -B build
cmake --build build --config Release
```

벤치마크까지 빌드하려면:

```bat
cmake -S . -B build -DBUILD_BENCHMARK=ON
cmake --build build --config Release
```

`Client`가 Visual Studio의 기본 시작 프로젝트로 설정되어 있습니다.

---

## 실행

`Client` 실행 시 명령줄 옵션:

```
Client.exe                       # 기본 샘플 씬 (res/scene/sample.jscene.json)
Client.exe --new                 # 빈 씬으로 시작 (-n)
Client.exe --scene <path>        # 특정 씬 파일 (-s)
Client.exe --project <path>      # 프로젝트 폴더 (-p, project.json 탐색)
Client.exe <projectPath>         # 위치 인자도 프로젝트 폴더로 해석
```

예: 동봉된 Bistro 샘플 프로젝트 열기

```
Client.exe --project SampleProjects/BistroExterior
```

FBX를 프로젝트로 임포트:

```
JAssetImporter <source.fbx> <projectDir>
```

`<projectDir>` 아래에 `scenes/`, `meshes/`, `materials/`, `textures/`, `shader/` 디렉터리가 생성되고, 머터리얼·메시는 JSON, 텍스처는 `.jtex`로 저장됩니다.

---

## 외부 코드

- [nlohmann/json](https://github.com/nlohmann/json) — `third_party/nlohmann/`
- [Dear ImGui](https://github.com/ocornut/imgui) — `third_party/imgui/` (에디터 UI)
- [OpenFBX](https://github.com/nem0/OpenFBX) — `src/client/OpenFBX/`
- [oneTBB](https://github.com/oneapi-src/oneTBB) — 벤치마크 타겟 한정, CMake FetchContent로 가져옴

각 라이브러리의 라이선스를 따릅니다.
