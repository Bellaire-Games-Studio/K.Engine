# WebGPU backend — status & port plan

## Status: experimental scaffold (not runnable)

WebGPU is **not** a working backend yet. Until this scaffold it was only an enum
value + a CMake `#define` + a comment; `CreateDevice()` always returned the GL
device. What exists now:

- `RHI/WebGPUDevice.{hpp,cpp}` — an `rhi::Device` implementation against
  Emscripten's `webgpu.h`: buffer create/update, WGSL shader modules, render-
  pipeline assembly from `PipelineDesc`, a uniform bind group, and a
  `BeginFrame`/record/`EndFrame` skeleton. **Guarded by `KE_BACKEND_WEBGPU`**, so
  the default OpenGL build never compiles it.
- `CreateDevice()` now dispatches to `CreateWebGPUDevice()` under that define.
- `Assets/Shaders/3D/Grass.wgsl` — the grass shader translated to WGSL (one
  module, `vs_main` + `fs_main`, same `Constants` UBO layout).
- CMake adds `-sUSE_WEBGPU=1` to the web link when the option is on.

It is **not runnable** for two structural reasons:

1. **One canvas, one API.** A browser canvas exposes either WebGL2 *or* WebGPU,
   not both. The entire renderer must move at once — you can't run terrain/props
   on WebGL2 and grass on WebGPU side by side.
2. **The renderer is still mostly raw GL.** `GrassRenderer` and now the bloom
   chain go through the RHI, but the main `Renderer` is still hundreds of raw
   `gl*` calls (terrain, props, lighting, HDR, SSAO, shadows, tonemap), the 4
   scene shaders are GLSL, and the editor UI renders through `ImGui_ImplOpenGL3`.

## Why the RHI has to grow first

The current `rhi::Device` is a tiny immediate interface (CreateBuffer /
CreatePipeline / Bind* / DrawInstanced) that only covers what grass needs. WebGPU
(and Vulkan) are command/pass based and need concepts the seam doesn't model yet:
**textures & samplers, render targets / framebuffers, render passes with depth
attachments, index buffers, multiple bind groups, and explicit frame
submission**. The HDR/bloom/SSAO/shadow passes all rely on framebuffers and
sampled textures that the RHI doesn't expose.

## Staged plan

1. **Grow the RHI to cover the whole renderer.** Add Texture, Sampler,
   RenderTarget/Framebuffer, a render-pass abstraction, index buffers, and
   `BeginFrame`/`EndFrame`/`Present`. Keep the GL backend (`GLDevice`) passing the
   existing tests at every step — it stays the reference implementation.
   *In progress:* `Texture`, `RenderTarget`, `BeginRenderPass`/`EndRenderPass`,
   `BindTexture`, non-instanced `Draw`, and inline shader source are in (GL backend
   implemented). Still to add: samplers as first-class objects, index buffers +
   `DrawIndexed`, depth attachments on render targets, and frame submission hooks.
2. **Port `Renderer.cpp` onto the RHI.** Replace the 444 `gl*` calls with RHI
   calls, pass by pass (scene → shadow atlas → bloom → SSAO → tonemap resolve).
   This is the bulk of the work and is **independently verifiable on the GL
   backend** before any WebGPU runs.
   *In progress (444 → ~250 `gl*` calls so far):* the entire **post chain** is
   ported — the **offscreen scene target** (HDR colour + sampleable depth), **bloom**
   (bright-pass + separable blur), **SSAO** (occlusion + blur), and the **tonemap
   resolve** all run through `rhi::Device` (RHI textures/render-targets/pipelines +
   std140 `Post`/`Ssao`/`Resolve` blocks). `BindFrameBuffer`/`UnbindFrameBuffer` are
   now `BeginRenderPass`/`EndRenderPass` on the scene target; the scene itself is
   still drawn into it by raw GL between those calls. The `externalHandle` borrow
   seams are gone — the only remaining native-handle use is `GetFrameBufferTexture`
   handing the resolved texture to `ImGui::Image`. **Still raw GL:** the scene draw
   passes (`Flush`/`DrawMesh`/props), the shadow atlas, the 2D/HUD pass, and the
   main scene shaders. Next: the shadow atlas, then the main scene draw + its shader
   (which needs the per-frame lighting/material uniforms modelled as UBOs).
3. **Translate every shader to WGSL.** Scene (`FragmentShader`/`VertexShader`),
   grass (done), and the post/tonemap/SSAO/shadow programs. Keep the GLSL set for
   the GL backend; pick per backend (or compile WGSL→GLSL/SPIR-V via Tint).
4. **Finish `WebGPUDevice`.** Textures/samplers/depth attachments, a surface +
   swapchain (`wgpuInstanceCreateSurface` / configure), and **async device init**
   (`wgpuInstanceRequestAdapter` → `wgpuAdapterRequestDevice`, or
   `emscripten_webgpu_get_device()` with the device created in the shell). Cache
   bind groups instead of the scaffold's per-draw create.
5. **Swap the ImGui backend** to `imgui_impl_wgpu` in `ImGuiLayer`.
6. **Build & run.** `emcmake cmake -DKE_BACKEND_WEBGPU=ON`, `-sUSE_WEBGPU=1`
   (already wired), and async startup (`-sASYNCIFY` or a callback-driven loop)
   since device acquisition is asynchronous. Needs a WebGPU-capable browser.

## Notes / risks

- **API churn.** The WebGPU C API and WGSL are still moving; some `webgpu.h`
  field names (e.g. the WGSL descriptor's `code`, `wgpuRenderPassEncoderEnd` vs
  `…EndPass`) depend on the Emscripten SDK version — expect small edits when you
  first compile the scaffold.
- **Vulkan** would reuse stages 1–3 (the grown RHI + a shader story) and swap
  stage 4 for a Vulkan device — that's the desktop high-performance path.
- Doing stages 1–2 first is worthwhile **regardless of WebGPU**: a renderer
  behind a real RHI is cleaner and is the prerequisite for any explicit backend.
