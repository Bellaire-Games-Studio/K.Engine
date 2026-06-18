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

vec4 sampleBase()
{
    int ti = int(texIndexF);
    if      (ti == 1)  return colorF * texture(tex0,  texCoordF);
    else if (ti == 2)  return colorF * texture(tex1,  texCoordF);
    else if (ti == 3)  return colorF * texture(tex2,  texCoordF);
    else if (ti == 4)  return colorF * texture(tex3,  texCoordF);
    else if (ti == 5)  return colorF * texture(tex4,  texCoordF);
    else if (ti == 6)  return colorF * texture(tex5,  texCoordF);
    else if (ti == 7)  return colorF * texture(tex6,  texCoordF);
    else if (ti == 8)  return colorF * texture(tex7,  texCoordF);
    else if (ti == 9)  return colorF * texture(tex8,  texCoordF);
    else if (ti == 10) return colorF * texture(tex9,  texCoordF);
    else if (ti == 11) return colorF * texture(tex10, texCoordF);
    else if (ti == 12) return colorF * texture(tex11, texCoordF);
    else if (ti == 13) return colorF * texture(tex12, texCoordF);
    else if (ti == 14) return colorF * texture(tex13, texCoordF);
    else if (ti == 15) return colorF * texture(tex14, texCoordF);
    else if (ti == 16) return colorF * texture(tex15, texCoordF);
    return colorF;
}

void main()
{
    vec3 N = normalize(normalF);

    vec4 base;
    if (uShadeMode == 1)
        base = vec4(proceduralTerrain(fragPos, N), 1.0);
    else
        base = sampleBase();

    if (uShadeMode == 2) // unlit (2D / HUD)
    {
        fragColor = base;
        return;
    }

    vec3 V = normalize(uCameraPos - fragPos);
    vec3 lighting = uAmbientColor * uAmbientIntensity;

    // Directional (sun) light + specular, attenuated by the shadow map.
    vec3 Ld = normalize(-uSunDir);
    float sunDiff = max(dot(N, Ld), 0.0);
    float shadow = sampleShadow(fragPos, vViewDepth, N, Ld);
    lighting += uSunColor * uSunIntensity * sunDiff * shadow;
    if (sunDiff > 0.0)
    {
        vec3 H = normalize(Ld + V);
        lighting += uSunColor * uSunIntensity * pow(max(dot(N, H), 0.0), 32.0) * 0.2 * shadow;
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
        lighting += uPointColor[i] * uPointIntensity[i] * pow(max(dot(N, H), 0.0), 32.0) * att * 0.2;
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
