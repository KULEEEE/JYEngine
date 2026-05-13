# Scene Data Next Prompt

Date: 2026-05-14

## Current State

`ScenePanel` sample scene hardcoding has been split into a scene-data build path.

Implemented so far:

- `JSceneData` plain data structs exist in `src/engine/scene/JSceneData.h`.
- `JScene` now has entity metadata: `stableID`, `name`, `tags`, `componentMask`.
- `JSceneBuilder` converts `JSceneData` into runtime `JScene`, materials, meshes, textures, and render-server registrations.
- `MakeSampleSceneData()` temporarily creates the current sample scene as code-defined scene data.
- `JScenePanel` now calls `MakeSampleSceneData()` and `JSceneBuilder::Build()` instead of constructing the sample scene directly.
- Debug build passed with:

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

## Important Design Decision

`MakeSampleSceneData()` is temporary. It exists only to validate the `JSceneData -> JSceneBuilder -> JScene` boundary before adding JSON IO.

The intended final flow is:

```text
res/scene/sample.jscene.json
-> JSceneSerializer
-> JSceneData
-> JSceneBuilder
-> runtime JScene
```

Do not add snapshot/dump_scene yet. Snapshot is for runtime observation later. The next step should focus on authoring/save-load data.

## Next Prompt

Use this prompt to continue:

```text
JYEngine currently has the ScenePanel sample scene split into a JSceneData + JSceneBuilder path.

The next task is the first JSON-based scene save/load step.

Goals:
1. Add JSON parser/serializer support, isolated to a Scene IO layer.
2. Make `JSceneData` loadable from JSON and serializable back to JSON.
3. Move the current `MakeSampleSceneData()` content into `res/scene/sample.jscene.json`.
4. Make `JScenePanel` load `sample.jscene.json` and pass the loaded `JSceneData` to `JSceneBuilder::Build()`.
5. Remove `MakeSampleSceneData()` from the normal runtime path, or keep it only as a temporary fallback/fixture when file loading fails.

Rules:
- Do not add a JSON dependency inside `JScene`.
- Do not make `JSceneBuilder` know about the JSON parser directly.
- Keep JSON IO in a separate `JSceneSerializer` or `JSceneIO` layer.
- Do not store runtime handles, raw pointers, or GPU resources in JSON.
- JSON should store only stableID, name, transform, camera, light, renderObject, material asset id, mesh asset id/path, shader path, and texture path.
- snapshot/dump_scene, command API, and JPool SoA refactoring are out of scope for this step.

Recommended structure:
- `src/engine/scene/JSceneSerializer.h`
- `src/engine/private/scene/JSceneSerializer.cpp`
- `res/scene/sample.jscene.json`

Recommended JSON parser:
- Prefer adding the single-header `nlohmann/json` as `third_party/nlohmann/json.hpp`.
- Avoid adding package-manager or CMake dependency complexity for now.

Completion criteria:
- `sample.jscene.json` represents the current cup/grid/camera/light sample scene.
- `JSceneSerializer::LoadFromFile()` or an equivalent API can produce `JSceneData`.
- `JScenePanel` passes the JSON-loaded `JSceneData` to the builder and keeps the existing scene behavior.
- Debug build passes.
```

## Follow-Up After JSON

After this JSON step is stable:

1. Remove or move `MakeSampleSceneData()` to test/fixture-only code.
2. Add `SaveToFile()` for editor-authored scene data.
3. Add `dump_scene` snapshot later as runtime observation data.
4. Revisit `JPool` storage and migrate hot components, starting with `Transform`, toward SoA when performance work begins.
