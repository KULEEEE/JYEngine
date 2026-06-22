# JYEngine

<img width="2551" height="1367" alt="image" src="https://github.com/user-attachments/assets/91c430fd-7e6a-4080-a908-efbb584b1e93" />

```
Windows 10+  ·  Direct3D 12 (FL 11.0+)  ·  C++17  ·  Visual Studio + Win10 SDK
```

> GitHub: https://github.com/KULEEEE/JYEngine

[한국어](README.md) | **English**

---

## Overview

A custom DirectX 12 rendering engine built on Data-Oriented Design. Scene/Component data is converted into a render-only **Frame Snapshot** and **Draw Item Cache**, and the Renderer runs its Forward / Deferred path against cached, immutable frame data rather than the live Scene. Transforms use a SoA layout with a dirty index list to cut update cost, and RenderObject changes are folded into the Draw Item Cache.

Per-camera frustum culling builds the visible draw items, and the Deferred path is split into Shadow, Depth Pre-Pass, G-Buffer, SSAO, Lighting, and Forward Overlay passes. DirectX 12 resources, descriptors, command queues, render targets, and root signatures are wrapped behind an in-house rendering API, while shader reflection and the RenderDB manage material, mesh, transform, and light resources.

## End-to-end data flow
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
        ├──▶ JRenderSnapshotBuilder (camera view·projection, dirty transform → world, light snapshot)
        └──▶ JCameraRenderQueueBuilder (frustum 6-plane cull, visible opaque / transparent indices)
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
The rendering data flow is designed as a **single-direction structure**: `JScene → Snapshot → RenderDB → Renderer`. The Scene only manages the mutable source state, while the Renderer is isolated to reference only the per-frame finalized `JFrameDesc` and `RenderDB`.

This separation makes Scene change tracking simple and lets only changed data be updated. Because RenderPasses never depend directly on the mutable Scene, debugging boundaries stay clear, and stages such as sorting-queue construction and snapshot building become easy to parallelize and optimize — making the design well-suited to the Multi-Thread Rendering structure planned for the future.

## Data-Oriented Design

The core of Data-Oriented Design is laying out memory around data-access patterns to raise the cache hit rate and reduce unnecessary processing. Rather than object-level abstraction, data was partitioned around the **processing flows that repeat every frame**, grouping data needed by the same computation into contiguous structures so they can be accessed sequentially.

Likewise, not all data is reprocessed every frame — only the changed data and the **hot data** actually needed for rendering are selected and converted into the Snapshot and Draw Item Cache. This separates the Scene's data from the Renderer's data so the rendering pipeline operates on cache-friendly data.

A representative example is `JTransformPool`, which stores translation / rotation / scale each in a separate `std::vector` — a **SoA (Structure of Arrays)** layout — and accumulates only changed indices into a dirty ring, so only the transforms that changed that frame are converted to world matrices.

```cpp
// JTransformPool — SoA + dirty index ring
std::vector<JVec3>  _translations;
std::vector<JVec3>  _rotations;
std::vector<JVec3>  _scales;
std::vector<uint8>  _dirtyFlags;    // prevents duplicate pushes
std::vector<uint32> _dirtyIndices;  // only indices changed this frame
```

## Component Pool

The Scene does not scatter components per object; instead it **gathers components of the same kind into a Pool that owns them directly**. The key point is that the outside (Scene / systems) never holds a component's **address — only a lightweight handle** — and every actual access goes through the pool.

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

**Using handles instead of pointers** is the ultimate reason for the pool structure. If components were held directly by pointer, the moment the pool grows its internal array (`std::vector` reallocation) or relocates slots, all those addresses become invalid — so the memory layout effectively can't be changed. A handle re-resolves the location through the pool at access time, so external references never break no matter where the data moves. In other words, **because nobody holds an address, the pool is free to control data placement**, and the optimization of packing same-kind data contiguously / in SoA to raise the cache hit rate (the SoA split of `JTransformPool` seen earlier) becomes possible precisely on top of this. The generation packed into the handle distinguishes a slot that was deleted and reused, acting as a safety net that naturally filters out a stale handle pointing at a vanished component.

As a result, the irregular memory access of chasing pointers between objects disappears, and per-frame repeated work like Transform updates or RenderObject traversal becomes sequential access over contiguous arrays. Each component pool uses the **Entity index directly as the component slot index**, so one entity can have at most one component of a given kind, and lookup is pure indexing with no hashing.

```
Entity Index      0        1        2        3        4
EntityPool      active   active    free    active   active
RenderObject    mesh/                       mesh/    mesh/
Pool            material                    material material
TransformPool    SoA      SoA               SoA      SoA
```

## Snapshot

From a DOD perspective, the Snapshot is the stage that **rearranges Scene data into a render-ready form**. The Scene stores data in a structure that's good for managing Entities and Components, but the Renderer only needs immediately-usable data such as the per-frame computed world matrices, viewProjection, light parameters, and the visible draw item list.

So the Snapshot stage converts CPU-side component data into the form rendering needs, ensuring the Renderer never has to re-traverse the original Scene. This is a DOD design that reconstructs data **by actual usage flow rather than by the object's ownership relationships**, cutting unnecessary access and computation.

RenderObject changes are **incrementally applied** to the `JDrawItemCache` rather than fully rebuilt every frame. Driven by Scene events: `Added` appends a draw item, `Modified` patches the existing range in place, and only `Removed` triggers a rebuild.

## DirectX 12 abstraction

The explicit control of DirectX 12 is preserved, but a rendering **abstraction layer** was built so that higher-level RenderPasses don't have to deal directly with Command Allocators, Descriptor Heaps, Resource Upload, and Root Binding in detail.

### JRenderContext

JRenderContext handles GPU resource creation. It provides functions like `CreateVertexBuffer`, `CreateIndexBuffer`, `CreateConstantBuffer`, `CreateTextureFromFile`, `CreateShader`, `CreatePipeline`, and `CreateRootSignature`, simplifying the DX12 resource creation process into engine API calls.

```cpp
JVertexBuffer* vb      = renderContext->CreateVertexBuffer(vertices, vertexCount);
JIndexBuffer*  ib      = renderContext->CreateIndexBuffer(indices);
JTexture*      texture = renderContext->CreateTextureFromFile(path);
JPipeline*     pipe    = renderContext->CreatePipeline(shader);
```

Internally it handles DX12 procedures such as Default Heap creation, Upload Buffer copies, Resource Barrier transitions, and Descriptor creation. So the caller doesn't have to write complex code every time just to create a GPU resource.

### JCommandQueue

JCommandQueue simplifies recording rendering commands. It provides functions like `RenderBegin`, `BeginRenderPass`, `BeginDepthPass`, `SetPipeline`, `SetGraphicResources`, `BindVertexBuffer`, `DrawIndexed`, `EndRenderPass`, and `RenderEnd`, so a RenderPass can call just the commands it needs in order.

```cpp
commandQueue->BeginRenderPass(renderTarget, clearColor, 1);
commandQueue->SetPipeline(materialResource->pipeline);
commandQueue->SetGraphicResources(&graphicResource);
commandQueue->BindVertexBuffer(meshResource);
commandQueue->DrawIndexed(indexCount, 1, startIndex, 0, 0);
commandQueue->EndRenderPass();
```

It also manages RenderTarget state transitions, per-frame-resource Upload Buffers (a 16 MB constant-buffer upload ring), Descriptor Heaps, and Fence synchronization inside JCommandQueue. For when the same material is drawn multiple times within a frame, it caches the GPU descriptor table by a `(shader, texture set)` key. This lets a RenderPass focus solely on "which resources to bind to which target and which draw calls to issue."

### Shader-reflection-based binding

After shader compilation, `D3DReflect` collects cBuffer, Texture, and Sampler binding information. Based on this metadata the engine builds the Root Signature and Graphic Resource bindings, so **C++ code never has to manually track shader register indices**.

```
CompileShader()
 → D3DReflect(VS / PS)
 → collect BoundResources (CBUFFER / TEXTURE / SAMPLER)
 → build binding table
 → create root signature
```

cBuffers are bound as root CBVs, Textures are grouped into a descriptor table, and Samplers are baked into the root signature as static samplers. So instead of specifying slot indices in code, **just matching the same name as in the shader** wires up the binding.

```cpp
JGraphicResource::SetConstantBuffer("PerObject", buffer);
JGraphicResource::SetTexture("AlbedoMap", texture);
```

Going one step further, the engine **infers even the sampler type from the name convention** — if a resource name contains `Shadow`, the engine auto-generates a reverse-Z comparison sampler (`GREATER_EQUAL` + border). For a new shader to use shadow comparison sampling, it only needs to match the name.

## Deferred Rendering Pipeline

The Renderer switches between `RenderPath::Forward` and `RenderPath::Deferred` at runtime. Instead of one giant shader, the Deferred path is split into **six independent passes**, each with clear inputs and outputs.

```
JRenderer::Render(JFrameDesc)
   │
   ├─▶ JShadowPass          cascade depth from the directional light (Texture2DArray)
   ├─▶ JDepthPass           camera-view reverse-Z depth pre-pass
   ├─▶ JGBufferPass         write Albedo / Normal / Material (3 MRTs)
   ├─▶ JSSAOPass            screen-space occlusion from depth·normal → AO target (Lighting blurs it)
   ├─▶ JLightingPass        sample G-Buffer with a full-screen triangle → PBR lighting + IBL
   └─▶ JForwardOverlayPass  forward overlay for transparency · debug · grid, etc.
```

The important design decision here is that **none of the passes know anything about the mutable Scene**. A pass's inputs are only the immutable `JFrameDesc` and `JRenderDB` (the GPU resource mirror), so each pass is close to a pure function that always produces the same output for the same input. That makes per-pass testing, repeatable replay of the same frame (replay / headless), and parallelization of the snapshot · culling stages structurally possible.

Also, each pass lazily creates its own shader · pipeline (`ensureResources`), and depth · shadow · gbuffer share the same draw-item list (`opaqueDrawItemIndices` = frustum culling result), so the **cost of adding a pass is about "adding one class."**

## Depth Pre-Pass + Reverse-Z

Before the full G-Buffer write, the depth of opaque objects is drawn once first. Establishing the final depth with a depth pre-pass lets later G-Buffer / Lighting passes skip the pixel-shader cost of occluded pixels.

A distinctive choice here is **unifying Reverse-Z as a single engine-wide convention**. The depth comparison function is set to `GREATER` and the depth clear value to `0` (near = 1, far = 0), and this convention applies identically not only to the depth pre-pass but to the G-Buffer, the shadow ortho projection, the comparison sampler, and even the "is this pixel empty" test. Because "0 = far, compare with GREATER" holds consistently everywhere depth is handled, adding more passes leaves little room for depth-handling mistakes. As a bonus, the floating-point depth distribution becomes denser at long range, reducing z-fighting.

Since the depth pre-pass and shadow pass ultimately **only need depth**, they share the same `depth_prepass.hlsl` shader. Passes were added, but only one copy of shader code is maintained.

```cpp
// JDepthPass / JShadowPass both reuse the same shader
_shader = renderContext->CreateShader(get_Engine_Shader_Path() + "\\depth_prepass.hlsl");
_pipeline = renderContext->CreatePipeline(
    _shader, /*alphaBlend*/ false, /*vertexInput*/ true, ...,
    /*depthWrite*/ D3D12_DEPTH_WRITE_MASK_ALL,
    /*depthFunc*/  D3D12_COMPARISON_FUNC_GREATER);  // reverse-Z
```

## Cascade Shadow Map (CSM)

A 4-tier Cascade Shadow Map was implemented for the directional light's wide-area shadows. Depth is drawn into a single `Texture2DArray` with `CASCADE_COUNT(4)` slices, split per cascade by DSV.

### Cascade split — self-contained from a single camera matrix

Since closer-to-camera regions need denser resolution, the view distance is divided by a non-linear ratio.

```cpp
// cumulative ratios — closer cascades cover a narrower range
constexpr float CASCADE_SPLIT_RATIOS[4] = { 0.05f, 0.16f, 0.42f, 1.0f };
```

A distinctive feature of this implementation is that it does not pass near/far plane values or extra parameters in as frame data — it derives every split from **only the camera's `inverseViewProjection`** (self-contained). It un-projects the NDC corners to get world-space frustum corners, then carves each cascade range by interpolating the corners.

```cpp
// NDC corners → world-space frustum corners (reverse-Z: z=1 near, z=0 far)
corners.nearCorners[i] = XMVector3TransformCoord({ndcX, ndcY, 1.0f, 0.0f}, inverseViewProjection);
corners.farCorners[i]  = XMVector3TransformCoord({ndcX, ndcY, 0.0f, 0.0f}, inverseViewProjection);
```

### Stable light-space volume

The orthographic projection volume is fit with a **bounding sphere** enclosing the 8 corners of each cascade slice. Being sphere-based, the volume size doesn't change as the camera rotates, reducing shimmering at shadow edges. A margin (`casterMargin`) is added so casters behind the slice (toward the light) are still included, and the ortho projection also swaps near/far to match the engine-wide reverse-Z.

```cpp
const XMMATRIX lightView = XMMatrixLookToLH(eye, lightDirection, up);
// swap near/far to match reverse-Z (GREATER, clear 0)
const XMMATRIX lightProjection =
    XMMatrixOrthographicOffCenterLH(-radius, radius, -radius, radius, /*zFar*/ zFar, /*zNear*/ 0.0f);
```

### Sampling — the shader decides even the sampler type

In the lighting pass, a cascade is chosen by the pixel's camera distance and then sampled with 2×2 PCF. Rather than creating the comparison sampler in C++ and passing it in, per the name convention described earlier (`Shadow`) the engine auto-generates a reverse-Z comparison sampler at the shader reflection stage. UVs outside the map are compared against the border (0), so `ref >= 0` → always treated as lit.

```hlsl
Texture2DArray ShadowMap : register(t4);
SamplerComparisonState ShadowSampler : register(s0); // name contains Shadow → comparison sampler auto-generated

float referenceDepth = saturate(shadowPos.z + CascadeBiases[cascade]); // per-cascade reverse-Z bias
shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(uv, cascade), referenceDepth);
```

The depth bias is specified once in world units and divided by each cascade's own `zFar` to convert to reverse-Z depth units. As a result, near and far cascades have a consistent bias thickness.

## Deferred PBR / Lighting

### G-Buffer layout

| MRT | Format | Contents |
|-----|--------|----------|
| Albedo   | `R8G8B8A8_UNORM`     | base color (× material baseColor) |
| Normal   | `R16G16B16A16_FLOAT` | world-space normal (0.5+0.5 encoded) |
| Material | `R8G8B8A8_UNORM`     | R=Roughness, G=Metallic, B=Occlusion |
| Depth    | `D32_FLOAT`          | reverse-Z depth (shared with pre-pass) |

Material textures read the Bistro/glTF-convention **ORM packing (R=Occlusion, G=Roughness, B=Metalness)** directly, getting all three values in a single sample.

### Tangent-free normal mapping (screen-space derivatives)

Vertex tangents are not stored / imported separately for normal mapping. Instead, the G-Buffer pass **builds a cotangent frame on the fly from screen-space derivatives (ddx/ddy)** to apply the normal map (Schüler's cotangent frame). Because normal mapping works even when the mesh format has no tangent channel, the FBX import pipeline and vertex layout stay that much simpler.

```hlsl
// Build TBN without vertex tangents. ddx/ddy are valid only during rasterization, so the normal is finalized in the G-buffer pass.
float3 dp1 = ddx(worldPos);  float3 dp2 = ddy(worldPos);
float2 duv1 = ddx(uv);       float2 duv2 = ddy(uv);
float3 tangent   = cross(dp2, normal) * duv1.x + cross(normal, dp1) * duv2.x;
float3 bitangent = cross(dp2, normal) * duv1.y + cross(normal, dp1) * duv2.y;
```

A BC5-compressed normal map stores only RG, so Z is reconstructed as `sqrt(1 - x² - y²)`.

### Lighting (Cook-Torrance PBR)

The Lighting pass covers the screen with a single full-screen triangle and reads the G-Buffer directly with `Load` to compute per-pixel PBR.

- **BRDF**: Cook-Torrance model with GGX (normal distribution) + Smith (geometry attenuation) + Schlick Fresnel.
- **Lights**: `w <= 0` is a directional light (no attenuation); `w > 0` is a point light with inverse-square + range windowing (UE/Frostbite-style) attenuation.
- **Shadows**: all directional lights share the cascade shadow based on the first directional light.
- **AO**: occlusion (SSAO) is applied only to indirect light (ambient); direct-light occlusion is left to the shadow map — the roles are separated.
- **Tonemapping**: the final color is tonemapped with the **ACES filmic approximation (Narkowicz 2015)** and then gamma corrected. It preserves contrast and saturation better than Reinhard.

Thanks to the global reverse-Z convention, "pixels where nothing was drawn (depth == 0 = far plane)" are culled with a single comparison to skip lighting — the benefit of unifying the depth convention carries straight through to the lighting stage too.

```hlsl
float depth = DepthBuffer.Load(int3(pixel, 0)).r;
if (depth <= 0.0f) return float4(albedo, alpha); // far plane = unwritten pixel
...
color = ACESFilm(ambient + direct);
color = pow(saturate(color), 1.0 / 2.2);
```

## Scene-captured IBL + SSAO (Global Illumination)

With direct light (CSM) alone, shadowed regions fall pitch black. To fill in indirect light, a **split-sum IBL that bakes from a directly captured static scene** and **screen-space SSAO** were added. Instead of an external HDRI, it **captures the scene itself as the environment map**, so reflections and indirect light match the actual scene.

```
[bake once after load]                          [every frame]
 scene's 6 faces from probe → HDR cube capture
        │  (generate irradiance / prefiltered / BRDF LUT)
        ▼                                        LightingPass:
 ┌─ irradiance cube (diffuse GI) ─┐               ambient = kD·irradiance·albedo
 ├─ prefiltered cube (specular)   ├──resident(sample only)──▶  + prefiltered·(F·brdf.x + brdf.y)
 └─ BRDF LUT (F term integral)    ─┘               (× SSAO)
```

### 1. Scene capture — reusing immutable frame data

From the probe (main camera) position, the **6 faces are rendered into an HDR cube (`R16G16B16A16F`) at 90° FOV with reverse-Z**. Here the single-direction data-flow design pays off directly — rendering the same scene 6 times through the same deferred path just by **building a fresh `JFrameDesc` per face** (probe camera + face direction + cube face target) is all it takes. Since the IBL source must be linear, the capture path **skips tonemapping/gamma** and records linear HDR.

### 2. Convolution — render-to-cube-face without compute

The engine has no compute pipeline, so all convolution is done by **rendering directly to cube faces with full-screen passes**.

- **irradiance cube**: cosine-weighted integral of the capture cube → low-frequency diffuse GI (a small 32² cube is enough)
- **prefiltered cube**: GGX importance sampling per roughness, prefiltered per mip → blurrier reflections at higher roughness
- **BRDF LUT**: a 2D LUT integrating the split-sum F term over `(NdotV, roughness)`. Environment-independent, so it's baked only once.

### 3. Lighting — applying split-sum

```hlsl
float3 F   = FresnelSchlickRoughness(NdotV, f0, roughness);
float3 kD  = (1 - F) * (1 - metallic);
float3 diffuseIBL  = IrradianceMap.Sample(s, N).rgb * albedo;
float3 prefiltered = PrefilteredEnv.SampleLevel(s, reflect(-V, N), roughness * MaxMip).rgb;
float2 brdf        = BRDFLUT.Sample(sClamp, float2(NdotV, roughness)).rg;
float3 ambient = (kD * diffuseIBL + prefiltered * (F * brdf.x + brdf.y)) * ao;
```

The three resulting cubes **stay resident throughout runtime** and are merely texture-sampled each frame — since the scene is static, a single bake suffices and the runtime cost is near zero. Pixels with no geometry are filled with a procedural sky, used simultaneously as the **skybox background and the IBL upper-hemisphere light source**.

> The engine's name-convention binding continues here too — a rule was added so that a resource name containing `LUT` **auto-generates a clamp sampler** (preventing edge bleed) for the BRDF LUT, so the shader only needs to match the name with no slot/sampler setup.

### 4. SSAO — routing around a data problem with real-time occlusion

This asset's "ORM" textures are actually **Specular maps**, so the baked occlusion (R channel) was near 0, and multiplying it through killed indirect light wholesale, making shadows look pitch black. Instead of relying on baked AO, occlusion is now **computed in real time from depth·normal by `JSSAOPass`** and multiplied only into indirect light.

- AO is **rendered into a single-channel target in a dedicated pass** and **read back with a box blur** in lighting to remove grainy noise.
- The sample radius is taken in **world space** and converted to screen pixels. A fixed screen-pixel radius makes the world footprint grow excessively at long range, causing blotches that cross faces at different depths; anchoring it in world space gives consistent occlusion regardless of distance.

> **Design trade-off**: being a single probe, there's no per-position reflection parallax, and it's a 1-bounce static capture. This doesn't show in a static-scene showcase, and the structure is left open to extension via multiple probes / multi-bounce re-bake.

## AI-friendly design

Anticipating connecting an AI agent to the engine in the future, the engine architecture was designed to be easy for AI to understand and extend. The roles of Scene, Snapshot, RenderDB, Renderer, and RenderPass are clearly separated, and the data flow is kept close to single-direction so each stage's responsibility and input/output are easy to trace. The low-level DX12 API is also wrapped behind in-house APIs like `JRenderContext`, `JCommandQueue`, and `JGraphicResource`, so when AI adds or modifies features it can work against clear engine-level interfaces rather than the complex DX12 details.

The Scene save format is likewise JSON-based so AI tools can understand it easily. A Scene is stored not as binary but as an explicit JSON structure split into `entities` / `materials` / `meshes`, and each Entity has meaningful field names like `stableID`, `name`, `transform`, `camera`, `light`, and `renderObjectComponent`.

```json
{
  "stableID": "ent_camera_001",
  "name": "MainCamera",
  "transform": { "translation": [0, 3, -5], "rotation": [0, 0, 0], "scale": [1, 1, 1] },
  "camera": { "primary": true, "nearP": 0.1, "farP": 1000.0 }
}
```

This structure is decoupled from the engine's internal Pool/Handle-based runtime structure. **At runtime it uses a performance-oriented data layout**, while **for saving/editing it prioritizes readability and automation potential.**

## FBX importer + texture cooking

`JAssetImporter` is a separate executable that uses OpenFBX to convert FBX → mesh / material / texture / scene JSON. Textures are cooked at import time into a `.jtex` container (including mip chain and compressed format), and the runtime uploads them straight to the GPU with a dedicated reader. Build and import are separated to keep the FBX dependency out of the runtime.

```
JAssetImporter <source.fbx> <projectDir>
```

---

## Module layout

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
│   │                passes: SceneColor / Shadow / Depth / GBuffer / SSAO / Lighting / ForwardOverlay,
│   │                JShadowMap (CSM), JReflectionProbe (IBL)
│   ├── asset/       JMaterial, JMesh, JShader, JTextureFile (.jtex)
│   ├── platform/    JDevice
│   └── dx12/        JDx12Helper, d3dx12 wrapper
├── client/
│   ├── editor/      JSceneManager, JScenePanel, JSceneBuilder, JImGuiLayer,
│   │                JFBXLoader, JAssetManager
│   └── OpenFBX/     vendored OpenFBX
tools/
└── JAssetImporter/  FBX → project JSON/.jtex converter
samples/
└── GaussianSmoke/   particle smoke demo (separate executable)
SampleProjects/
└── BistroExterior/  project.json-based sample project (deferred + CSM + IBL)
benchmark/           OOP / AoS / SoA / WorldMatrix data-layout comparison (optional)
third_party/
├── nlohmann/json
└── imgui            editor UI
```

Build targets are as follows.

| Target               | Form         | Notes                                      |
|----------------------|--------------|--------------------------------------------|
| `Engine`             | static lib   | DX12 / DXGI / D3DCompiler / WindowsCodecs  |
| `Client`             | WIN32 exe    | host app serving as editor/viewer (ImGui)  |
| `JAssetImporter`     | console exe  | FBX → project/.jtex conversion tool        |
| `GaussianSmokeSample`| WIN32 exe    | particle smoke demo                        |
| `Benchmark`          | console exe  | `BUILD_BENCHMARK=ON` option. oneTBB FetchContent |

---

## Build

### Requirements

- Windows 10 or later
- A Direct3D 12-capable GPU (`D3D_FEATURE_LEVEL_11_0`+)
- Visual Studio 2019/2022 + C++ desktop workload, Windows 10 SDK
- Optional: *Graphics Tools* (D3D12 debug layer)

### Build steps

```bat
cmake -S . -B build
cmake --build build --config Release
```

To build the benchmark too:

```bat
cmake -S . -B build -DBUILD_BENCHMARK=ON
cmake --build build --config Release
```

`Client` is set as Visual Studio's default startup project.

---

## Run

Command-line options when running `Client`:

```
Client.exe                       # default sample scene (res/scene/sample.jscene.json)
Client.exe --new                 # start with an empty scene (-n)
Client.exe --scene <path>        # a specific scene file (-s)
Client.exe --project <path>      # a project folder (-p, searches for project.json)
Client.exe <projectPath>         # a positional argument is also interpreted as a project folder
```

Example: open the bundled Bistro sample project

```
Client.exe --project SampleProjects/BistroExterior
```

Import an FBX as a project:

```
JAssetImporter <source.fbx> <projectDir>
```

Under `<projectDir>`, the directories `scenes/`, `meshes/`, `materials/`, `textures/`, `shader/` are created; materials and meshes are saved as JSON and textures as `.jtex`.

---

## Third-party code

- [nlohmann/json](https://github.com/nlohmann/json) — `third_party/nlohmann/`
- [Dear ImGui](https://github.com/ocornut/imgui) — `third_party/imgui/` (editor UI)
- [OpenFBX](https://github.com/nem0/OpenFBX) — `src/client/OpenFBX/`
- [oneTBB](https://github.com/oneapi-src/oneTBB) — benchmark target only, fetched via CMake FetchContent

Each library is subject to its own license.
