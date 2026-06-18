#version 300 es

precision highp float;

// Per-vertex blade geometry: x in [-1,1] across the blade, y in [0,1] up it.
// The blade is now a multi-segment strip (several rows of y) so it can curve
// along its length instead of being a single flat billboard quad.
layout(location=0) in vec2 vBlade;
// Per-instance (divisor 1): base world position, then (windPhase, heightScale, yaw).
layout(location=1) in vec3 iPos;
layout(location=2) in vec3 iRand;

// Per-draw constants (RHI uniform buffer, std140) - shared with the fragment stage.
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

out float vH;      // height along the blade [0,1]
out vec3  vWorld;  // world position (for fog + specular)
out float vFade;   // distance fade [0,1]
out vec3  vNormal; // per-vertex blade normal (curvature + rounding)
out float vSeed;   // per-blade random [0,1] for hue variation

// Cheap hash -> [0,1) so each blade gets stable per-blade variation.
float hash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

void main()
{
    float time        = params0.x;
    float bladeWidth  = params0.y;
    float bladeHeight = params0.z;
    float maxDist     = params0.w;

    float phase       = iRand.x;
    float heightScale = iRand.y;
    float yaw         = iRand.z;
    float t           = vBlade.y;

    // Distance fade based on the camera's horizontal distance to the blade.
    vec3 camDelta = cameraPos.xyz - iPos;
    camDelta.y = 0.0;
    float camDist = length(camDelta);
    float fade = clamp(1.0 - camDist / max(maxDist, 1.0), 0.0, 1.0);
    fade = fade * fade * (3.0 - 2.0 * fade); // smoothstep ease so it doesn't pop

    // Per-blade orientation. Each blade faces its own yaw instead of always
    // billboarding flat at the camera, which is what made the old grass read as
    // cardboard. 'side' is the width axis, 'fwd' the bending axis.
    vec3 side = vec3(cos(yaw), 0.0, sin(yaw));
    vec3 fwd  = vec3(-sin(yaw), 0.0, cos(yaw));
    vec3 up   = vec3(0.0, 1.0, 0.0);

    // Unfaded height is used for the tangent/normal so far blades (height -> 0)
    // never produce a zero-length tangent (NaN normals).
    float hN = bladeHeight * heightScale;
    float h  = hN * fade;

    // Wind: a low-frequency directional gust plus a high-frequency flutter,
    // phase-shifted per blade and by world position so the field ripples.
    float gust    = sin(time * 1.5 + phase + dot(iPos.xz, vec2(0.15)));
    float flutter = sin(time * 4.3 + phase * 1.7);
    float baseLean = (hash11(phase) - 0.5) * 0.5;     // resting curve, per blade
    float bendAngle = baseLean + gust * 0.30 + flutter * 0.06;

    // Curve the blade forward; displacement grows with t^2 so the base stays put
    // and the tip sweeps. Tangent is the analytic derivative of that curve.
    float sweep = sin(bendAngle);
    vec3  bend  = fwd * (sweep * h * t * t);
    vec3  T     = normalize(up * hN + fwd * (sweep * hN * 2.0 * t));

    // Width tapers to a rounded point; sqrt keeps it full near the base.
    float w = bladeWidth * heightScale * sqrt(max(0.0, 1.0 - t));

    vec3 world = iPos + side * (vBlade.x * w) + up * (t * h) + bend;

    // Face normal from the curve tangent, then tilt across the width so the
    // blade reads as a rounded tube rather than a flat sheet.
    vec3 N = normalize(cross(T, side));
    N = normalize(N + side * (vBlade.x * 0.5));

    vWorld  = world;
    vH      = t;
    vFade   = fade;
    vNormal = N;
    vSeed   = hash11(phase * 1.7 + yaw);
    gl_Position = projection * view * vec4(world, 1.0);
}
