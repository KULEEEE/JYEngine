# JYEngine

<img width="2551" height="1367" alt="image" src="https://github.com/user-attachments/assets/91c430fd-7e6a-4080-a908-efbb584b1e93" />

```
Windows 10+  ·  Direct3D 12 (FL 11.0+)  ·  C++17  ·  Visual Studio + Win10 SDK
```

> GitHub: https://github.com/KULEEEE/JYEngine

**한국어** | [English](README.en.md)

---

## 요약

Data-Oriented Design을 기반으로 구현한 DirectX 12 커스텀 렌더링 엔진이다. Scene/Component 데이터를 렌더링 전용 **Frame Snapshot**과 **Draw Item Cache**로 변환하고, Renderer는 원본 Scene이 아닌 캐시된 불변(immutable) 프레임 데이터를 기반으로 Forward / Deferred path를 실행한다. Transform은 SoA 구조와 dirty index로 갱신 비용을 줄였으며, RenderObject 변경은 Draw Item Cache에 반영한다.

카메라별 Frustum Culling으로 visible draw item을 구성하고, Deferred path는 Shadow, Depth Pre-Pass, G-Buffer, SSAO, Lighting, Forward Overlay Pass로 분리한다. DirectX 12 리소스, descriptor, command queue, render target, root signature는 자체 렌더링 API로 래핑했으며, shader reflection과 RenderDB를 통해 material, mesh, transform, light 리소스를 관리한다.

## 전체 데이터 흐름
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
렌더링 데이터 흐름을 `JScene → Snapshot → RenderDB → Renderer` 의 **단일 방향 구조**로 설계했다. Scene은 변경 가능한 원본 상태만 관리하고, Renderer는 프레임마다 확정된 `JFrameDesc` 와 `RenderDB` 만 참조하도록 분리했다.

이렇게 분리하면 Scene 변경 추적이 단순해지고, 변경된 데이터만 갱신할 수 있다. 또한 RenderPass가 가변하는 Scene에 직접 의존하지 않아 디버깅 경계가 명확하고, Sorting Queue 생성이나 Snapshot 빌드 같은 단계의 병렬화와 최적화가 쉬워져 추후 적용할 Multi Thread Rendering 구조에 적합하게 설계하였다.

## Data-Oriented Design

Data-Oriented Design의 핵심은 데이터 접근 패턴을 기준으로 메모리 배치를 설계해 Cache Hit를 높이고, 불필요한 데이터 처리를 줄이는 것이다. 객체 단위의 추상화보다 **매 프레임 반복되는 처리 흐름**을 기준으로 데이터를 분리했으며, 같은 계산에 필요한 데이터들을 연속된 구조로 묶어 순차 접근이 가능하도록 구성했다.

또한 모든 데이터를 매 프레임 다시 처리하지 않고, 변경된 데이터와 렌더링에 실제로 필요한 **Hot Data만** 선별해 Snapshot과 Draw Item Cache로 변환한다. 이를 통해 Scene의 데이터와 Renderer의 데이터를 분리하고, 렌더링 파이프라인에서는 Cache 친화적인 데이터 기반으로 동작하게 했다.

대표적으로 `JTransformPool` 은 translation / rotation / scale을 각각 별도의 `std::vector` 로 보관하는 **SoA(Structure of Arrays)** 구조이며, 변경된 인덱스만 dirty ring에 누적해 그 프레임에 바뀐 transform만 world matrix로 변환한다.

```cpp
// JTransformPool — SoA + dirty index ring
std::vector<JVec3>  _translations;
std::vector<JVec3>  _rotations;
std::vector<JVec3>  _scales;
std::vector<uint8>  _dirtyFlags;    // 중복 push 방지
std::vector<uint32> _dirtyIndices;  // 이 프레임에 변경된 인덱스만 누적
```

## Component Pool

Scene은 컴포넌트를 객체 단위로 흩어 두지 않고 **같은 종류끼리 Pool에 모아 직접 소유**한다. 핵심은 외부(Scene·시스템)가 컴포넌트의 **주소를 들지 않고 가벼운 핸들로만** 가리키며, 실제 접근은 매번 풀을 거친다는 점이다.

```cpp
struct JEntityHandle { uint32 index; uint32 generation; };

template <typename TData>
struct JPoolSlot { uint32 generation; bool active; TData data; };

template <typename TData>
class JEntityPool {
    std::vector<JPoolSlot<TData>> _slots;
    std::vector<uint32>           _freeIndices;
};
```

이렇게 **포인터 대신 핸들을 쓰는 것**이 Pool 구조의 궁극적인 이유다. 컴포넌트를 포인터로 직접 들고 있었다면, 풀이 내부 배열을 키우거나(`std::vector` 재할당) 슬롯을 재배치하는 순간 그 주소들이 전부 무효가 되어 메모리 배치를 사실상 바꿀 수 없다. 핸들은 접근 시점에 풀에서 위치를 다시 찾으므로 데이터가 어디로 옮겨가도 외부 참조가 깨지지 않는다. 즉 **아무도 주소를 들고 있지 않기에 풀이 데이터 배치를 자유롭게 통제할 수 있고**, 같은 종류의 데이터를 연속·SoA로 모아 Cache Hit율을 높이는 최적화(앞서 본 `JTransformPool`의 SoA 분리)가 바로 이 위에서 가능해진다. 핸들에 함께 담긴 generation은 삭제 후 재사용된 슬롯을 구분해, 사라진 컴포넌트를 가리키는 옛 핸들을 자연스럽게 걸러 주는 안전장치 역할을 한다.

결과적으로 객체 간 포인터를 따라가는 불규칙한 메모리 접근이 사라지고, Transform 갱신이나 RenderObject 순회처럼 매 프레임 반복되는 작업이 연속된 배열을 순차 접근하는 형태가 된다. 각 컴포넌트 풀은 **Entity index를 그대로 컴포넌트 슬롯 인덱스로** 사용하므로, 한 엔티티당 같은 종류의 컴포넌트는 하나만 가질 수 있고 lookup은 해시 없이 인덱싱만으로 끝난다.

```
Entity Index      0        1        2        3        4
EntityPool      active   active    free    active   active
RenderObject    mesh/                       mesh/    mesh/
Pool            material                    material material
TransformPool    SoA      SoA               SoA      SoA
```

## Snapshot

Snapshot은 DOD 관점에서 Scene 데이터를 **렌더링이 가능한 형태로 재배치**하는 단계이다. Scene은 Entity와 Component를 관리하기 좋은 구조로 데이터를 보관하지만, Renderer는 매 프레임 계산된 world matrix, viewProjection, light parameter, visible draw item 목록처럼 바로 사용할 수 있는 데이터만 필요하다.

따라서 Snapshot 단계에서 CPU-side Component 데이터를 렌더링에 필요한 형태로 변환하고, Renderer가 원본 Scene을 다시 탐색하지 않도록 한다. 이는 데이터를 객체의 소유 관계가 아니라 **실제 사용 흐름 기준으로 재구성**하여 불필요한 접근과 계산을 줄이는 DOD 설계이다.

RenderObject의 변경은 매 프레임 전체를 재구축하지 않고 `JDrawItemCache` 에 **증분 반영**한다. Scene 이벤트를 받아 `Added` 는 draw item을 append, `Modified` 는 기존 range를 in-place patch, `Removed` 일 때만 rebuild 한다.

## DirectX 12 API화

DirectX 12의 explicit한 제어권은 유지하되, 상위 RenderPass가 Command Allocator, Descriptor Heap, Resource Upload, Root Binding을 직접 세부적으로 다루지 않도록 렌더링 **Abstraction Layer**를 구성했다.

### JRenderContext

JRenderContext는 GPU 리소스 생성을 담당한다. `CreateVertexBuffer`, `CreateIndexBuffer`, `CreateConstantBuffer`, `CreateTextureFromFile`, `CreateShader`, `CreatePipeline`, `CreateRootSignature` 같은 함수를 제공하여 DX12 Resource 생성 과정을 엔진 API 호출로 단순화했다.

```cpp
JVertexBuffer* vb      = renderContext->CreateVertexBuffer(vertices, vertexCount);
JIndexBuffer*  ib      = renderContext->CreateIndexBuffer(indices);
JTexture*      texture = renderContext->CreateTextureFromFile(path);
JPipeline*     pipe    = renderContext->CreatePipeline(shader);
```

내부에서는 Default Heap 생성, Upload Buffer 복사, Resource Barrier 전환, Descriptor 생성 같은 DX12 절차를 처리한다. 따라서 사용하는 쪽에서는 GPU Resource를 만들기 위해 복잡한 코드를 매번 작성하지 않아도 된다.

### JCommandQueue

JCommandQueue는 렌더링 명령 기록을 단순화한다. `RenderBegin`, `BeginRenderPass`, `BeginDepthPass`, `SetPipeline`, `SetGraphicResources`, `BindVertexBuffer`, `DrawIndexed`, `EndRenderPass`, `RenderEnd` 같은 함수를 제공하여 RenderPass가 필요한 명령만 순서대로 호출할 수 있게 했다.

```cpp
commandQueue->BeginRenderPass(renderTarget, clearColor, 1);
commandQueue->SetPipeline(materialResource->pipeline);
commandQueue->SetGraphicResources(&graphicResource);
commandQueue->BindVertexBuffer(meshResource);
commandQueue->DrawIndexed(indexCount, 1, startIndex, 0, 0);
commandQueue->EndRenderPass();
```

또한 RenderTarget의 상태 전환, Frame Resource별 Upload Buffer(16 MB 상수 버퍼 업로드 링), Descriptor Heap, Fence 동기화도 JCommandQueue 내부에서 관리한다. 같은 머터리얼이 한 프레임 안에서 여러 번 그려질 때를 위해 `(shader, texture set)` 키로 GPU descriptor table을 캐시한다. 이를 통해 RenderPass는 "어떤 Target에 어떤 Resource를 바인딩하고 어떤 Draw Call을 실행할 것인가"에만 집중할 수 있다.

### Shader Reflection 기반 Binding

Shader Compile 이후 `D3DReflect` 로 cBuffer, Texture, Sampler Binding 정보를 수집한다. 이 metadata를 기반으로 Root Signature와 Graphic Resource Binding을 구성하여, **C++ 코드가 shader register index를 직접 반복 관리하지 않도록** 했다.

```
CompileShader()
 → D3DReflect(VS / PS)
 → collect BoundResources (CBUFFER / TEXTURE / SAMPLER)
 → build binding table
 → create root signature
```

cBuffer는 root CBV로, Texture는 descriptor table로 묶이고, Sampler는 static sampler로 root signature에 박힌다. 따라서 슬롯 인덱스를 코드로 지정하지 않고 **셰이더와 동일한 이름만 맞추면** 바인딩이 연결된다.

```cpp
JGraphicResource::SetConstantBuffer("PerObject", buffer);
JGraphicResource::SetTexture("AlbedoMap", texture);
```

한발 더 나아가 **샘플러의 종류까지 이름 규약으로 추론**한다 — 리소스 이름에 `Shadow` 가 들어가면 엔진이 reverse-Z용 비교 샘플러(`GREATER_EQUAL` + border)를 자동 생성한다. 새 셰이더가 그림자 비교 샘플링을 쓰려면 이름만 맞추면 된다.

## Deferred Rendering Pipeline

Renderer는 `RenderPath::Forward` / `RenderPath::Deferred` 를 런타임에 전환한다. Deferred path는 하나의 거대한 셰이더 대신, 각자 입력과 출력이 명확한 **6개의 독립 패스**로 분리했다.

```
JRenderer::Render(JFrameDesc)
   │
   ├─▶ JShadowPass          directional light 기준 cascade depth 생성 (Texture2DArray)
   ├─▶ JDepthPass           카메라 시점 reverse-Z depth pre-pass
   ├─▶ JGBufferPass         Albedo / Normal / Material(MRT 3장) 기록
   ├─▶ JSSAOPass            depth·normal로 화면 공간 차폐 계산 → AO 타겟 (Lighting이 블러해 사용)
   ├─▶ JLightingPass        full-screen triangle로 G-Buffer 샘플링 → PBR 라이팅 + IBL
   └─▶ JForwardOverlayPass  반투명·디버그·그리드 등 forward 오버레이
```

여기서 중요한 설계 결정은, **모든 패스가 mutable Scene을 전혀 모른다**는 점이다. 패스의 입력은 immutable `JFrameDesc` 와 `JRenderDB`(GPU 리소스 mirror)뿐이라, 각 패스는 같은 입력에 대해 항상 같은 출력을 내는 순수 함수에 가깝다. 그 덕분에 패스 단위 테스트, 같은 프레임의 반복 재생(리플레이·헤드리스), snapshot·culling 단계의 병렬화가 구조적으로 가능해진다.

또한 각 패스가 자기 셰이더·파이프라인을 lazy하게 생성(`ensureResources`)하고, depth·shadow·gbuffer가 동일한 draw item 목록(`opaqueDrawItemIndices` = frustum culling 결과)을 공유하므로, **패스를 추가하는 비용이 "클래스 하나 추가" 수준**으로 낮다.

## Depth Pre-Pass + Reverse-Z

본격적인 G-Buffer 기록 전에, 불투명 오브젝트의 깊이만 먼저 한 번 그린다. depth pre-pass로 최종 깊이를 확정해 두면 이후 G-Buffer / Lighting 패스에서 가려지는 픽셀의 픽셀 셰이더 비용을 절감할 수 있다.

이 과정에서 **Reverse-Z를 엔진 전역의 단일 규약으로 통일**한 것이 특징이다. 깊이 비교 함수를 `GREATER`, depth clear 값을 `0`(near = 1, far = 0)으로 맞췄고, 이 규약은 depth pre-pass뿐 아니라 G-Buffer, shadow ortho 투영, 비교 샘플러, "아무것도 안 그려진 픽셀 판정"까지 전부 동일하게 적용된다. depth를 다루는 코드 어디서든 "0 = far, 비교는 GREATER"가 일관되게 성립하므로, 패스를 늘려도 깊이 처리에서 실수가 끼어들 여지가 적다. 부가적으로 부동소수점 depth 분포가 원거리에서 촘촘해져 z-fighting도 줄어든다.

depth pre-pass와 shadow pass는 결국 **깊이만 필요**하므로 동일한 `depth_prepass.hlsl` 셰이더를 공유한다. 패스가 늘었지만 셰이더 코드는 한 벌만 유지된다.

```cpp
// JDepthPass / JShadowPass 모두 같은 셰이더를 재사용
_shader = renderContext->CreateShader(get_Engine_Shader_Path() + "\\depth_prepass.hlsl");
_pipeline = renderContext->CreatePipeline(
    _shader, /*alphaBlend*/ false, /*vertexInput*/ true, ...,
    /*depthWrite*/ D3D12_DEPTH_WRITE_MASK_ALL,
    /*depthFunc*/  D3D12_COMPARISON_FUNC_GREATER);  // reverse-Z
```

## Cascade Shadow Map (CSM)

directional light의 광역 그림자를 위해 4단 Cascade Shadow Map을 구현했다. 깊이는 `CASCADE_COUNT(4)` 슬라이스를 가진 `Texture2DArray` 한 장에, cascade별 DSV로 나눠 그린다.

### Cascade 분할 — 카메라 행렬 하나로 자기완결

카메라에서 가까울수록 촘촘한 해상도가 필요하므로, 시야 거리를 비선형 비율로 나눈다.

```cpp
// 누적 비율 — 가까운 cascade일수록 좁은 구간을 덮음
constexpr float CASCADE_SPLIT_RATIOS[4] = { 0.05f, 0.16f, 0.42f, 1.0f };
```

이 구현의 특징은 near/far 평면 값이나 별도 파라미터를 프레임 데이터로 넘기지 않고, **카메라의 `inverseViewProjection` 하나만으로** 모든 분할을 유도한다는 점이다(self-contained). NDC 코너를 역투영해 world-space 프러스텀 코너를 구하고, 각 cascade 구간을 코너 보간으로 잘라낸다.

```cpp
// NDC 코너 → world-space 프러스텀 코너 (reverse-Z: z=1 near, z=0 far)
corners.nearCorners[i] = XMVector3TransformCoord({ndcX, ndcY, 1.0f, 0.0f}, inverseViewProjection);
corners.farCorners[i]  = XMVector3TransformCoord({ndcX, ndcY, 0.0f, 0.0f}, inverseViewProjection);
```

### 안정적인 light-space 볼륨

각 cascade 슬라이스의 8개 코너를 감싸는 **bounding sphere**로 직교 투영 볼륨을 잡는다. sphere 기반이라 카메라가 회전해도 볼륨 크기가 변하지 않아 그림자 가장자리의 깜빡임(shimmering)이 줄어든다. 슬라이스 뒤쪽(라이트 방향)의 caster까지 담기도록 여유(`casterMargin`)를 더하며, 직교 투영도 엔진 전역 reverse-Z에 맞춰 near/far를 스왑한다.

```cpp
const XMMATRIX lightView = XMMatrixLookToLH(eye, lightDirection, up);
// reverse-Z(GREATER, clear 0)에 맞춰 near/far를 스왑
const XMMATRIX lightProjection =
    XMMatrixOrthographicOffCenterLH(-radius, radius, -radius, radius, /*zFar*/ zFar, /*zNear*/ 0.0f);
```

### 샘플링 — 셰이더가 샘플러 종류까지 결정한다

라이팅 패스에서는 픽셀의 카메라 거리로 cascade를 고른 뒤 2×2 PCF로 샘플링한다. 비교 샘플러를 C++에서 만들어 넘기는 게 아니라, 앞서 설명한 이름 규약(`Shadow`)에 따라 셰이더 리플렉션 단계에서 엔진이 reverse-Z 비교 샘플러를 자동 생성한다. 맵 밖 UV는 border(0)와 비교돼 `ref >= 0` → 항상 lit으로 처리된다.

```hlsl
Texture2DArray ShadowMap : register(t4);
SamplerComparisonState ShadowSampler : register(s0); // 이름에 Shadow → 비교 샘플러 자동 생성

float referenceDepth = saturate(shadowPos.z + CascadeBiases[cascade]); // cascade별 reverse-Z bias
shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(uv, cascade), referenceDepth);
```

depth bias는 world 단위로 한 번만 지정하고, cascade마다 자신의 `zFar`로 나눠 reverse-Z depth 단위로 환산한다. 덕분에 가까운 cascade와 먼 cascade가 일관된 두께의 bias를 갖는다.

## Deferred PBR / Lighting

### G-Buffer 레이아웃

| MRT | 포맷 | 내용 |
|-----|------|------|
| Albedo   | `R8G8B8A8_UNORM`     | base color (× material baseColor) |
| Normal   | `R16G16B16A16_FLOAT` | world-space normal (0.5+0.5 인코딩) |
| Material | `R8G8B8A8_UNORM`     | R=Roughness, G=Metallic, B=Occlusion |
| Depth    | `D32_FLOAT`          | reverse-Z depth (pre-pass와 공유) |

머터리얼 텍스처는 Bistro/glTF 컨벤션의 **ORM 패킹(R=Occlusion, G=Roughness, B=Metalness)** 을 그대로 읽어 한 번의 샘플로 세 값을 얻는다.

### 탄젠트 없는 노멀 매핑 (화면 공간 미분)

노멀 매핑을 위해 버텍스 탄젠트를 따로 저장·임포트하지 않는다. 대신 G-Buffer 패스에서 **화면 공간 미분(ddx/ddy)으로 cotangent frame을 즉석 구성**해 노멀 맵을 적용한다(Schüler의 cotangent frame). 메시 포맷에 tangent 채널이 없어도 노멀 매핑이 동작하므로 FBX 임포트 파이프라인과 정점 레이아웃이 그만큼 단순해진다.

```hlsl
// 버텍스 탄젠트 없이 TBN 구성. ddx/ddy는 래스터화 중에만 유효하므로 G버퍼 패스에서 노멀을 완성한다.
float3 dp1 = ddx(worldPos);  float3 dp2 = ddy(worldPos);
float2 duv1 = ddx(uv);       float2 duv2 = ddy(uv);
float3 tangent   = cross(dp2, normal) * duv1.x + cross(normal, dp1) * duv2.x;
float3 bitangent = cross(dp2, normal) * duv1.y + cross(normal, dp1) * duv2.y;
```

BC5로 압축된 노멀 맵은 RG만 저장하므로 Z는 `sqrt(1 - x² - y²)`로 복원한다.

### 라이팅 (Cook-Torrance PBR)

Lighting 패스는 full-screen triangle 하나로 화면을 덮고, G-Buffer를 `Load`로 직접 읽어 픽셀별 PBR을 계산한다.

- **BRDF** : GGX(법선 분포) + Smith(기하 감쇠) + Schlick Fresnel 의 Cook-Torrance 모델.
- **라이트** : `w <= 0` 이면 directional(감쇠 없음), `w > 0` 이면 point light로 inverse-square + range windowing(UE/Frostbite 스타일) 감쇠를 적용한다.
- **그림자** : 모든 directional light가 첫 directional 기준 cascade shadow를 공유한다.
- **AO** : occlusion(SSAO)은 간접광(ambient)에만 적용하고, 직접광 차폐는 shadow map이 담당하도록 역할을 분리했다.
- **톤매핑** : 최종 색을 **ACES filmic 근사(Narkowicz 2015)** 로 톤매핑한 뒤 감마 보정한다. Reinhard보다 대비·채도 보존이 좋다.

전역 reverse-Z 규약 덕분에, "아무것도 그려지지 않은 픽셀(depth == 0 = far plane)"을 단 한 번의 비교로 걸러 라이팅을 건너뛴다 — 깊이 규약을 통일해 둔 효과가 라이팅 단계에서도 그대로 이어진다.

```hlsl
float depth = DepthBuffer.Load(int3(pixel, 0)).r;
if (depth <= 0.0f) return float4(albedo, alpha); // far plane = 미작성 픽셀
...
color = ACESFilm(ambient + direct);
color = pow(saturate(color), 1.0 / 2.2);
```

## 씬 캡처 기반 IBL + SSAO (Global Illumination)

직접광(CSM)만으로는 그림자 영역이 새까맣게 떨어진다. 간접광을 채우기 위해 **정적 씬을 직접 캡처해 굽는 split-sum IBL**과 **화면 공간 SSAO**를 추가했다. 외부 HDRI를 쓰지 않고 **씬 자체를 환경맵으로 캡처**하므로 반사·간접광이 실제 장면과 일치한다.

```
[로드 후 1회 bake]                              [매 프레임]
 probe 위치에서 씬 6면 → HDR 큐브 캡처
        │  (irradiance / prefiltered / BRDF LUT 생성)
        ▼                                        LightingPass:
 ┌─ irradiance 큐브 (diffuse GI) ─┐               ambient = kD·irradiance·albedo
 ├─ prefiltered 큐브 (specular)   ├──상주(샘플만)──▶        + prefiltered·(F·brdf.x + brdf.y)
 └─ BRDF LUT (F항 적분)           ─┘               (× SSAO)
```

### 1. 씬 캡처 — immutable 프레임 데이터의 재사용

probe(주 카메라) 위치에서 **6면을 90° FOV·reverse-Z로 HDR 큐브(`R16G16B16A16F`)에 렌더**한다. 여기서 단방향 데이터 흐름 설계가 그대로 이득이 된다 — 같은 씬을 **면마다 `JFrameDesc`만 새로 구성**(probe 카메라 + 면 방향 + 큐브 face 타겟)해 동일한 deferred 경로를 6번 돌리면 끝이다. IBL 소스는 linear여야 하므로 캡처 경로에서는 **톤매핑/감마를 건너뛰고** linear HDR로 기록한다.

### 2. 컨볼루션 — compute 없이 render-to-cube-face

엔진에 compute 파이프라인이 없어, 모든 컨볼루션을 **풀스크린 패스로 큐브 면에 직접 렌더**해 처리했다.

- **irradiance 큐브**: 캡처 큐브를 코사인 가중 적분 → 저주파 diffuse GI (작은 32² 큐브로 충분)
- **prefiltered 큐브**: roughness별 GGX importance sampling으로 mip마다 프리필터 → roughness가 클수록 흐린 반사
- **BRDF LUT**: split-sum의 F항을 `(NdotV, roughness)`로 적분한 2D LUT. 환경과 무관하므로 한 번만 굽는다.

### 3. 라이팅 — split-sum 적용

```hlsl
float3 F   = FresnelSchlickRoughness(NdotV, f0, roughness);
float3 kD  = (1 - F) * (1 - metallic);
float3 diffuseIBL  = IrradianceMap.Sample(s, N).rgb * albedo;
float3 prefiltered = PrefilteredEnv.SampleLevel(s, reflect(-V, N), roughness * MaxMip).rgb;
float2 brdf        = BRDFLUT.Sample(sClamp, float2(NdotV, roughness)).rg;
float3 ambient = (kD * diffuseIBL + prefiltered * (F * brdf.x + brdf.y)) * ao;
```

결과 큐브 3종은 **런타임 내내 상주**하고 매 프레임 텍스처 샘플만 한다 — 정적 씬이라 bake는 1회면 충분하고 런타임 비용은 거의 0이다. 또 지오메트리가 없는 픽셀에는 절차적 하늘을 그려, **skybox 배경이자 IBL 상반구 광원**으로 동시에 활용했다.

> 여기서도 엔진의 이름 규약 바인딩이 이어진다 — BRDF LUT용으로 이름에 `LUT`가 들어가면 **clamp 샘플러를 자동 생성**하는 규칙을 추가해(가장자리 누수 방지), 셰이더는 슬롯/샘플러 설정 없이 이름만 맞추면 된다.

### 4. SSAO — 데이터 문제를 실시간 차폐로 우회

이 자산의 "ORM" 텍스처가 사실 **Specular 맵**이라 baked occlusion(R 채널)이 0에 가까웠고, 그대로 곱하면 간접광이 통째로 죽어 그림자가 새까맣게 보였다. baked AO에 의존하는 대신 **`JSSAOPass`로 depth·normal에서 실시간 차폐를 계산**해 간접광에만 곱하도록 바꿨다.

- AO를 **전용 패스에서 단일 채널 타겟에 렌더**하고, 라이팅에서 **박스 블러로 읽어** 알갱이 노이즈를 제거한다.
- 샘플 반경을 **월드 공간 기준**으로 잡아 화면 픽셀로 환산한다. 화면 고정 픽셀 반경은 원거리에서 월드 footprint가 과도하게 커져 깊이가 다른 면을 가로질러 얼룩이 생기는데, 월드 기준으로 두면 거리와 무관하게 일정한 차폐가 된다.

> **설계 트레이드오프**: 단일 probe라 위치별 반사 시차(parallax)는 없고, 1-bounce 정적 캡처다. 정적 씬 쇼케이스에선 드러나지 않으며, 다중 probe·멀티바운스 재bake로 확장 가능한 구조로 남겨 두었다.

## AI 친화적 설계

추후 AI Agent를 엔진에 연결하는 것을 고려하여, 엔진 아키텍처를 AI가 이해하고 확장하기 쉬운 형태로 설계했다. Scene, Snapshot, RenderDB, Renderer, RenderPass의 역할을 명확히 분리하고, 데이터 흐름을 단방향에 가깝게 구성하여 각 단계의 책임과 입력/출력을 추적하기 쉽도록 했다. 또한 저수준 DX12 API를 `JRenderContext`, `JCommandQueue`, `JGraphicResource` 같은 자체 API로 래핑하여, AI가 기능을 추가·수정할 때 복잡한 DX12 세부 구현보다 엔진 레벨의 명확한 인터페이스를 기준으로 작업할 수 있도록 했다.

Scene 저장 포맷 역시 AI 도구가 이해하기 쉽도록 JSON 기반으로 구성했다. Scene은 binary가 아니라 `entities` / `materials` / `meshes` 로 분리된 명시적 JSON 구조로 저장되며, 각 Entity는 `stableID`, `name`, `transform`, `camera`, `light`, `renderObjectComponent` 처럼 의미 있는 필드명을 가진다.

```json
{
  "stableID": "ent_camera_001",
  "name": "MainCamera",
  "transform": { "translation": [0, 3, -5], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
  "camera": { "primary": true, "nearP": 0.1, "farP": 1000.0 }
}
```

이 구조는 엔진 내부의 Pool/Handle 기반 런타임 구조와 분리되어 있다. **실행 시에는 성능 중심의 데이터 배치**를 사용하고, **저장·편집 시에는 가독성과 자동화 가능성**을 우선한 구조이다.

## FBX 임포터 + 텍스처 쿠킹

`JAssetImporter`는 OpenFBX를 사용해 FBX → 메시 / 머터리얼 / 텍스처 / 씬 JSON으로 변환하는 별도 실행 파일이다. 텍스처는 임포트 시 `.jtex` 컨테이너(밉체인·압축 포맷 포함)로 구워지고, 런타임은 전용 리더로 그대로 GPU에 올린다. 런타임에 FBX 의존을 두지 않기 위해 빌드와 임포트를 분리했다.

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
└── BistroExterior/  project.json 기반 샘플 프로젝트 (deferred + CSM + IBL)
benchmark/           OOP / AoS / SoA / WorldMatrix 데이터 레이아웃 비교 (옵션)
third_party/
├── nlohmann/json
└── imgui            에디터 UI
```

빌드 타겟은 다음과 같다.

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

`Client`가 Visual Studio의 기본 시작 프로젝트로 설정되어 있다.

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

`<projectDir>` 아래에 `scenes/`, `meshes/`, `materials/`, `textures/`, `shader/` 디렉터리가 생성되고, 머터리얼·메시는 JSON, 텍스처는 `.jtex`로 저장된다.

---

## 외부 코드

- [nlohmann/json](https://github.com/nlohmann/json) — `third_party/nlohmann/`
- [Dear ImGui](https://github.com/ocornut/imgui) — `third_party/imgui/` (에디터 UI)
- [OpenFBX](https://github.com/nem0/OpenFBX) — `src/client/OpenFBX/`
- [oneTBB](https://github.com/oneapi-src/oneTBB) — 벤치마크 타겟 한정, CMake FetchContent로 가져옴

각 라이브러리의 라이선스를 따른다.
