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

### A. Tonemapping + sRGB correctness — **highest ROI, do first**
Render to an HDR target, then map to display with **ACES** or **AgX** and correct
sRGB. Right now lighting is summed and written straight to an 8‑bit buffer, so
bright areas clip to flat white and colour is mathematically off. This single
change makes *everything* — terrain, grass, lights — look "rendered" instead of
"OpenGL". Cheap, no scene changes. **WebGL2: yes** (render to a float/half FBO,
full‑screen resolve pass).

### B. Cascaded shadow maps (sun)
`QualitySettings` already carries a `shadowMapSize` dial with no pass behind it.
Add a depth‑only pass from the sun's POV split into 2–4 cascades by view depth,
sample with PCF. Grounds objects and terrain; by far the biggest "3D" cue after
tonemapping. Medium cost. **WebGL2: yes** (depth textures + PCF are core in GLES3).

### C. Post‑processing stack (bloom → color grade → optional TAA)
- **Bloom**: bright‑pass + separable blur + add. Cheap, high payoff once HDR exists.
- **Color grading / LUT**: art‑directable mood for free.
- **TAA**: jittered sampling + history reproject — removes shimmer *and* makes
  cheaper SSAO/SSR/volumetrics viable by amortizing samples over frames. Medium
  cost; needs motion vectors. **WebGL2: yes‑with‑caveats** (works; history ping‑pong
  is fiddly).

### D. Ambient occlusion (SSAO/GTAO)
Screen‑space AO from the depth buffer adds contact shadows and depth in crevices —
especially where grass meets terrain. SSAO is cheap; GTAO is the modern,
better‑grounded variant. **WebGL2: yes.**

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
1. **HDR + ACES/AgX tonemapping + sRGB** — transforms the whole image, days of work.
2. **Cascaded shadow maps** for the sun — wire up the dial that already exists.
3. **Bloom + color grading** — cheap once HDR is in.
4. **SSAO/GTAO** — contact shadows, grass‑meets‑ground depth.
5. **PBR + a sky/IBL ambient** — consistent materials and grounded ambient light.

Everything above is WebGL2‑reachable. The items that genuinely need a new backend
(GPU‑driven grass culling, compute clustered shading, voxel/RT GI) are exactly the
ones the RHI seam + planned Vulkan/WebGPU backends are there to unlock.

---

## Performance notes (the "takes a hit after a bit")
Cheap wins already applied: grass distance‑culling (was submitting up to 120k
blades every frame), no more per‑fragment distance `discard`, and cached
per‑light uniform locations (was a string lookup per light per frame).

The biggest remaining steady‑state cost is **terrain re‑upload**: `DrawMesh`
re‑`glBufferData`s every chunk every frame even though terrain only changes on
LOD switch or sculpt. Caching a GPU buffer per chunk (keyed by chunk id + a mesh
version that bumps on rebuild) and re‑uploading only dirty chunks is the next
perf item — it removes several MB/frame of redundant uploads. The offscreen
framebuffer is also a fixed 2560×1440 regardless of window size; tying its
internal resolution to the fidelity dial would scale cost with quality.
