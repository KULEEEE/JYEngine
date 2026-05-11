# AI-Friendly Engine Vision

This document captures the intended long-term direction of `JYEngine` so the project context can be recovered on another machine or in a future session.

## Project Goal

`JYEngine` is not only a DirectX 12 mini engine. The target is an **AI-friendly rendering engine**:

- AI should be able to read engine state through structured data.
- AI should be able to diagnose rendering problems from exported debug data.
- AI should be able to safely control the engine through a command layer.
- The engine should remain understandable as a graphics programmer portfolio project.

The key idea is:

> Do not build an engine with AI embedded everywhere first.
> Build an engine whose internal state is observable, structured, and externally controllable.

## Architecture Direction

Current preferred layering:

```text
Game Object / Asset
-> JRenderServer
-> JRenderDB
-> JGraphicResource
-> JCommandQueue
```

Extended target layering:

```text
Game Object / Asset
-> JRenderServer
-> JRenderDB
-> Render Graph / Profiler / Validator
-> JSON Export / Command API
-> External AI Agent
```

### Role of Each Layer

#### Runtime Objects

- `JEntity`, `JMaterial`, `JMesh`, `JTexture`
- Source data edited by the user or editor

#### `JRenderServer`

- Collect scene changes
- Track dirty state
- Decide which render resources must be updated
- Prevent the renderer from directly depending on gameplay objects

#### `JRenderDB`

- Renderer-facing cache database
- Hold `MaterialResource`, `MeshResource`, `TextureResource`, and pass-related resources
- Become the source of structured debug export

#### `JGraphicResource`

- Final draw binding bundle
- Hold the resources bound to a shader for one draw or one material pass
- Stay as the thin GPU binding layer, not the whole render database

#### Render Graph / Profiler / Validator

- Describe pass dependencies
- Track resource reads, writes, and barriers
- Measure GPU timings
- Produce structured validation results

#### Tool Bridge

- Export JSON snapshots
- Execute safe commands
- Connect to MCP, HTTP, or another external tool channel

## Design Principles

### 1. Observability First

The engine should expose structured state that both humans and tools can consume:

- frame stats
- pass list
- draw call list
- pipeline state
- resource barriers
- texture and buffer usage
- shader reflection data
- scene hierarchy
- warnings and validation results

Preferred export format: JSON.

### 2. AI Uses Structured Data, Not Raw Guessing

The AI side should consume data like:

- frame capture
- render graph dump
- shader reflection dump
- validation reports
- screenshots

The engine should not require the AI to infer everything from free-form logs.

### 3. Command Layer Over Natural-Language Control

The engine should provide explicit commands such as:

- `capture_frame`
- `dump_scene`
- `dump_render_graph`
- `dump_gpu_profile`
- `set_material_parameter`
- `reload_shader`
- `toggle_render_pass`
- `take_screenshot`

Natural-language interpretation should happen outside the engine.

### 4. Keep the AI External

Preferred integration model:

```text
JYEngine
-> JSON / Command / Screenshot / Logs
-> Local Tool Server
-> MCP or HTTP
-> Claude / ChatGPT / Local LLM Agent
```

This keeps the engine lighter and makes the AI provider replaceable.

## MVP Roadmap

### Phase 1: DX12 Mini Renderer

- window
- device and swap chain
- command queue
- descriptor heap
- buffer and texture abstraction
- mesh rendering
- camera
- basic lighting model

### Phase 2: Render Pass Structure

- depth pass
- opaque pass
- skybox
- post process
- debug views

### Phase 3: GPU Profiling

- timestamp queries
- per-pass GPU time
- CPU frame time
- draw call count

### Phase 4: Frame Capture Export

- pass info to JSON
- draw event info to JSON
- pipeline state to JSON
- screenshot save

### Phase 5: AI Bridge

- HTTP or MCP server
- `capture_frame`
- `dump_render_graph`
- `set_debug_view`
- `reload_shader`

### Phase 6: AI Analysis Features

- frame bottleneck analysis
- draw call diagnosis
- render graph validation
- shader error explanation

## Immediate Implementation Guidance

Near-term implementation order for this repository:

1. Make `JGraphicResource` the draw binding object.
2. Add `JMaterial` and material parameter binding by shader resource name.
3. Introduce a small `JRenderDB` for cached render resources.
4. Add a lightweight `JRenderServer` for dirty synchronization.
5. Add pass descriptions and structured frame capture.

## Non-Goals For Now

Avoid doing these too early:

- embedding an LLM directly inside the engine
- building a huge Unity-scale render database from the start
- making AI generate gameplay or full scenes before render tooling exists

The engine should first become easy to inspect and safe to drive.

## Why This Matters

The target portfolio is not:

> "A toy engine with AI features."

The target portfolio is:

> "A DirectX 12 mini engine designed for AI-assisted rendering analysis, debugging, and tooling."

That difference should influence the architecture from the beginning.
