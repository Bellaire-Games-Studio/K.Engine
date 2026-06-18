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
- [X] GPU procedural terrain texturing (height/slope material splatting)
- [X] GPU-instanced grass (wind-animated, density from the fidelity dial)
- [X] Physics (rigidbody, AABB/sphere, raycasts, terrain collision)
- [X] Entity Component System
- [X] Scene editor: World Explorer (hierarchy) + Properties (per-component) inspector
- [X] Play / Pause / Stop (physics + scripts frozen while editing; non-destructive)
- [X] Scene save / load (`.kscene` text format)
- [X] C++ scripting (`ScriptBehavior` behaviours, native + web)
- [X] Adjustable fidelity / LOD ("0.65" dial)
- [ ] Audio
- [ ] Animation
- [X] Online/In-browser (Emscripten / WebGL2)
- [X] Native desktop (OpenGL 3.3 core via GLFW + GLEW) — Linux build+run verified
- [ ] WebGPU backend *(planned; RHI seam in place)*
- [ ] Vulkan backend *(planned)*
- [ ] Multithreading

## Modules

| Module | Contents |
| --- | --- |
| `K.Engine.Core` | Application loop, window, layer stack, ImGui editor shell, demo |
| `K.Engine.Graphics` | Batched renderer (quads/cubes), `DrawMesh` for terrain, camera, lights |
| `K.Engine.Physics` | Shapes, collision manifolds, rigidbodies, `PhysicsWorld` (broadphase + solver), raycasts |
| `K.Engine.Terrain` | `HeightField`, LOD `TerrainChunk` meshing, `Terrain` (noise gen + sculpting) |
| `K.Engine.Common` | ECS, math, events, `QualitySettings` dial, `Transform`, scene components, noise |
| `K.Engine.Input` | Keyboard/mouse polling, platform window |
| `K.Engine.Editor` | Scene serializer (`.kscene`) + C++ scripting (`ScriptBehavior`, registry, built-ins) |

## Editor, scenes & scripting

The app is an editor over an ECS world. Every world item is an entity with
components (`Transform`, `Prop`, `LightSource`, `Rigidbody`, `Collider`,
`Script`, ...); the **Explorer** lists them and the **Properties** panel edits
them. **Play** snapshots the world and runs physics + scripts; **Stop** restores
the snapshot, so editing is always non-destructive. **File → Save/Open Scene**
serializes the world (and environment) to a `.kscene` text file.

Game logic lives in C++ behaviours — *engine for the game, not games for the
engine*. A behaviour derives from `KDot::ScriptBehavior`, uses the `KDot`
namespace, and is registered by name; attach it via a `Script` component. Because
behaviours compile into the binary, the same code runs on native **and** web
(no separate scripting VM):

```cpp
class Spin : public KDot::ScriptBehavior {
    void OnUpdate(float dt) override {
        if (auto* t = GetTransform())
            t->rotation = glm::angleAxis(glm::radians(90.0f * dt),
                                         glm::vec3(0, 1, 0)) * t->rotation;
    }
};
KE_REGISTER_SCRIPT(Spin, "Spin"); // now selectable in the Script component

```

Built-ins: `Spin`, `Hover`, `Patrol` (see `K.Engine.Editor/src/Script/BuiltinScripts.cpp`).

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
- **Procedural terrain texturing (GPU)**: terrain is shaded in the fragment
  shader from world height + slope, blending sand/grass/rock/snow and broken up
  with GPU value-noise detail — no texture assets required, all per-pixel work
  on the GPU.
- **GPU-instanced grass**: the CPU scatters blade instances once (`GrassField`,
  density + view distance from the fidelity dial, skipping water/steep/rock);
  the GPU draws them in **one instanced call**, doing the billboarding, taper,
  wind sway and distance fade in the vertex shader.
- **2D pass**: `Renderer::Begin2D/End2D` switch to an orthographic, unlit,
  alpha-blended mode for HUD/sprites (the demo draws a crosshair, status bars,
  and a cursor brush marker).
- **Mouse**: right-drag to look, scroll to zoom, and **left-click to sculpt**
  the terrain at the picked point. Picking builds a world ray from the cursor
  (`Picking::ScreenToRay`) and queries `PhysicsWorld::Raycast`.

Demo controls: `WASD` fly · right-drag look · scroll zoom ·
left-click sculpt (hold `Shift` to lower) · `R` re-drop the ball.

## Cross-platform

The engine now builds on both the web (Emscripten/WebGL2) and **native desktop**
(GLFW + GLEW, OpenGL 3.3 core). Platform differences are isolated:

- `Platform/Platform.hpp` — platform detection (`KE_PLATFORM_WEB/WINDOWS/LINUX/MACOS`).
- `Platform/GL.hpp` — one place for the GL + window-system includes.
- `Window::Create` returns a `JavascriptWindow` (web) or `DesktopWindow` (native);
  the main loop is `emscripten_set_main_loop` on web and a plain `while` loop on desktop.
- One set of shaders authored as GLSL ES 3.00; `ShaderUtil` rewrites them to
  GL 3.3 core on desktop on the fly.

### Roadmap: WebGPU & Vulkan

The gameplay systems (terrain, physics, ECS, config, math) are graphics-API
agnostic, so the remaining work is backend-only, behind the RHI seam
(`RHI/` — `Device`/`Buffer`/`Pipeline` interfaces + `GraphicsAPI.hpp`, selected
via the `KE_BACKEND_*` CMake options):

1. ✅ Native desktop OpenGL (window/loop/RHI seam, shader adaptation).
2. ✅ RHI device abstraction + OpenGL backend (`RHI/GLDevice`); `GrassRenderer`
   ported onto it (buffers + pipeline + std140 uniform buffer, zero direct GL).
3. **Vulkan** — implement `rhi::Device` for Vulkan; port the remaining renderer.
   Native high-performance desktop path.
4. **WebGPU** — implement `rhi::Device` for WebGPU; move the web target onto it.

## Building (web)

- Install [Visual Studio 2019](https://visualstudio.microsoft.com/downloads/), [CMake](https://cmake.org/download/), [Git](https://git-scm.com/downloads), and [Emscripten](https://emscripten.org/docs/getting_started/downloads.html)
- Clone the repository
- run `build.bat`
- Open `build/bin/K.Engine.html` in your browser

## Building (native desktop)

Install the dependencies, then a normal CMake build:

- **Linux**: `sudo apt install build-essential cmake libglfw3-dev libglew-dev libgl1-mesa-dev`
- **macOS**: `brew install cmake glfw glew`
- **Windows**: install [CMake](https://cmake.org/download/) + Visual Studio, and
  `vcpkg install glfw3 glew` (configure with the vcpkg toolchain file)

```sh
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native -j
./build-native/bin/K.Engine
```

The platform-agnostic modules (Physics / Terrain / Common) can also be compiled
and unit-tested with a normal host compiler (e.g. `g++ -std=c++17 -I glm ...`),
independently of any graphics backend.
