#version 300 es

precision highp float; // highp keeps procedural noise stable at world-scale coords

out vec4 fragColor;

in vec4 colorF;
in vec3 normalF;
in vec2 texCoordF;
flat in uint texIndexF;
in vec3 fragPos;
in float vViewDepth;

uniform sampler2D tex0, tex1, tex2, tex3, tex4, tex5, tex6, tex7, tex8, tex9, tex10, tex11, tex12, tex13, tex14, tex15;

// 0 = lit vertex colour (+ optional texture), 1 = procedural terrain, 2 = unlit (2D/HUD)
uniform int uShadeMode;

uniform vec3 uCameraPos;

uniform vec3  uAmbientColor;
uniform float uAmbientIntensity;

uniform vec3  uSunDir;       // direction the sunlight travels
uniform vec3  uSunColor;
uniform float uSunIntensity;

const int MAX_POINT_LIGHTS = 16;
uniform int   uPointCount;
uniform vec3  uPointPos[MAX_POINT_LIGHTS];
uniform vec3  uPointColor[MAX_POINT_LIGHTS];
uniform float uPointIntensity[MAX_POINT_LIGHTS];
uniform float uPointRadius[MAX_POINT_LIGHTS];

uniform vec3  uFogColor;
uniform float uFogDensity;

// ---- PBR-style surface detail (derived from the albedo texture) -------------
//  No authored normal/height/AO maps yet: a heightfield is read from the albedo
//  luminance and used for parallax (displacement), a derivative-based normal
//  (bump) and a cavity AO term. uRoughness drives the specular lobe. All of this
//  is gated by QualitySettings (uParallaxSteps == 0 / uNormalStrength == 0 turn
//  the expensive / bump parts off), so it scales with the fidelity dial.
uniform int   uPbrEnabled;     // 0 = legacy lit look
uniform float uNormalStrength; // 0 = no bump
uniform int   uParallaxSteps;  // 0 = no displacement
uniform float uParallaxScale;  // displacement depth
uniform float uRoughness;      // 0 = glossy, 1 = matte
uniform float uAoStrength;     // derived cavity AO amount

// ---- Cascaded shadow maps (sun) --------------------------------------------
const int MAX_CASCADES = 4;
uniform int       uShadowCount;          // 0 = shadows disabled (no-op)
uniform mat4      uShadowVP[MAX_CASCADES];
uniform float     uShadowSplit[MAX_CASCADES]; // cascade far distances (view space)
uniform sampler2D uShadowAtlas;
uniform float     uShadowBias;
uniform vec2      uShadowTexel;          // 1 / atlas size (x already accounts for tiling)

// Returns sun visibility in [0,1] (1 = fully lit). Cascades are tiled left->right
// in one atlas texture; each occupies 1/uShadowCount of the U range.
float sampleShadow(vec3 worldPos, float viewDepth, vec3 N, vec3 L)
{
    if (uShadowCount <= 0)
        return 1.0;

    int c = uShadowCount - 1;
    for (int i = 0; i < MAX_CASCADES; ++i)
    {
        if (i >= uShadowCount) break;
        if (viewDepth < uShadowSplit[i]) { c = i; break; }
    }

    vec4 lp = uShadowVP[c] * vec4(worldPos, 1.0);
    vec3 proj = lp.xyz / lp.w;
    proj = proj * 0.5 + 0.5;
    if (proj.z > 1.0)
        return 1.0; // beyond the cascade's far plane: treat as lit

    float invN = 1.0 / float(uShadowCount);
    vec2 uv = vec2((float(c) + proj.x) * invN, proj.y);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
        return 1.0;

    float bias = max(uShadowBias * (1.0 - dot(N, L)), uShadowBias * 0.15);
    float current = proj.z - bias;

    float lit = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            float d = texture(uShadowAtlas, uv + vec2(float(x), float(y)) * uShadowTexel).r;
            lit += current <= d ? 1.0 : 0.0;
        }
    return lit / 9.0;
}

// ---- GPU value noise (for procedural terrain texturing) --------------------
float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float vnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm2(vec2 p)
{
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; ++i) { s += a * vnoise(p); p *= 2.0; a *= 0.5; }
    return s;
}

// Height + slope based material blend, broken up with noise detail.
vec3 proceduralTerrain(vec3 p, vec3 N)
{
    float slope  = clamp(1.0 - N.y, 0.0, 1.0);
    float macro  = fbm2(p.xz * 0.08);
    float fine   = vnoise(p.xz * 0.9);

    vec3 sand  = vec3(0.78, 0.72, 0.50);
    vec3 grass = mix(vec3(0.20, 0.40, 0.13), vec3(0.34, 0.50, 0.20), macro);
    vec3 rock  = mix(vec3(0.34, 0.30, 0.27), vec3(0.46, 0.42, 0.38), fine);
    vec3 snow  = vec3(0.93, 0.95, 0.98);

    float h = p.y + (macro - 0.5) * 8.0; // perturb band edges so they aren't flat lines

    vec3 col = sand;
    col = mix(col, grass, smoothstep(2.0, 6.0, h));
    col = mix(col, rock,  smoothstep(38.0, 55.0, h));
    col = mix(col, snow,  smoothstep(64.0, 80.0, h));
    col = mix(col, rock,  smoothstep(0.30, 0.55, slope)); // steep faces show rock

    col *= 0.85 + 0.3 * macro; // subtle large-scale variation
    return col;
}

// Albedo sample for a given (1-based) texture index at an explicit UV.
vec4 sampleAlbedo(int ti, vec2 uv)
{
    if      (ti == 1)  return texture(tex0,  uv);
    else if (ti == 2)  return texture(tex1,  uv);
    else if (ti == 3)  return texture(tex2,  uv);
    else if (ti == 4)  return texture(tex3,  uv);
    else if (ti == 5)  return texture(tex4,  uv);
    else if (ti == 6)  return texture(tex5,  uv);
    else if (ti == 7)  return texture(tex6,  uv);
    else if (ti == 8)  return texture(tex7,  uv);
    else if (ti == 9)  return texture(tex8,  uv);
    else if (ti == 10) return texture(tex9,  uv);
    else if (ti == 11) return texture(tex10, uv);
    else if (ti == 12) return texture(tex11, uv);
    else if (ti == 13) return texture(tex12, uv);
    else if (ti == 14) return texture(tex13, uv);
    else if (ti == 15) return texture(tex14, uv);
    else if (ti == 16) return texture(tex15, uv);
    return vec4(1.0);
}

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

// Derived heightfield: brighter albedo == raised, darker (mortar, cracks) == recessed.
float heightAt(int ti, vec2 uv) { return luma(sampleAlbedo(ti, uv).rgb); }

// Parallax occlusion mapping: walk the view ray through the heightfield (in
// tangent space) and return the displaced UV. viewT is the view dir in tangent
// space (surface -> eye). Bounded loop so it stays uniform-friendly.
vec2 parallaxUV(int ti, vec2 uv, vec3 viewT)
{
    if (uParallaxSteps <= 0)
        return uv;

    const int MAX_STEPS = 32;
    float numLayers = float(uParallaxSteps);
    float layerDepth = 1.0 / numLayers;
    vec2  maxOffset = (viewT.xy / max(abs(viewT.z), 0.3)) * uParallaxScale;
    vec2  deltaUV = maxOffset / numLayers;

    float curDepth = 0.0;
    vec2  curUV = uv;
    float curH = 1.0 - heightAt(ti, curUV); // depth = 1 - height
    for (int i = 0; i < MAX_STEPS; ++i)
    {
        if (i >= uParallaxSteps || curDepth >= curH)
            break;
        curUV -= deltaUV;
        curH = 1.0 - heightAt(ti, curUV);
        curDepth += layerDepth;
    }

    // Interpolate between the last two layers for a smooth intersection.
    vec2  prevUV = curUV + deltaUV;
    float afterD = curH - curDepth;
    float beforeD = (1.0 - heightAt(ti, prevUV)) - (curDepth - layerDepth);
    float w = afterD / (afterD - beforeD + 1e-5);
    return mix(curUV, prevUV, clamp(w, 0.0, 1.0));
}

// Perturb a geometric normal by the screen-space gradient of the heightfield,
// with no precomputed tangents (Mikkelsen's surface-gradient bump mapping).
vec3 perturbNormal(vec3 N, vec3 p, float h, float strength)
{
    vec3 dpx = dFdx(p);
    vec3 dpy = dFdy(p);
    float dhx = dFdx(h);
    float dhy = dFdy(h);
    vec3 r1 = cross(dpy, N);
    vec3 r2 = cross(N, dpx);
    float det = dot(dpx, r1);
    if (abs(det) < 1e-7)
        return N; // degenerate (tiny/edge-on triangle): keep the geometric normal
    vec3 grad = sign(det) * (dhx * r1 + dhy * r2);
    return normalize(abs(det) * N - strength * grad);
}

// Cotangent frame (T, B, N) from screen-space derivatives, for tangent-space math.
mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv)
{
    vec3 dp1 = dFdx(p), dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv), duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    // epsilon keeps invmax finite when the UVs don't vary (avoids NaN T/B).
    float invmax = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-8));
    return mat3(T * invmax, B * invmax, N);
}

// Roughness -> Blinn-Phong specular lobe (sharp+bright when smooth, broad+dim
// when matte). Keeps the look continuous with the old fixed pow(...,32)*0.2.
float specular(vec3 N, vec3 H, float roughness)
{
    if (uPbrEnabled == 0)
        return pow(max(dot(N, H), 0.0), 32.0) * 0.2; // exact legacy lobe
    float gloss = mix(96.0, 6.0, roughness);
    float scale = mix(0.45, 0.04, roughness);
    return pow(max(dot(N, H), 0.0), gloss) * scale;
}

void main()
{
    vec3 N = normalize(normalF);
    int  ti = int(texIndexF);
    vec2 uv = texCoordF;

    // Derived surface detail only applies to textured, lit surfaces.
    bool usePbr = (uPbrEnabled != 0) && (uShadeMode == 0) && (ti > 0);
    float roughness = clamp(uRoughness, 0.04, 1.0);
    float ao = 1.0;

    if (usePbr)
    {
        mat3 TBN = cotangentFrame(N, fragPos, uv);
        vec3 V0 = normalize(uCameraPos - fragPos);
        vec3 viewT = normalize(V0 * TBN); // world->tangent (TBN orthonormalish)
        uv = parallaxUV(ti, uv, viewT);   // displacement

        float h = heightAt(ti, uv);
        N = perturbNormal(N, fragPos, h, uNormalStrength); // bump
        ao = 1.0 - uAoStrength * (1.0 - h);                // cavity AO
    }

    vec4 base;
    if (uShadeMode == 1)
        base = vec4(proceduralTerrain(fragPos, N), 1.0);
    else
        base = colorF * sampleAlbedo(ti, uv);

    if (uShadeMode == 2) // unlit (2D / HUD)
    {
        fragColor = base;
        return;
    }

    vec3 V = normalize(uCameraPos - fragPos);
    vec3 lighting = uAmbientColor * uAmbientIntensity * ao;

    // Directional (sun) light + specular, attenuated by the shadow map.
    vec3 Ld = normalize(-uSunDir);
    float sunDiff = max(dot(N, Ld), 0.0);
    float shadow = sampleShadow(fragPos, vViewDepth, N, Ld);
    lighting += uSunColor * uSunIntensity * sunDiff * shadow;
    if (sunDiff > 0.0)
    {
        vec3 H = normalize(Ld + V);
        lighting += uSunColor * uSunIntensity * specular(N, H, roughness) * shadow;
    }

    // Point lights (already culled to the nearest few on the CPU).
    for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
    {
        if (i >= uPointCount)
            break;

        vec3 toL = uPointPos[i] - fragPos;
        float dist = length(toL);
        vec3 L = toL / max(dist, 0.0001);

        float att = clamp(1.0 - dist / max(uPointRadius[i], 0.0001), 0.0, 1.0);
        att *= att;

        float diff = max(dot(N, L), 0.0);
        lighting += uPointColor[i] * uPointIntensity[i] * diff * att;

        vec3 H = normalize(L + V);
        lighting += uPointColor[i] * uPointIntensity[i] * specular(N, H, roughness) * att;
    }

    vec3 colorOut = base.rgb * lighting;

    if (uFogDensity > 0.0)
    {
        float distCam = length(uCameraPos - fragPos);
        float f = clamp(exp(-pow(distCam * uFogDensity, 2.0)), 0.0, 1.0);
        colorOut = mix(uFogColor, colorOut, f);
    }

    fragColor = vec4(colorOut, base.a);
}
