# JYEngine의 Data-Oriented 설계 전환기

게임 엔진을 만들면서 가장 많이 고민한 지점은 "어떻게 하면 데이터를 엔진이 실제로 쓰는 방식에 맞게 배치할 수 있을까"였다.

처음에는 일반적인 객체 중심 구조처럼 `Scene`이 여러 데이터를 직접 들고 있고, 각 기능이 필요한 객체를 찾아서 수정하는 방식으로 접근했다. 하지만 렌더링이나 culling처럼 매 프레임 대량의 데이터를 순회해야 하는 작업에서는 이런 구조가 점점 부담으로 느껴졌다.

그래서 JYEngine은 점진적으로 Data-Oriented Design, 즉 DOD에 가까운 구조로 바꾸기 시작했다.

이 글은 그 과정에서 어떤 문제를 발견했고, 어떤 방향으로 구조를 바꿨는지 정리한 기록이다.

## 처음 문제의식

게임 엔진은 결국 매 프레임 많은 데이터를 읽고, 일부 데이터를 수정하고, 그 결과를 GPU에 전달한다.

예를 들면 다음과 같은 작업이 반복된다.

```text
Transform 갱신
Camera view/projection 계산
Light 정보 수집
RenderObject 목록 구성
Frustum culling
GPU resource sync
Draw call 생성
```

이런 작업들은 개별 객체 하나를 예쁘게 추상화하는 것보다, 같은 종류의 데이터를 한 번에 순회하는 일이 더 많다.

그래서 단순히 `Camera` 객체가 `Transform`을 소유하고, `Light` 객체가 자기 위치를 들고 있고, `RenderObject`가 이것저것 포인터로 연결된 구조는 점점 어색해졌다.

내가 원했던 방향은 다음과 같았다.

```text
실제 데이터는 Pool에 모은다.
Scene은 데이터 접근과 handle 연결을 관리한다.
System이나 Builder는 Pool을 순회해서 필요한 결과를 만든다.
GPU resource는 CPU data의 mirror로 RenderDB에서 관리한다.
```

즉, 객체가 데이터를 소유하는 구조가 아니라, 데이터가 먼저 있고 기능이 그 데이터를 처리하는 구조를 목표로 했다.

## Entity와 Component Pool

가장 먼저 정리한 것은 entity와 component의 관계였다.

기존에는 component가 별도 pool에 들어가더라도 handle index가 component pool 기준으로 생성될 수 있었다. 하지만 그렇게 되면 entity와 component 사이를 찾기 위해 lookup이 필요하고, 구조가 복잡해진다.

그래서 지금 구조는 entity index를 component slot index로 직접 사용한다.

```text
Entity 5
-> TransformPool[5]
-> CameraPool[5]
-> LightPool[5]
-> RenderObjectComponentPool[5]
```

이 방식의 장점은 명확하다.

```text
한 entity에 같은 종류의 component는 하나만 붙는다.
entity handle만 있으면 component handle을 바로 만들 수 있다.
component lookup 비용이 줄어든다.
삭제/재생성 시 generation으로 stale handle을 검증할 수 있다.
```

이를 위해 `JEntityComponentPool`을 만들었다.

```cpp
template <typename THandle, typename TData>
class JEntityComponentPool
{
public:
    THandle Add(JEntityHandle entity, const TData& data = {});
    bool Remove(THandle handle);
    TData* Get(THandle handle);
    bool IsValid(THandle handle) const;
};
```

component pool은 필요한 entity index까지 chunk 단위로 확장된다. 모든 entity 수만큼 무조건 고정 배열을 잡는 것이 아니라, 필요한 범위까지 늘려가도록 했다.

## Transform은 SoA로 분리

DOD 관점에서 가장 먼저 효과를 볼 수 있는 데이터는 `Transform`이라고 판단했다.

Transform은 거의 모든 object가 가지고 있고, culling, world matrix 계산, GPU constant buffer sync 등 여러 작업에서 대량으로 순회된다.

그래서 `JTransformPool`은 일반적인 AoS 구조 대신 SoA 형태로 분리했다.

```cpp
std::vector<JVec3> _translations;
std::vector<JVec3> _rotations;
std::vector<JVec3> _scales;
```

이렇게 하면 특정 작업에서 필요한 데이터만 연속적으로 읽을 수 있다.

예를 들어 위치만 필요한 light snapshot이나 bounds 계산에서는 translation만 보면 된다. world matrix를 계산할 때는 translation, rotation, scale을 순차적으로 읽는다.

또한 transform은 dirty index를 따로 관리한다.

```text
SetTranslation()
-> dirty flag 설정
-> dirty index push

RenderSnapshotBuilder
-> dirty transform만 world matrix 재계산
```

즉 transform이 10,000개 있어도 매 프레임 10,000개를 모두 GPU에 다시 올리는 것이 아니라, 변경된 transform만 snapshot으로 넘긴다.

## Scene은 데이터를 직접 계산하지 않는다

`JScene`은 실제 runtime scene data를 가지고 있지만, 렌더링을 위한 계산을 직접 수행하지 않도록 정리했다.

Scene의 역할은 다음에 가깝다.

```text
Entity 생성/삭제
Component 추가/삭제
Handle 검증
Pool 접근
RenderObject 변경 이벤트 기록
```

반대로 이런 일은 Scene이 하지 않도록 했다.

```text
Camera view/projection 계산
Transform world matrix 계산
Light GPU resource 구성
DrawItem 구성
GPU buffer 생성
```

이런 계산은 별도 builder나 RenderServer 단계에서 처리한다.

이렇게 한 이유는 데이터 흐름을 한 방향으로 유지하고 싶었기 때문이다.

```text
Scene Pool Data
-> Snapshot
-> RenderDB Resource
-> Renderer
```

중간에서 ScenePanel이나 Scene이 GPU resource를 직접 만지기 시작하면 구조가 금방 섞인다. CPU data와 GPU resource의 경계를 유지하는 것이 중요했다.

## Snapshot은 CPU 계산 결과

Snapshot은 "Scene data를 그대로 복사한 것"이 아니라, 렌더링 전에 CPU에서 계산해 둔 결과물이다.

현재 snapshot은 크게 세 종류가 있다.

```cpp
struct JFrameSnapshot
{
    std::vector<JCameraSnapshot> cameras;
    std::vector<JTransformSnapshot> transforms;
    std::vector<JLightSnapshot> lights;
};
```

각 snapshot의 의미는 다르다.

```text
CameraSnapshot
-> viewProjection, frustum, visible draw item index 통계

TransformSnapshot
-> dirty transform의 world matrix

LightSnapshot
-> active light의 color/intensity/position
```

중요한 점은 Snapshot이 GPU resource가 아니라는 것이다.

Snapshot은 CPU 계산 결과이고, `RenderDB`는 그 snapshot을 받아 GPU resource mirror를 갱신한다.

```text
JRenderSnapshotBuilder
-> CPU 계산 결과 생성

JRenderDB
-> GPU constant buffer / mesh / material resource 관리
```

## RenderDB는 GPU Resource Mirror

`RenderDB`는 CPU scene data를 직접 소유하지 않는다. 대신 GPU에 올라간 resource들을 관리한다.

예를 들면 다음과 같은 것들이다.

```text
MaterialResource
CameraResource
TransformResource
LightResource
MeshResource
```

렌더링 단계에서는 draw item이 직접 GPU resource pointer를 들고 있지 않도록 정리했다.

현재 `JDrawItem`은 GPU resource를 직접 소유하지 않고, key 역할을 하는 데이터만 가진다.

```cpp
struct JDrawItem
{
    JEntityHandle entity;
    JRenderObjectComponentHandle renderObject;
    JTransformHandle transform;
    uint32 materialID;
    const JMesh* mesh;
    uint32 subMeshIndex;
    uint32 indexCount;
    uint32 startIndex;
    bool transparent;
};
```

실제 draw 시점에는 `RenderDB`를 통해 필요한 resource를 조회한다.

```text
drawItem.mesh
-> RenderDB::FindMeshResource()

drawItem.materialID
-> RenderDB::FindMaterialResource()

drawItem.transform
-> RenderDB::FindTransformResource()
```

이렇게 하면 `JDrawItem`은 CPU-side render queue의 항목이고, GPU resource lifetime은 `RenderDB`가 책임진다.

## DrawItemCache와 RenderObjectComponent

렌더링을 위해 매 프레임 `RenderObjectComponent` 전체를 다시 순회해서 draw item을 만드는 것은 낭비라고 봤다.

그래서 `RenderServer`는 `JDrawItemCache`를 가진다.

```cpp
struct JDrawItemCache
{
    std::vector<JDrawItem> drawItems;
    std::vector<JDrawRange> drawRangeByEntityIndex;
    std::vector<uint32> activeDrawEntityIndices;
};
```

`RenderObjectComponent`가 추가되면 draw item을 append하고, 수정되면 해당 entity range만 patch한다.

```text
RenderObject Added
-> append draw items

RenderObject Modified
-> patch existing range

RenderObject Removed
-> cache rebuild
```

삭제는 우선 rebuild로 처리했다. 삭제까지 부분 patch하려면 free range 관리가 필요해지고, 지금 단계에서는 복잡도 대비 이득이 크지 않다고 판단했다.

## CameraRenderQueueBuilder와 JobSystem

카메라별로 어떤 draw item을 그릴지 결정하는 로직은 `JCameraRenderQueueBuilder`로 분리했다.

처음에는 `RenderServer` 안에 draw item index를 채우는 함수가 있었지만, 역할이 섞이는 느낌이 강했다.

현재 구조는 다음과 같다.

```text
JRenderServer
-> 전체 sync 순서 orchestration

JRenderSnapshotBuilder
-> camera / transform / light snapshot 생성

JCameraRenderQueueBuilder
-> camera별 visible draw item index 구성
```

`JCameraRenderQueueBuilder`는 `JJobSystem`을 사용해서 active draw item range를 chunk로 나눠 처리한다.

```text
DrawItem 0..N
-> chunk 분할
-> 각 worker가 frustum culling
-> local output에 visible index 저장
-> main thread에서 merge
```

worker thread가 같은 vector에 동시에 push하지 않도록 chunk별 local result를 사용했다.

```cpp
struct ChunkResult
{
    std::vector<uint32> opaque;
    std::vector<uint32> transparent;
    uint32 testedCount;
    uint32 culledCount;
};
```

이 구조는 지금은 CPU frustum culling에 쓰고 있지만, 나중에는 cluster culling이나 GPU-driven path로 확장할 수 있는 중간 단계라고 보고 있다.

## Bounds와 Frustum Culling

Frustum culling을 위해 `JMesh`에 local AABB를 추가했다.

```cpp
struct Bounds
{
    JVec3 min;
    JVec3 max;
    JVec3 center;
    JVec3 extents;
    bool valid;
};
```

FBX import나 procedural mesh 생성 이후 `SetPositions()`가 호출되면 position data를 기반으로 bounds를 자동 계산한다.

```text
positions
-> min/max 계산
-> local AABB 저장
```

culling 시점에는 local AABB를 transform world matrix로 변환하고, camera frustum plane과 검사한다.

```text
Mesh local AABB
-> Transform world matrix
-> World AABB
-> Camera frustum test
```

결과는 `CameraSnapshot`에 통계로 남긴다.

```text
cullingTestedDrawItemCount
culledDrawItemCount
visible draw count
```

이 통계는 디버깅과 성능 측정에 사용할 수 있다.

## Debug Overlay 제거

처음에는 D3D12 pass로 debug text를 그렸다. 하지만 프로파일링해보니 `DebugOverlayPass`의 geometry 생성 비용이 너무 컸다.

특히 텍스트를 사각형 단위로 계속 생성하는 방식은 디버그용으로는 단순하지만, 성능 측정에는 방해가 됐다.

그래서 렌더링 파이프라인에서 `DebugOverlayPass`를 제거하고, Win32 popup으로 최소한의 정보만 표시하도록 바꿨다.

```text
FPS
DrawCalls
```

이 판단도 DOD 설계와 같은 맥락이다.

성능을 측정하려면 측정 도구 자체가 프레임을 크게 왜곡하면 안 된다.

## 현재 구조의 데이터 흐름

현재 JYEngine의 한 프레임 준비 흐름은 다음에 가깝다.

```text
JScene
  - Entity Pool
  - Transform Pool
  - Camera Pool
  - Light Pool
  - RenderObjectComponent Pool

        |
        v

JRenderSnapshotBuilder
  - CameraSnapshot
  - TransformSnapshot
  - LightSnapshot

        |
        v

JRenderServer
  - DrawItemCache update
  - CameraRenderQueueBuilder dispatch

        |
        v

JRenderDB
  - GPU resource mirror sync

        |
        v

JRenderer
  - Render pass execution
```

핵심은 `Scene -> Snapshot -> RenderDB -> Renderer`로 데이터 흐름을 단방향에 가깝게 유지하는 것이다.

## DOD라고 해서 모든 코드가 배열 순회만 하는 것은 아니다

이 작업을 하면서 느낀 점은, DOD는 "모든 코드를 SoA로 바꾸는 것"이 아니라는 것이다.

게임 엔진에는 여전히 handle lookup, 개별 component 수정, editor interaction, resource management 같은 코드가 필요하다.

예를 들어 특정 entity의 위치를 수정하는 코드는 결국 pool에 접근해서 해당 slot을 바꾼다.

이런 작업은 그 자체로 DOD의 큰 이점을 얻기 어렵다.

대신 DOD의 이점은 다음 같은 구간에서 나온다.

```text
대량 Transform 순회
dirty transform sync
camera별 culling
draw item queue 구성
GPU resource upload 대상 선별
```

즉 개별 수정 코드보다, 매 프레임 대량 처리되는 hot path를 데이터 중심으로 만드는 것이 중요했다.

## 아직 남은 과제

현재 구조가 완성형이라고 보지는 않는다.

앞으로 더 정리하고 싶은 부분은 다음과 같다.

```text
1. Instanced rendering
   - 같은 mesh/material을 사용하는 draw item을 묶어 draw call 감소

2. Benchmark
   - object count 증가에 따른 SyncScene / CameraQueue / Render time 측정
   - culling on/off 비교
   - dirty rate별 transform sync 비용 측정

3. RenderDB lookup hot path 최적화
   - unordered_map lookup 대신 resource index/key 캐싱 검토

4. Cluster-based geometry prototype
   - mesh를 cluster 단위로 나누고 cluster bounds culling으로 확장

5. GPU timestamp query
   - CPU/GPU 병목을 엔진 내부에서 수치로 확인
```

특히 포트폴리오 관점에서는 단순히 "DOD로 만들었다"라고 말하는 것보다, 실제 수치가 필요하다고 본다.

예를 들면 다음과 같은 결과를 보여주고 싶다.

```text
10,000 objects 기준
Scene preparation 평균 ms
Culling 전/후 visible draw count
Dirty transform 비율별 sync cost
Instancing 전/후 draw call count
```

## 마무리

JYEngine의 DOD 전환은 단번에 완성된 구조가 아니라, 계속 문제를 발견하면서 바꿔온 과정이다.

처음에는 단순히 `Scene`에 데이터를 넣고 렌더링하면 된다고 생각했지만, 실제로 구조를 만들다 보니 CPU data, snapshot, GPU resource, render queue의 경계를 명확히 나눌 필요가 있었다.

현재 가장 중요한 설계 방향은 다음 세 가지다.

```text
1. Runtime data는 Pool에 모은다.
2. Frame 계산 결과는 Snapshot으로 분리한다.
3. GPU resource는 RenderDB가 mirror로 관리한다.
```

이 구조가 완벽하다고 생각하지는 않는다. 하지만 적어도 엔진이 어떤 데이터를 언제, 어떤 순서로, 어떤 목적으로 처리하는지 명확해지고 있다.

그리고 이 명확한 데이터 흐름이 앞으로 instancing, benchmark, cluster culling, GPU-driven rendering 같은 기능을 붙일 수 있는 기반이 된다고 보고 있다.
