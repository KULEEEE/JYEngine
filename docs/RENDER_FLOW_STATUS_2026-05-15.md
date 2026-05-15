# Render Flow Status

Date: 2026-05-15

## Goal

현재 엔진의 렌더 경계는 다음의 단방향 흐름으로 정리하는 것이 목표입니다.

```text
Pool Data -> RenderServer Snapshot -> RenderDB Resource -> Render
```

핵심 원칙은 다음과 같습니다.

- 원본 데이터는 `Scene` 안의 `Pool`이 소유한다.
- `RenderServer`는 `Scene`을 읽어서 프레임용 `Snapshot`을 만든다.
- `RenderDB`는 snapshot을 받아 렌더 리소스만 보관한다.
- `Renderer`와 render pass는 `RenderDB`의 결과만 읽는다.
- `ScenePanel`과 editor UI는 render resource를 직접 만지지 않는다.

즉, 데이터 흐름은 항상 한 방향이어야 한다.

---

## What Is Implemented Now

### 1. Scene data and storage

- `JSceneData`는 저장/로드용 authoring data 역할을 한다.
- `JScene`은 runtime storage 역할을 한다.
- `Transform`은 별도 `JTransformPool`로 분리했다.
- `Transform`은 field-wise SoA 형태로 저장한다.
  - `translation`
  - `rotation`
  - `scale`
- `JComponentLayoutDefinition.h`에 component layout definition을 모아두었다.
- `Camera`, `Light`, `RenderObject`도 전용 pool 경계를 가지고 있다.

### 2. CPU-side asset reuse

- `JAssetManager`가 material / mesh / texture CPU cache 역할을 한다.
- `JSceneBuilder`는 자산을 직접 새로 만들기보다 `JAssetManager`를 통해 재사용한다.
- `JSceneManager`가 `JAssetManager`와 scene lifecycle을 함께 소유한다.

### 3. Snapshot layer

- `JRenderSnapshot.h`가 프레임용 중간 데이터를 정의한다.
- 현재 snapshot 타입:
  - `JCameraSnapshot`
  - `JTransformSnapshot`
  - `JLightSnapshot`
  - `JRenderObjectSnapshot`
  - `JFrameSnapshot`
- `RenderServer`가 `Scene`의 pool을 순회해서 `JFrameSnapshot`을 만든다.

### 4. Render resource layer

- `JRenderDB`는 render resource cache만 관리한다.
- render resource 타입은 `JRenderResource.h`에 모았다.
  - `JCameraResource`
  - `JTransformResource`
  - `JLightResource`
  - `JMeshResource`
- `RenderDB`는 원본 scene을 직접 스캔하지 않는다.
- render pass는 `RenderDB`가 가진 resource만 사용한다.

### 5. Renderer and passes

- `JRenderer`는 frame descriptor와 render DB를 사용해 pass를 실행한다.
- `JSceneColorPass`, `JGBufferPass`, `JForwardOverlayPass`, `JLightingPass`는 render resource만 읽는다.
- `Scene`이나 `ScenePanel`은 render pass 내부로 들어가지 않는다.

### 6. Editor flow

- `JSceneManager`가 scene open / new / reload lifecycle을 담당한다.
- `ScenePanel`은 UI와 scene editing input만 담당한다.
- `ScenePanel`은 render resource를 직접 소유하거나 수정하지 않는다.

---

## Current Flow

현재 코드 기준의 실제 흐름은 다음과 같다.

```text
JSON
-> JSceneData
-> JSceneBuilder
-> JScene / Pool Data
-> RenderServer Snapshot
-> RenderDB Resource
-> Renderer / RenderPass
```

세부 역할:

- `JSceneSerializer`
  - JSON load/save
- `JSceneBuilder`
  - `JSceneData -> JScene`
- `JScene`
  - runtime pool storage
- `JRenderServer`
  - scene pool traversal
  - frame snapshot generation
  - render DB sync entry point
- `JRenderDB`
  - resource cache and render data ownership
- `JRenderer`
  - render pass execution

---

## Target Structure

앞으로 더 강하게 고정하고 싶은 구조는 아래와 같다.

### 1. One-way data flow

반드시 다음 방향만 허용한다.

```text
Scene Pool Data -> Snapshot -> Render Resource -> Render
```

금지하고 싶은 패턴:

- `ScenePanel`이 render resource를 직접 읽는 것
- `Scene`이 render resource를 직접 아는 것
- `RenderDB`가 원본 `Scene`을 직접 해석하는 것
- 중간 계층이 아래 계층의 원본 데이터를 수정하는 것

### 2. Snapshot is transient

Snapshot은 영구 저장소가 아니라 프레임 단위의 중간 결과다.

- `TransformSnapshot`
- `CameraSnapshot`
- `LightSnapshot`
- `RenderObjectSnapshot`

이 데이터는 `RenderServer`가 만들고, frame scope로만 유지한다.

### 3. RenderServer is the bridge

`RenderServer`는 `Scene`과 `RenderDB` 사이의 브리지다.

- `Scene`의 pool을 순회한다.
- snapshot을 만든다.
- snapshot을 `RenderDB`로 넘긴다.
- renderer가 쓸 frame data를 조립한다.

### 4. RenderDB is resource ownership

`RenderDB`는 render resource ownership만 맡는다.

- `JCameraResource`
- `JTransformResource`
- `JLightResource`
- `JMeshResource`
- `JMaterialResource`

원본 scene data는 여기서 읽지 않는다.

### 5. Systems are not part of the target path

예전에는 `System` 객체를 두는 방향을 잠깐 시도했지만, 현재 목표는 그쪽이 아니다.

원하는 최종 방향은:

- `Scene`은 데이터만
- `RenderServer`는 snapshot만
- `RenderDB`는 resource만

즉, 중간 로직은 `RenderServer`에 모으고, `System`은 제거하는 쪽이다.

---

## Current Pieces That Match the Goal

- `TransformPool` SoA 분리
- `JComponentLayoutDefinition` 중앙 정의 파일
- `JAssetManager` CPU asset cache
- `JRenderSnapshot` 프레임용 중간 계층
- `JRenderResource` 렌더 resource 타입 분리
- `JRenderDB` / `JRenderServer` 역할 분리
- `ScenePanel`에서 render resource 직접 접근 제거

---

## Remaining Cleanup

현재 구조를 더 깔끔하게 하려면 남은 일은 다음과 같다.

1. 모든 render 경로가 정말 snapshot만 읽는지 계속 정리한다.
2. `ScenePanel`과 editor code에서 render resource 접근이 다시 생기지 않게 막는다.
3. `Transform`, `Camera`, `Light`, `RenderObject` 모두 snapshot / resource 경계를 같은 규칙으로 유지한다.
4. dirty tracking을 붙여서 변경된 것만 계산하는 방향으로 확장한다.
5. 필요하면 `Light`와 `Camera`도 더 단순한 snapshot 형태로 다듬는다.

---

## Summary

현재까지는 다음까지 완료되었다.

- scene authoring / runtime 분리
- CPU asset reuse layer 추가
- transform SoA 분리
- snapshot layer 추가
- render resource layer 분리
- render flow를 one-way로 정리하는 중

앞으로는 이 원칙을 더 강하게 고정하는 것이 목표다.

```text
Pool Data -> RenderServer Snapshot -> RenderDB Resource -> Render
```
