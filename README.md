# JYEngine

DirectX 12 기반의 미니 렌더링 엔진입니다. Scene 권위 모델과 GPU 렌더러 사이를 **불변(immutable) 프레임 스냅샷**으로 분리한 단방향 데이터 흐름 위에서, ECS-lite 컴포넌트 풀과 SoA 기반 Transform, 병렬 frustum culling, 셰이더 리플렉션 기반 root signature를 결합한 구조로 동작합니다.

```
Windows 10+  ·  Direct3D 12 (FL 11.0+)  ·  C++17  ·  Visual Studio + Win10 SDK
```

---

## 동작 개요

엔진은 한 프레임을 세 단계로 처리합니다.

```
[Authoring · mutable]            [Render Frontend · CPU]              [Render Backend · GPU]

  JScene  ──events──▶  JRenderServer::SyncScene
                          │
                          ├──▶ JRenderSnapshotBuilder
                          │     · 카메라 view·projection
                          │     · dirty transform → world matrix
                          │     · light snapshot
                          │
                          ├──▶ JDrawItemCache 증분 갱신
                          │     · Added / Modified → patch
                          │     · Removed          → rebuild
                          │
                          └──▶ JCameraRenderQueueBuilder
                                · frustum 6-plane 추출
                                · ParallelFor 기반 AABB 컬링
                          ▼
                  ── JFrameDesc (immutable) ──
                          ▼
                JRenderer::Render(frameDesc)
                          │
                          ├──▶ prepareFrameResources
                          │     · Material / Camera / Transform / Light 동기화
                          │
                          └──▶ pass[i].Execute()
                                · Forward  : SceneColorPass
                                · Deferred : GBufferPass → LightingPass → ForwardOverlayPass
```

`JScene`의 변경사항은 이벤트(`ConsumeRenderObjectEvents`)와 dirty 인덱스(`ConsumeDirtyTransformIndices`, `ConsumeDirtyCameras`, `ConsumeDirtyLights`)로 누적되며, `JRenderServer::SyncScene`이 이를 소비해 `JFrameDesc`를 만듭니다. 이후 백엔드는 `JFrameDesc`를 `const`로 받아 mutable한 Scene 상태를 직접 참조하지 않습니다.

---

## 주요 기능

### 단방향 데이터 흐름

`JScene`은 권위(authoritative) 모델이고, 백엔드 패스(`JRenderPass`)는 immutable `JFrameDesc`만 받습니다.

```cpp
// JRenderPass.h
virtual void Execute(const JRenderPassContext&, const JFrameDesc&) = 0;
```

Scene의 mutable 상태가 렌더 코드로 새지 않으므로 패스를 다른 스레드에서 record하거나, 같은 `JFrameDesc`로 반복 재생(리플레이·헤드리스 렌더링)하는 확장이 구조적으로 가능합니다.

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

Entity index를 그대로 컴포넌트 슬롯 인덱스로 사용합니다. 즉 한 엔티티당 같은 종류의 컴포넌트는 하나만 가질 수 있고, 컴포넌트 lookup은 해시 없이 인덱싱만으로 수행됩니다.

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

카메라마다 view·projection으로부터 6개의 평면을 추출하고, draw item에 대해 AABB 컬링을 수행합니다.
worker 간 lock 경합을 피하기 위해, 각 청크는 **자기만의 로컬 결과 버퍼**에 push하고 메인 스레드가 마지막에 병합합니다.

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

활성 draw item 수가 임계치(256) 이하이면 직렬 경로(`buildSerial`)로 자동 폴백합니다.

### Draw item incremental cache

`JDrawItemCache`는 한 엔티티가 만든 모든 sub-mesh draw item을 `JDrawRange { start, count, generation }`로 묶어 추적합니다. Scene의 이벤트를 받아:

- `Added`    → 새 entity의 draw item을 append
- `Modified` → 기존 range를 in-place로 patch
- `Removed`  → 전체 rebuild

매 프레임 전체 재구축을 피하기 위한 구조입니다.

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

`JGraphicResource::SetConstantBuffer("PerObject", buffer)`처럼 셰이더와 동일한 이름만 맞춰 주면, 슬롯 인덱스를 코드로 지정하지 않고도 바인딩이 연결됩니다.

### Per-frame upload ring + descriptor cache

`JCommandQueue`는 swap-chain 버퍼 수만큼 `FrameResource`를 보유합니다. 각 frame resource는

- 자체 command allocator
- 16 MB 상수 버퍼 업로드 링 (CPU 매핑)
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
- **Deferred** : `JGBufferPass` (Albedo / Normal / Material MRT) → `JLightingPass` (full-screen triangle로 G-buffer 샘플링) → `JForwardOverlayPass`

### JSON 직렬화

`JSceneSerializer`가 nlohmann/json으로 씬을 `.jscene.json`으로 저장 / 로드합니다. 알 수 없는 필드는 무시되며, 누락된 필드는 기본값으로 채워집니다. (`value("key", default)` 패턴)

### FBX 임포터 (별도 실행 파일)

`JAssetImporter`는 OpenFBX를 사용해 FBX → 메시 / 머터리얼 / 텍스처 / 씬 JSON으로 변환하는 별도 실행 파일입니다. 런타임에 FBX 의존을 두지 않기 위해 빌드와 임포트를 분리했습니다.

```
JAssetImporter <source.fbx> <projectDir>
```

---

## 모듈 구성

```
src/
├── engine/
│   ├── core/        JEngine, JJobSystem, JPool, JHashFunction
│   ├── scene/       JScene, JTransformPool, JCameraPool, JLightPool,
│   │                JRenderObjectComponentPool, JSceneSerializer
│   ├── render/      JRenderer, JRenderServer, JRenderSnapshotBuilder,
│   │                JCameraRenderQueueBuilder, JDrawItemCache,
│   │                JRenderDB, JCommandQueue, JSwapChain,
│   │                JRenderPass (SceneColor / GBuffer / Lighting / ForwardOverlay),
│   │                JGBuffer, JRenderTarget, JGraphicResource
│   ├── asset/       JMaterial, JMesh, JShader
│   ├── platform/    JDevice
│   └── dx12/        JDx12Helper, d3dx12 wrapper
├── client/
│   ├── editor/      JSceneManager, JScenePanel, JSceneBuilder,
│   │                JFBXLoader, JAssetManager
│   └── OpenFBX/     vendored OpenFBX
tools/
└── JAssetImporter/  FBX → 프로젝트 JSON 변환기
benchmark/           OOP / AoS / SoA / WorldMatrix 데이터 레이아웃 비교 (옵션)
third_party/
└── nlohmann/json
```

빌드 타겟은 세 개입니다.

| 타겟              | 형태          | 비고                                       |
|------------------|--------------|-------------------------------------------|
| `Engine`         | static lib   | DX12 / DXGI / D3DCompiler / WindowsCodecs |
| `Client`         | WIN32 exe    | 에디터/뷰어를 겸하는 호스트 응용              |
| `JAssetImporter` | console exe  | FBX → 프로젝트 변환 도구                    |
| `Benchmark`      | console exe  | `BUILD_BENCHMARK=ON` 옵션. oneTBB FetchContent |

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
Client.exe --new                 # 빈 씬으로 시작
Client.exe --scene <path>        # 특정 .jscene.json 파일
Client.exe --project <path>      # 프로젝트 폴더 (scenes/ 하위를 탐색)
Client.exe <projectPath>         # 위치 인자도 프로젝트 폴더로 해석
```

FBX를 프로젝트로 임포트:

```
JAssetImporter <source.fbx> <projectDir>
```

`<projectDir>` 아래에 `scenes/`, `meshes/`, `materials/`, `textures/`, `shader/` 디렉터리가 생성되고, 머터리얼·메시가 JSON으로 저장됩니다.

---

## 제3자 코드

- [nlohmann/json](https://github.com/nlohmann/json) — `third_party/nlohmann/`
- [OpenFBX](https://github.com/nem0/OpenFBX) — `src/client/OpenFBX/`
- [oneTBB](https://github.com/oneapi-src/oneTBB) — 벤치마크 타겟 한정, CMake FetchContent로 가져옴

각 라이브러리의 라이선스를 따릅니다.
