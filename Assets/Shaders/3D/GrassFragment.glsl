#version 300 es

precision highp float;

in float vH;
in vec3  vWorld;
in float vFade;

out vec4 fragColor;

uniform vec3  uCameraPos;
uniform vec3  uAmbientColor;
uniform float uAmbientIntensity;
uniform vec3  uSunDir;
uniform vec3  uSunColor;
uniform float uSunIntensity;
uniform vec3  uFogColor;
uniform float uFogDensity;

void main()
{
    if (vFade <= 0.01)
        discard; // fully faded-out blade

    vec3 rootCol = vec3(0.12, 0.22, 0.07);
    vec3 tipCol  = vec3(0.42, 0.55, 0.18);
    vec3 base = mix(rootCol, tipCol, vH);

    // Grass is lit mainly from the sky/sun; treat the normal as up.
    vec3 N = vec3(0.0, 1.0, 0.0);
    float sunDiff = max(dot(N, normalize(-uSunDir)), 0.0);
    vec3 lighting = uAmbientColor * uAmbientIntensity + uSunColor * uSunIntensity * sunDiff;

    // Fake ambient occlusion: darker toward the root.
    lighting *= mix(0.55, 1.0, vH);

    vec3 col = base * lighting;

    if (uFogDensity > 0.0)
    {
        float d = length(uCameraPos - vWorld);
        float f = clamp(exp(-pow(d * uFogDensity, 2.0)), 0.0, 1.0);
        col = mix(uFogColor, col, f);
    }

    fragColor = vec4(col, 1.0);
}
