#version 300 es

precision highp float;

in float vH;
in vec3  vWorld;
in float vFade;

out vec4 fragColor;

// Per-draw constants (RHI uniform buffer, std140) - shared with the vertex stage.
layout(std140) uniform Constants
{
    mat4 projection;
    mat4 view;
    vec4 cameraPos;    // xyz
    vec4 sunDir;       // xyz
    vec4 sunColor;     // xyz
    vec4 ambientColor; // xyz
    vec4 fogColor;     // xyz
    vec4 params0;      // time, bladeWidth, bladeHeight, maxDist
    vec4 params1;      // ambientIntensity, sunIntensity, fogDensity, _
};

void main()
{
    if (vFade <= 0.01)
        discard; // fully faded-out blade

    float ambientIntensity = params1.x;
    float sunIntensity     = params1.y;
    float fogDensity       = params1.z;

    vec3 rootCol = vec3(0.12, 0.22, 0.07);
    vec3 tipCol  = vec3(0.42, 0.55, 0.18);
    vec3 base = mix(rootCol, tipCol, vH);

    // Grass is lit mainly from the sky/sun; treat the normal as up.
    vec3 N = vec3(0.0, 1.0, 0.0);
    float sunDiff = max(dot(N, normalize(-sunDir.xyz)), 0.0);
    vec3 lighting = ambientColor.xyz * ambientIntensity + sunColor.xyz * sunIntensity * sunDiff;

    // Fake ambient occlusion: darker toward the root.
    lighting *= mix(0.55, 1.0, vH);

    vec3 col = base * lighting;

    if (fogDensity > 0.0)
    {
        float d = length(cameraPos.xyz - vWorld);
        float f = clamp(exp(-pow(d * fogDensity, 2.0)), 0.0, 1.0);
        col = mix(fogColor.xyz, col, f);
    }

    fragColor = vec4(col, 1.0);
}
