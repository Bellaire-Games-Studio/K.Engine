# K.Engine
<img width="1918" height="922" alt="image" src="https://github.com/user-attachments/assets/531c1653-c5fd-4161-ad41-7502e279f715" />

K.Engine is a WIP custom game engine written in C++, targeting a fidelity level
**roughly two-thirds of the way from Iruna Online to Elden Ring (~0.65)** with a
single, runtime-adjustable detail dial.

You can test the current development build [here](https://bellaire-games-studio.github.io/K.Engine/build/bin/K.Engine.html).
The build folder holds a current build of the repository.

## Features

- [X] 2D Rendering (orthographic sprite/HUD pass)
- [X] 3D Rendering
- [X] Lighting (ambient + directional sun + point lights, Blinn-Phong, fog)
- [X] Mouse input + screen-ray picking (click-to-sculpt, look/zoom)
- [X] Procedural Terrain (chunked, LOD, runtime sculpting)
- [X] Physics (rigidbody, AABB/sphere, raycasts, terrain collision)
- [X] Entity Component System
- [X] Adjustable fidelity / LOD ("0.65" dial)
- [ ] Grass / vegetation *(next)*
- [ ] Texture splatting *(next)*
- [ ] Audio
- [ ] Animation
- [X] Online/In-browser (Emscripten / WebGL2)
- [ ] Native Windows / Linux *(roadmap)*
- [ ] WebGPU backend *(roadmap)*
- [ ] Multithreading

## Modules

| Module | Contents |
| --- | --- |
| `K.Engine.Core` | Application loop, window, layer stack, ImGui editor shell, demo |
| `K.Engine.Graphics` | Batched renderer (quads/cubes), `DrawMesh` for terrain, camera, lights |
| `K.Engine.Physics` | Shapes, collision manifolds, rigidbodies, `PhysicsWorld` (broadphase + solver), raycasts |
| `K.Engine.Terrain` | `HeightField`, LOD `TerrainChunk` meshing, `Terrain` (noise gen + sculpting) |
| `K.Engine.Common` | ECS, math, events, `QualitySettings` dial, `Transform`, `MeshData`, noise |
| `K.Engine.Input` | Keyboard/mouse polling, platform window |

## The fidelity dial

Everything detail-related reads from one master value in `QualitySettings`
(`K.Engine.Common/include/Core/QualitySettings.hpp`), in the range `[0, 1]`:

```cpp
KDot::QualitySettings::Get().SetFidelity(0.65f); // project default
```

| Fidelity | Render dist | Terrain chunk edge | Max LOD | Shadows |
| --- | --- | --- | --- | --- |
| 0.00 (Iruna)   | 120  | 17  | 2 | off |
| 0.65 (K.Engine)| ~1730| 65  | 5 | 2048 |
| 1.00 (Elden)   | 2600 | 129 | 6 | 4096 |

Changing it at runtime (there's a slider in the demo's Inspector panel)
re-derives render distance, LOD bands, terrain density, shadow map size, light
budget and texture sampling, so a settings menu only ever touches one call.

## Terrain

- Heightfield generated from multi-octave simplex noise with optional **domain
  warping** (organic shapes) and **ridged multifractal** (mountains).
- Tiled into chunks, each meshed at a **LOD chosen per-frame from camera distance**.
- **Double-sided skirts** hide cracks between chunks at differing LODs.
- Height/slope-based vertex tinting (sand → grass → rock → snow) as a stand-in
  for full material splatting.
- **Runtime sculpting**: raise / lower / flatten / smooth / set with a radial brush.
- The same heightfield feeds physics ground collision and raycasts.

## Physics

- `Rigidbody` + `Collider` (box / sphere) components stepped over the ECS.
- Semi-implicit Euler integration, **spatial-hash broadphase**, sequential-impulse
  resolution with friction and Baumgarte positional correction.
- Heightfield ground collision and world raycasts (against colliders + terrain).

## Rendering & input

- **Lighting**: ambient + a directional "sun" + point lights (culled to the
  nearest few, capped by the fidelity dial) with Blinn-Phong specular and
  exp2 distance fog. Lights live in `LightManager` (plain data) and are turned
  into shader uniforms by the renderer.
- **2D pass**: `Renderer::Begin2D/End2D` switch to an orthographic, unlit,
  alpha-blended mode for HUD/sprites (the demo draws a crosshair, status bars,
  and a cursor brush marker).
- **Mouse**: right-drag to look, scroll to zoom, and **left-click to sculpt**
  the terrain at the picked point. Picking builds a world ray from the cursor
  (`Picking::ScreenToRay`) and queries `PhysicsWorld::Raycast`.

Demo controls: `WASD` fly · right-drag look · scroll zoom ·
left-click sculpt (hold `Shift` to lower) · `R` re-drop the ball.

## Roadmap: cross-platform & WebGPU

The new gameplay systems (terrain, physics, ECS, config, math) are deliberately
**graphics-API-agnostic** — pure C++/GLM with no GL coupling — so they already
compile toward any backend. The remaining cross-platform work is isolated to the
window + render layers:

1. Abstract the window/main-loop off Emscripten (native GLFW + a desktop game loop).
2. Add a desktop GL path (glad/GLEW) behind the existing `Renderer` so Windows/Linux build.
3. Introduce a small RHI seam and a **WebGPU (Dawn/wgpu)** backend behind it.

## Building (web)

- Install [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/), [CMake](https://cmake.org/download/), [Git](https://git-scm.com/downloads), and [Emscripten](https://emscripten.org/docs/getting_started/downloads.html)
- Clone the repository
- run `build.bat`
- Open `build/bin/K.Engine.html` in your browser

The platform-agnostic modules (Physics / Terrain / Common) can also be compiled
and unit-tested with a normal host compiler (e.g. `g++ -std=c++17 -I glm ...`),
which is how they are validated independently of the WebGL build.
