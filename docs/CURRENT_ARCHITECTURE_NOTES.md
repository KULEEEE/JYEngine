# Current Architecture Notes

This file captures the current short-term rendering architecture direction.

## Current Decision

`JGraphicResource` should be treated as a **draw binding bundle**, not as the whole render database.

Recommended relation:

```text
JMaterial / JMesh / JTexture
-> JRenderServer sync
-> JRenderDB resource cache
-> JGraphicResource
-> JCommandQueue
```

## Why

- It keeps the GPU binding layer small.
- It matches the current engine scale.
- It leaves room for a future `JRenderServer` and `JRenderDB`.
- It supports later JSON export and AI tooling naturally.

## Intended Usage Shape

Example API direction:

```cpp
JGraphicResource gr(shader);
gr.SetConstantBuffer("PerObject", &objectData, sizeof(objectData));
gr.SetTexture("AlbedoMap", albedoTexture);

cmd->SetPipeline(pipeline);
cmd->SetGraphicResources(&gr);
cmd->DrawIndexed(...);
```

The user-facing API should be name-based, not slot-based.

## DOD Direction

The current structure should evolve toward DOD-friendly data flow:

- `JMaterial` is source authoring data
- `JRenderServer` gathers dirty source objects
- `JRenderDB` builds renderer-side derived data
- `JMaterialResource` should stay as derived render data, not user-authored state
- resource payloads should prefer flat entry arrays over nested object graphs when possible

## Important Rule

The renderer should not directly depend on high-level gameplay objects forever.

Preferred ownership split:

- gameplay/editor objects own authoring state
- render server synchronizes changes
- render database owns renderer-facing cache
- graphic resource owns final draw bindings
