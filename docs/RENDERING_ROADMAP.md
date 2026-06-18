# K.Engine — raising the fidelity ceiling

This is the research-backed plan for pushing K.Engine's visuals past the current
forward Blinn‑Phong look, plus the reasoning behind the grass rewrite that landed
alongside it. It's organized by **visual impact per unit of effort**, and every
item is tagged for **WebGL2/GLES3 feasibility** (the browser build is the
constraint; the RHI seam is what eventually lets the heavier items move to
Vulkan/WebGPU).

---

## 1. Grass (done now — and why)

The old grass was one camera‑facing billboard quad per blade with a flat 2‑colour
gradient and a single sine wind. Every reference on modern grass
(Sucker Punch's *Ghost of Tsushima* GDC 2021 talk, Acerola / SimonDev breakdowns,
al‑ro's WebGL demo) reaches the same conclusion: **billboard "cards" read as
cardboard and waste fill** — they have no silhouette, flatten when the camera
moves, and the overdraw + per‑fragment `discard` defeats early‑Z.

What shipped:

- **Multi‑segment curved blades.** Each blade is now a 6‑segment strip swept along
  a curve in the vertex shader, with a tapered, rounded tip — a real silhouette
  instead of a flat quad.
- **Per‑blade orientation + variation.** Blades face their own yaw (not all
  billboarded at the camera), with per‑blade wind phase, height, a resting lean,
  and hue drift between lush green and a drier yellow.
- **Lighting that sells it.** Per‑vertex normals from the curve tangent (plus a
  cross‑width tilt for a rounded look), half‑Lambert diffuse, a sky/ground
  *hemisphere* ambient, fake subsurface **tip translucency** (blades glow when
  back‑lit), root **ambient occlusion**, and a soft specular sheen.
- **Two‑frequency wind.** A low‑frequency directional gust plus a high‑frequency
  flutter, phased by world position so the field ripples instead of pulsing in
  lock‑step.
- **CPU distance culling.** Only blades within the grass view distance of the
  camera are uploaded (recomputed when the camera actually moves), so the GPU
  stops processing the entire field every frame.

### Next grass steps (not yet done)
- **Clumping** — *Ghost of Tsushima* uses Voronoi/cellular noise so blades grow in
  tufts sharing height + orientation. Big realism win, low cost. *(WebGL2: yes.)*
- **Segment LOD** — drop from 6 → 3 → 1 segments with distance; fade the most
  distant ring into a terrain "grass tint" rather than popping out. *(yes.)*
- **Density/clump maps** painted on the terrain. *(yes; GPU‑driven scatter with
  slope rejection wants compute/storage buffers → WebGPU.)*
- **GPU culling / indirect draw** to replace the CPU cull entirely. *(needs
  compute → WebGPU/Vulkan.)*

---

## 2. The fidelity ceiling, in priority order

### A. Tonemapping + sRGB correctness — **DONE**
The scene now renders to an HDR (RGBA16F) target and a fullscreen resolve pass
applies exposure + a tonemap operator (ACES / Reinhard / Filmic-Hejl, or *None*
for the raw linear look) + sRGB. Controls live under **Rendering (HDR / Tonemap)**
in the editor. On web, `EXT_color_buffer_float` is enabled and there's an RGBA8
fallback if a float colour buffer isn't renderable. This was the highest-ROI
step: lighting can now exceed 1.0 instead of clipping to flat white.

*Follow-ups here:* bloom is now cheap to add (bright-pass the HDR buffer before
the resolve), and auto-exposure / eye-adaptation could drive `tonemapExposure`.

### B. Cascaded shadow maps (sun) — **DONE**
A depth‑only pass renders shadow casters from the sun into a horizontally‑tiled
cascade atlas (3 cascades fit per camera‑frustum slice). Terrain reuses its cached
GPU buffers; props render as boxes. The main shader picks a cascade by view depth
and samples with 3×3 PCF, attenuating the sun term. Controls (enable / bias /
distance) are under **Rendering (HDR / Tonemap)**, sized off the `shadowMapSize`
dial. The path is fail‑safe — if the atlas or depth programs don't build, the
scene just renders unshadowed. Bias tuning against a running build is expected.

*Follow‑ups:* texel‑snapping to kill cascade shimmer, normal‑offset bias, and
letting grass receive/cast shadows.

### C. Post‑processing stack (bloom → color grade → optional TAA)
- **Bloom — DONE**: half‑res bright‑pass + two‑iteration separable blur, added to
  the HDR colour before the tonemap resolve. Threshold/intensity in the editor.
- **Color grading / LUT**: art‑directable mood for free.
- **TAA**: jittered sampling + history reproject — removes shimmer *and* makes
  cheaper SSAO/SSR/volumetrics viable by amortizing samples over frames. Medium
  cost; needs motion vectors. **WebGL2: yes‑with‑caveats** (works; history ping‑pong
  is fiddly).

### D. Ambient occlusion (SSAO) — **DONE**
The scene FBO's depth is now a sampleable texture; a half‑res SSAO pass
reconstructs view position + normal from it, samples a 16‑tap hemisphere, box‑
blurs the result, and the tonemap resolve multiplies the HDR colour by the AO
before tonemapping. Adds contact darkening in crevices and where geometry meets
the ground. Controls (enable / radius / bias / intensity) under **Rendering
(HDR / Tonemap)**; fully gated so it's a no‑op when off/unavailable. GTAO is the
higher‑quality follow‑up; applying AO to the ambient term only (vs the whole
colour) would be more physically correct once there's a separate ambient buffer.

### E. PBR + image‑based lighting
Move surfaces from Blinn‑Phong to metallic/roughness Cook‑Torrance and light the
ambient term from a prefiltered environment (even a procedural sky probe). More
consistent materials across lighting conditions. Medium effort (BRDF + IBL maps).
**WebGL2: yes.**

### F. Physically‑based sky + aerial perspective
Replace the constant fog colour + exp2 fog with a Hosek‑Wilkie / Bruneton sky and
**aerial perspective** (distance haze that takes its colour from the atmosphere
and sun angle). Makes draw distance read as *atmosphere* rather than a grey wall —
directly relevant at K.Engine's large render distances. Medium cost. **WebGL2: yes**
(precomputed LUTs); volumetric light shafts want more.

### G. Clustered / forward+ shading
The renderer caps point lights at ~16 and culls to the nearest few. Clustered
shading bins lights into froxels so hundreds light the scene. Do this when light
count becomes the bottleneck. **WebGL2: yes‑with‑caveats** (no compute, so build
the cluster grid on CPU or with a fragment pass).

### H. Terrain & vegetation detail
- **Triplanar + detail normal maps** on terrain to kill stretching on cliffs and
  add close‑up texture. *(yes.)*
- **Normal/parallax mapping** generally. *(yes / yes‑with‑caveats.)*
- **Virtual texturing** for unique high‑res terrain — large effort, future. *(needs
  newer API to do well.)*

### I. Global illumination (later)
Feasible at this tier: **light probes / irradiance volumes** (bake or update
sparsely) and **SSGI/SSR** (screen‑space, cheap‑ish, view‑dependent). Out of scope
until there's a deferred or forward+ base: **voxel GI** and **hardware ray
tracing** — these are the Vulkan/WebGPU payoff.

---

## If I only did five things, in order
1. ✅ **HDR + ACES tonemapping + sRGB** — done.
2. ✅ **Cascaded shadow maps** for the sun — done (3 cascades, PCF).
3. ✅ **Bloom** — done; color grading / LUT still open.
4. ✅ **SSAO** — done (16‑tap half‑res, depth‑reconstructed normals).
5. **PBR + a sky/IBL ambient** — consistent materials and grounded ambient light. *(next)*

Everything above is WebGL2‑reachable. The items that genuinely need a new backend
(GPU‑driven grass culling, compute clustered shading, voxel/RT GI) are exactly the
ones the RHI seam + planned Vulkan/WebGPU backends are there to unlock.

---

## Performance notes (the "takes a hit after a bit")
Cheap wins already applied: grass distance‑culling (was submitting up to 120k
blades every frame), no more per‑fragment distance `discard`, and cached
per‑light uniform locations (was a string lookup per light per frame).

**Terrain re-upload — DONE.** `DrawMesh` used to `glBufferData` every chunk every
frame even though terrain only changes on LOD switch or sculpt. There's now a
per-chunk GPU buffer cache (keyed by chunk index, invalidated by a monotonic
`TerrainChunk::MeshVersion`), so a chunk is only re-uploaded when its mesh
actually changes — removing several MB/frame of redundant uploads in the steady
state.

Remaining: the offscreen framebuffer is a fixed 2560×1440 regardless of window
size; tying its internal resolution to the fidelity dial would scale cost with
quality. Grass clumping/segment-LOD and GPU-driven culling are the other open
items above.
