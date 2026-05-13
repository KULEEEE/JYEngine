# Source Layout

Engine source is grouped by responsibility instead of keeping all public headers
and private implementations in one flat directory.

## Engine

- `src/engine/core`: engine lifetime, context, base object/pool/hash utilities.
- `src/engine/scene`: scene-facing CPU data pools and scene queries.
- `src/engine/render`: renderer-facing frame data, render passes, RenderDB, render resources, command/render target/swapchain abstractions.
- `src/engine/platform`: low-level platform/device objects.
- `src/engine/asset`: asset authoring/runtime objects such as material, mesh, shader.
- `src/engine/dx12`: DirectX 12 helper code.

Private implementations mirror the public structure:

- `src/engine/private/core`
- `src/engine/private/scene`
- `src/engine/private/render`
- `src/engine/private/platform`
- `src/engine/private/asset`

## Include Rule

Use module-qualified includes from the `src` include root:

```cpp
#include "engine/render/JRenderer.h"
#include "engine/scene/JScene.h"
#include "engine/core/JEngineContext.h"
```

Avoid adding new public engine headers directly under `src/engine` unless they are
global bootstrap headers such as `precompile.h`.
