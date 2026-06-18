#version 300 es

precision highp float;

in float vH;
in vec3  vWorld;
in float vFade;
in vec3  vNormal;
in float vSeed;

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
    if (vFade <= 0.002)
        discard; // fully faded-out blade (rare: only the outermost ring)

    float ambientIntensity = params1.x;
    float sunIntensity     = params1.y;
    float fogDensity       = params1.z;

    // Per-blade colour variation: drift between a lush green and a drier, more
    // yellow blade so the field isn't a single flat hue.
    vec3 rootCol = vec3(0.05, 0.14, 0.04);
    vec3 lush    = vec3(0.33, 0.52, 0.15);
    vec3 dry     = vec3(0.55, 0.52, 0.22);
    vec3 tipCol  = mix(lush, dry, vSeed * 0.6);
    vec3 base    = mix(rootCol, tipCol, vH * vH); // darker, AO-like toward the root

    // Two-sided lighting: flip the normal for back faces (cull is off).
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing)
        N = -N;

    vec3 L = normalize(-sunDir.xyz);
    vec3 V = normalize(cameraPos.xyz - vWorld);
    vec3 H = normalize(L + V);

    // Half-Lambert keeps the shaded side soft instead of black.
    float nl   = max(dot(N, L), 0.0);
    float wrap = nl * 0.5 + 0.5;

    // Hemisphere ambient: sky tint from above, a darker ground bounce from below.
    vec3 skyAmb    = ambientColor.xyz;
    vec3 groundAmb = ambientColor.xyz * 0.35 + vec3(0.04, 0.05, 0.02);
    vec3 ambient   = mix(groundAmb, skyAmb, N.y * 0.5 + 0.5) * ambientIntensity;

    // Fake subsurface scattering: thin blades glow where the sun is behind them,
    // strongest toward the translucent tip.
    float backlit = pow(max(dot(-N, L), 0.0), 2.0);
    vec3  trans   = sunColor.xyz * sunIntensity * backlit * vH * 0.6;

    // Soft sheen so tips catch the sun.
    float spec = pow(max(dot(N, H), 0.0), 24.0) * 0.15 * vH;

    float ao = mix(0.45, 1.0, vH); // contact darkening near the ground
    vec3 lighting = ambient * ao + sunColor.xyz * sunIntensity * wrap * ao;

    vec3 col = base * lighting + trans + sunColor.xyz * spec;

    if (fogDensity > 0.0)
    {
        float d = length(cameraPos.xyz - vWorld);
        float f = clamp(exp(-pow(d * fogDensity, 2.0)), 0.0, 1.0);
        col = mix(fogColor.xyz, col, f);
    }

    fragColor = vec4(col, 1.0);
}
