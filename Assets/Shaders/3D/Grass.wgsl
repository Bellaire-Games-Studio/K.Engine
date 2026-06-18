// -----------------------------------------------------------------------------
// Grass.wgsl - WGSL port of Assets/Shaders/3D/Grass{Vertex,Fragment}.glsl
//
//  WebGPU shader modules carry both stages, so the GLSL vert+frag pair becomes
//  one module with @vertex `vs_main` and @fragment `fs_main`. The uniform struct
//  mirrors the std140 `Constants` block the GL grass shaders use (and the C++
//  GrassConstants struct), so the same per-draw UBO data uploads unchanged.
//
//  Part of the experimental WebGPU backend scaffold (KE_BACKEND_WEBGPU). It is
//  not wired into a running pipeline yet - see docs/WEBGPU_PORT.md.
// -----------------------------------------------------------------------------

struct Constants {
    projection   : mat4x4<f32>,
    view         : mat4x4<f32>,
    cameraPos    : vec4<f32>,   // xyz
    sunDir       : vec4<f32>,   // xyz
    sunColor     : vec4<f32>,   // xyz
    ambientColor : vec4<f32>,   // xyz
    fogColor     : vec4<f32>,   // xyz
    params0      : vec4<f32>,   // time, bladeWidth, bladeHeight, maxDist
    params1      : vec4<f32>,   // ambientIntensity, sunIntensity, fogDensity, _
};

@group(0) @binding(0) var<uniform> C : Constants;

struct VSOut {
    @builtin(position) clip   : vec4<f32>,
    @location(0)       vH     : f32,
    @location(1)       vWorld : vec3<f32>,
    @location(2)       vFade  : f32,
    @location(3)       vNormal: vec3<f32>,
    @location(4)       vSeed  : f32,
};

fn hash11(p_in : f32) -> f32 {
    var p = fract(p_in * 0.1031);
    p = p * (p + 33.33);
    p = p * (p + p);
    return fract(p);
}

@vertex
fn vs_main(@location(0) vBlade : vec2<f32>,
           @location(1) iPos   : vec3<f32>,
           @location(2) iRand  : vec3<f32>) -> VSOut {
    let time        = C.params0.x;
    let bladeWidth  = C.params0.y;
    let bladeHeight = C.params0.z;
    let maxDist     = C.params0.w;

    let phase       = iRand.x;
    let heightScale = iRand.y;
    let yaw         = iRand.z;
    let t           = vBlade.y;

    var camDelta = C.cameraPos.xyz - iPos;
    camDelta.y = 0.0;
    let camDist = length(camDelta);
    var fade = clamp(1.0 - camDist / max(maxDist, 1.0), 0.0, 1.0);
    fade = fade * fade * (3.0 - 2.0 * fade);

    let side = vec3<f32>(cos(yaw), 0.0, sin(yaw));
    let fwd  = vec3<f32>(-sin(yaw), 0.0, cos(yaw));
    let up   = vec3<f32>(0.0, 1.0, 0.0);

    let hN = bladeHeight * heightScale;
    let h  = hN * fade;

    let gust    = sin(time * 1.5 + phase + dot(iPos.xz, vec2<f32>(0.15, 0.15)));
    let flutter = sin(time * 4.3 + phase * 1.7);
    let baseLean = (hash11(phase) - 0.5) * 0.5;
    let bendAngle = baseLean + gust * 0.30 + flutter * 0.06;

    let sweep = sin(bendAngle);
    let bend  = fwd * (sweep * h * t * t);
    let T     = normalize(up * hN + fwd * (sweep * hN * 2.0 * t));

    let w = bladeWidth * heightScale * sqrt(max(0.0, 1.0 - t));
    let world = iPos + side * (vBlade.x * w) + up * (t * h) + bend;

    var N = normalize(cross(T, side));
    N = normalize(N + side * (vBlade.x * 0.5));

    var o : VSOut;
    o.vWorld  = world;
    o.vH      = t;
    o.vFade   = fade;
    o.vNormal = N;
    o.vSeed   = hash11(phase * 1.7 + yaw);
    o.clip    = C.projection * C.view * vec4<f32>(world, 1.0);
    return o;
}

@fragment
fn fs_main(in : VSOut, @builtin(front_facing) frontFacing : bool) -> @location(0) vec4<f32> {
    if (in.vFade <= 0.002) {
        discard;
    }

    let ambientIntensity = C.params1.x;
    let sunIntensity     = C.params1.y;
    let fogDensity       = C.params1.z;

    let rootCol = vec3<f32>(0.05, 0.14, 0.04);
    let lush    = vec3<f32>(0.33, 0.52, 0.15);
    let dry     = vec3<f32>(0.55, 0.52, 0.22);
    let tipCol  = mix(lush, dry, in.vSeed * 0.6);
    let base    = mix(rootCol, tipCol, in.vH * in.vH);

    var N = normalize(in.vNormal);
    if (!frontFacing) {
        N = -N;
    }

    let L = normalize(-C.sunDir.xyz);
    let V = normalize(C.cameraPos.xyz - in.vWorld);
    let H = normalize(L + V);

    let nl   = max(dot(N, L), 0.0);
    let wrap = nl * 0.5 + 0.5;

    let skyAmb    = C.ambientColor.xyz;
    let groundAmb = C.ambientColor.xyz * 0.35 + vec3<f32>(0.04, 0.05, 0.02);
    let ambient   = mix(groundAmb, skyAmb, N.y * 0.5 + 0.5) * ambientIntensity;

    let backlit = pow(max(dot(-N, L), 0.0), 2.0);
    let trans   = C.sunColor.xyz * sunIntensity * backlit * in.vH * 0.6;

    let spec = pow(max(dot(N, H), 0.0), 24.0) * 0.15 * in.vH;

    let ao = mix(0.45, 1.0, in.vH);
    let lighting = ambient * ao + C.sunColor.xyz * sunIntensity * wrap * ao;

    var col = base * lighting + trans + C.sunColor.xyz * spec;

    if (fogDensity > 0.0) {
        let d = length(C.cameraPos.xyz - in.vWorld);
        let f = clamp(exp(-pow(d * fogDensity, 2.0)), 0.0, 1.0);
        col = mix(C.fogColor.xyz, col, f);
    }

    return vec4<f32>(col, 1.0);
}
