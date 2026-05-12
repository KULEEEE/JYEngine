# DOD Benchmark Plan

## Goal

현재 엔진 본체는 `Scene -> RenderServer -> RenderDB -> Renderer` 구조와 pool 기반 데이터 관리로 진행한다.  
성능 비교는 엔진 본체를 OOP 구조로 되돌리지 않고, **benchmark 전용 mock 데이터 모델**을 따로 만들어 비교한다.

핵심 메시지:

- 메인 엔진은 현재 DOD/AoS 방향 유지
- 비교 실험은 별도 benchmark 코드에서 수행
- 비교 대상은 엔진 전체가 아니라 **hot path**

## Why

엔진 본체를 다시 OOP 구조로 섞으면:

- 현재 구조가 더러워짐
- 유지보수 포인트가 늘어남
- 비교 목적보다 실험 코드 관리 비용이 커짐

따라서 포트폴리오용 비교는:

- 동일한 입력
- 동일한 처리
- 서로 다른 데이터 레이아웃

만 따로 분리해서 보는 것이 맞다.

## Benchmark Scope

전체 엔진을 3벌로 만드는 것이 아니라, 아래 핵심 루프만 비교한다.

1. `Transform Update`
2. `Visibility Gather`
3. `Render Item Build`

초기 MVP는 다음 2개를 우선한다.

1. `Transform Update Benchmark`
2. `Render Item Gather Benchmark`

## Comparison Targets

### 1. OOP Mock

pointer chasing 중심 구조를 흉내 낸다.

예시:

```cpp
struct OOPTransform
{
    float x, y, z;
    float yaw, pitch;
};

struct OOPRenderObject
{
    OOPTransform* transform;
    uint32 materialID;
    uint32 meshID;
    bool visible;
};
```

### 2. AoS Pool

현재 엔진과 유사한 구조.

예시:

```cpp
struct TransformData
{
    float x, y, z;
    float yaw, pitch;
};

struct RenderObjectData
{
    uint32 transformIndex;
    uint32 materialID;
    uint32 meshID;
    bool visible;
};
```

### 3. SoA Pool

속성별 배열 구조.

예시:

```cpp
struct SoARenderObjectPool
{
    std::vector<uint32> transformIndex;
    std::vector<uint32> materialID;
    std::vector<uint32> meshID;
    std::vector<uint8> visible;
};
```

## Rules

- 메인 엔진 코드는 비교를 위해 OOP로 되돌리지 않는다
- benchmark 코드는 `benchmark/` 또는 `tools/benchmark/` 아래 별도 관리
- 비교 함수는 같은 알고리즘을 유지한다
- 차이는 **데이터 레이아웃과 접근 방식**만 둔다

## Suggested Directory

```text
benchmark/
├─ TransformBenchmark.cpp
├─ RenderItemGatherBenchmark.cpp
├─ OOPSceneMock.h
├─ AoSSceneMock.h
├─ SoASceneMock.h
└─ BenchmarkCommon.h
```

## Measurement

측정 항목:

- object count
- transform update time
- visibility gather time
- render item build time

권장 object count:

- 1k
- 10k
- 50k
- 100k

측정 방식:

- Release build
- `std::chrono`
- 여러 번 반복 후 평균
- CSV 출력 가능하면 추가

## Output

포트폴리오 문서에는 아래를 포함한다.

1. 문제 정의
2. 데이터 구조 설명
3. 실험 조건
4. 결과 표
5. 그래프
6. 해석

권장 그래프:

- X축: object count
- Y축: ms
- 선 3개: `OOP`, `AoS`, `SoA`

## Current Engine Policy

현재 엔진 본체는 아래 방향을 유지한다.

- `JScene`: source data owner
- `JPool`: 공용 pool 로직
- `JRenderServer`: frame data assembly
- `JRenderDB`: GPU mirror/cache
- `JRenderer`: normalized frame data consumer

즉 성능 비교용 OOP 구현은 **엔진 구조가 아니라 benchmark 전용 mock**이다.

## Next Step

1. `benchmark` 디렉토리 생성
2. `Render Item Gather`를 첫 실험 대상으로 선택
3. `OOP / AoS / SoA` mock 데이터 모델 구현
4. 동일한 gather 함수 3종 구현
5. 결과를 CSV/HTML로 정리
