#version 300 es

precision highp float;

// Per-vertex blade geometry: x in [-1,1] across the blade, y in [0,1] up it.
layout(location=0) in vec2 vBlade;
// Per-instance (divisor 1): base world position, then (windPhase, heightScale, yaw).
layout(location=1) in vec3 iPos;
layout(location=2) in vec3 iRand;

uniform mat4  projection;
uniform mat4  view;
uniform vec3  uCameraPos;
uniform float uTime;
uniform float uBladeWidth;
uniform float uBladeHeight;
uniform float uMaxDist;     // blades shrink to nothing past this distance

out float vH;     // height along the blade [0,1]
out vec3  vWorld; // world position (for fog)
out float vFade;  // distance fade [0,1]

void main()
{
    float t = vBlade.y;

    // Distance fade based on the camera's horizontal distance to the blade.
    vec3 flat = uCameraPos - iPos;
    flat.y = 0.0;
    float camDist = length(flat);
    float fade = clamp(1.0 - camDist / max(uMaxDist, 1.0), 0.0, 1.0);

    float h = uBladeHeight * iRand.y * fade; // far blades collapse to 0 height
    float w = uBladeWidth * mix(1.0, 0.12, t); // taper toward the tip

    // Billboard around the Y axis so the blade always faces the camera.
    vec3 toCam = camDist > 0.001 ? flat / camDist : vec3(0.0, 0.0, 1.0);
    vec3 right = vec3(toCam.z, 0.0, -toCam.x);

    // Wind: bend the upper part of the blade along a world direction.
    vec3 windDir = normalize(vec3(0.8, 0.0, 0.6));
    float wind = sin(uTime * 1.6 + iRand.x + dot(iPos.xz, vec2(0.12)))
               + 0.4 * sin(uTime * 3.1 + iRand.x);
    vec3 bend = windDir * (wind * t * t * h * 0.25);

    vec3 world = iPos + right * (vBlade.x * w) + vec3(0.0, t * h, 0.0) + bend;

    vWorld = world;
    vH = t;
    vFade = fade;
    gl_Position = projection * view * vec4(world, 1.0);
}
