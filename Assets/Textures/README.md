# Built-in textures

These are the engine's bundled, seamless (GL_REPEAT-tileable) textures. They are
**procedurally generated** by [`scripts/gen_textures.py`](../../scripts/gen_textures.py),
so they are **CC0 / public-domain by construction** — no third-party licensing,
safe to ship in the repo, and tiny (256×256 PNG, ~330 KB total). They ship to the
web build automatically because the whole `Assets/` folder is preloaded.

| File | Material |
| --- | --- |
| `uvgrid.png`  | UV-debug checker + grid lines |
| `brick.png`   | Running-bond brick |
| `planks.png`  | Wood planks |
| `cobble.png`  | Voronoi cobblestone |
| `tiles.png`   | Ceramic tiles + grout |
| `concrete.png`| Concrete / plaster noise |
| `grass.png`   | Grass ground |
| `metal.png`   | Brushed metal panels + rivets |

Apply one to a cube in the editor: select an entity with a **Prop**, open the
Prop section, and use **Built-in → pick…** (or **Image path** to load your own,
or **Checkerboard** for the runtime-generated one). Regenerate/extend the set
with `python3 scripts/gen_textures.py`.

## Adding real (photo) textures

If you want photographic materials, **only commit textures you're licensed to
redistribute**. Safe, truly free sources (CC0 = public domain, no attribution
required, redistribution allowed):

- **ambientCG** — https://ambientcg.com — CC0 PBR materials (albedo/normal/rough/AO).
- **Poly Haven** — https://polyhaven.com/textures — CC0, high quality.
- **cc0-textures / cc0textures** (now ambientCG) and **shareTextures** — CC0.
- **Kenney** — https://kenney.nl — CC0 game-asset packs (lower-res, stylized).

Watch out for **non-redistributable** sources that look free but aren't:

- **textures.com (CGTextures)** — free *downloads* but the license forbids
  redistributing the files in a repo. Do **not** commit these.
- **OpenGameArt** — mixed licenses per asset (CC0, CC-BY, GPL…). Check each one;
  CC-BY needs attribution, GPL is viral. Only commit CC0 (or keep attribution).

### Format notes for this engine
- The shader currently samples **albedo only** (normal/roughness/AO maps are a
  future PBR step — see `docs/RENDERING_ROADMAP.md`).
- Keep textures **power-of-two and seamless** (we use `GL_REPEAT`).
- **PNG** for crisp/lossless, **JPG** to shrink photos; 256–1024 px is plenty for
  a web build (mind the total preload size).
- Albedo is treated as colour; data maps (normal/rough) would be linear.
