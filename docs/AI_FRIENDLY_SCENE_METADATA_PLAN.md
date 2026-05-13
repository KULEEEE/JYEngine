# AI-Friendly Scene Metadata Plan

## Goal

`JScene`에 AI/저장/툴링용 식별 계층을 붙인다.  
이번 단계의 완료 기준은 **AI가 Scene을 읽고, stableID로 대상을 지정할 수 있는 상태**까지다.

이 단계에서는 command 실행, CLI, JSON 저장/로드까지 가지 않는다.  
먼저 scene 내부에 stable identity와 snapshot 기반을 만든다.

## Target Structure

```text
JScene
├─ Runtime Pools
│  ├─ EntityPool
│  ├─ TransformPool
│  ├─ CameraPool
│  ├─ LightPool
│  └─ RenderObjectPool
├─ Metadata
│  ├─ stableID
│  ├─ stableIDHash
│  ├─ name
│  ├─ componentMask
│  └─ tags
└─ Snapshot
   ├─ scene summary
   └─ entity summaries
```

## Current Components

현재 `JScene` pool에 존재하는 runtime component/data는 아래 네 가지다.

- `TransformData`
- `CameraData`
- `LightData`
- `RenderObjectData`

`EntityData`는 component라기보다 entity row 상태에 가깝다.

기존 object-style `JCameraComponent`는 제거한다.  
현재 runtime camera는 `JScene::CameraData`와 `TransformData` 조합으로 표현한다.

## Planned Work

### 1. Scene Metadata

추가할 타입:

```cpp
enum class JSceneComponentMask : uint32
{
    None = 0,
    Transform = 1 << 0,
    Camera = 1 << 1,
    Light = 1 << 2,
    RenderObject = 1 << 3,
};

struct JEntityMetadata
{
    uint64 stableIDHash = 0;
    uint32 componentMask = 0;

#if J_ENABLE_SCENE_METADATA
    std::string stableID;
    std::string name;
    std::vector<std::string> tags;
#endif
};
```

### 2. Stable ID

`stableID`는 AI command, 저장 파일, 툴링에서 entity를 안정적으로 가리키기 위한 ID다.

정책:

- 엔진이 entity 생성 시 발급한다
- AI가 임의로 정하지 않는다
- `name`과 다르다
- `name`은 중복 가능, stableID는 고유
- command target은 stableID를 사용한다

초기 형식:

```text
ent_000001
ent_000002
ent_000003
```

추후 UUID/ULID로 교체 가능하다.

### 3. Metadata Storage

초기 구조:

```cpp
std::vector<JEntityMetadata> _entityMetadata;
std::unordered_map<uint64, JEntityHandle> _stableIDLookup;
```

규칙:

```text
entity.index == metadata index
```

이렇게 하면 entity에서 metadata로 갈 때 map lookup이 필요 없다.

`name`은 lookup map에 넣지 않는다.  
name 검색 인덱스는 나중에 필요할 때 transient cache로 만든다.

### 4. Component Mask Update

component 추가 시 metadata의 `componentMask`를 자동 갱신한다.

- `AddTransform()` -> `Transform`
- `AddCamera()` -> `Camera`
- `AddLight()` -> `Light`
- `AddRenderObject()` -> `RenderObject`

### 5. Snapshot

추가할 snapshot 타입:

- `JSceneSnapshot`
- `JEntitySnapshot`
- `JTransformSnapshot`
- `JCameraSnapshot`
- `JLightSnapshot`
- `JRenderObjectSnapshot`

초기 snapshot은 C++ 구조체까지만 만든다.  
JSON exporter는 다음 단계에서 한다.

Snapshot 규칙:

- raw pointer를 넣지 않는다
- runtime handle은 외부 표현에 직접 노출하지 않는다
- entity는 stableID와 name으로 식별한다
- render object는 `hasMesh`, `materialID`, `visible`, `transparent` 정도만 포함한다

### 6. ScenePanel Sample Metadata

현재 sample scene entity에 name을 부여한다.

- `Main Camera`
- `Grid Plane`
- `Car`

stableID는 자동 생성된 값을 사용한다.

## Out Of Scope For This Step

- JSON 저장/로드
- Command API
- CLI
- name/tag 검색 인덱스
- UUID/ULID
- free-list/remove 안정화
- transform GPU sync

## Completion Criteria

- `JScene`이 entity별 stableID/name/componentMask를 가진다
- stableID로 entity를 찾을 수 있다
- snapshot을 빌드할 수 있다
- 기존 렌더링 경로가 깨지지 않는다
- `JScenePanel`의 sample scene이 metadata를 생성한다

## Next Steps After This

이 단계 이후 자연스럽게 이어질 작업:

1. `dump_scene`
2. `set_transform` command
3. `scene save/load`
4. `PerObject / Transform GPU sync`
