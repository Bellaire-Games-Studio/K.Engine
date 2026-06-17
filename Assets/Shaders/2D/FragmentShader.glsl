#version 300 es

precision mediump float;

out vec4 fragColor;

in vec4 colorF;
in vec3 normalF;
in vec2 texCoordF;
flat in uint texIndexF;
in vec3 fragPos;

uniform sampler2D tex0, tex1, tex2, tex3, tex4, tex5, tex6, tex7, tex8, tex9, tex10, tex11, tex12, tex13, tex14, tex15;

// 1 => skip lighting (used for 2D / HUD rendering).
uniform int uUnlit;

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
uniform float uFogDensity;   // 0 disables fog

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
    vec4 base = sampleBase();

    if (uUnlit == 1)
    {
        fragColor = base;
        return;
    }

    vec3 N = normalize(normalF);
    vec3 V = normalize(uCameraPos - fragPos);

    vec3 lighting = uAmbientColor * uAmbientIntensity;

    // Directional (sun) light + specular highlight.
    vec3 Ld = normalize(-uSunDir);
    float sunDiff = max(dot(N, Ld), 0.0);
    lighting += uSunColor * uSunIntensity * sunDiff;
    if (sunDiff > 0.0)
    {
        vec3 H = normalize(Ld + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0);
        lighting += uSunColor * uSunIntensity * spec * 0.2;
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
        att *= att; // smoother falloff

        float diff = max(dot(N, L), 0.0);
        lighting += uPointColor[i] * uPointIntensity[i] * diff * att;

        vec3 H = normalize(L + V);
        float spec = pow(max(dot(N, H), 0.0), 32.0);
        lighting += uPointColor[i] * uPointIntensity[i] * spec * att * 0.2;
    }

    vec3 colorOut = base.rgb * lighting;

    // Exponential-squared distance fog.
    if (uFogDensity > 0.0)
    {
        float distCam = length(uCameraPos - fragPos);
        float f = clamp(exp(-pow(distCam * uFogDensity, 2.0)), 0.0, 1.0);
        colorOut = mix(uFogColor, colorOut, f);
    }

    fragColor = vec4(colorOut, base.a);
}
