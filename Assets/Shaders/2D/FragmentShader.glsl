#version 300 es

precision mediump float;
out vec4 fragColor;
in vec4 colorF;
in vec3 normalF;
in vec2 texCoordF;
flat in uint texIndexF;

uniform sampler2D tex0, tex1, tex2, tex3, tex4, tex5, tex6, tex7, tex8, tex9, tex10, tex11, tex12, tex13, tex14, tex15;

void main()
{
    // texIndex 0 => vertex colour only; 1..16 => modulate with the bound texture.
    vec4 base = colorF;
    int ti = int(texIndexF);
    if      (ti == 1)  base = colorF * texture(tex0,  texCoordF);
    else if (ti == 2)  base = colorF * texture(tex1,  texCoordF);
    else if (ti == 3)  base = colorF * texture(tex2,  texCoordF);
    else if (ti == 4)  base = colorF * texture(tex3,  texCoordF);
    else if (ti == 5)  base = colorF * texture(tex4,  texCoordF);
    else if (ti == 6)  base = colorF * texture(tex5,  texCoordF);
    else if (ti == 7)  base = colorF * texture(tex6,  texCoordF);
    else if (ti == 8)  base = colorF * texture(tex7,  texCoordF);
    else if (ti == 9)  base = colorF * texture(tex8,  texCoordF);
    else if (ti == 10) base = colorF * texture(tex9,  texCoordF);
    else if (ti == 11) base = colorF * texture(tex10, texCoordF);
    else if (ti == 12) base = colorF * texture(tex11, texCoordF);
    else if (ti == 13) base = colorF * texture(tex12, texCoordF);
    else if (ti == 14) base = colorF * texture(tex13, texCoordF);
    else if (ti == 15) base = colorF * texture(tex14, texCoordF);
    else if (ti == 16) base = colorF * texture(tex15, texCoordF);

    // Simple directional (sun) lighting using the interpolated normal, plus a
    // flat ambient term so shadowed faces never go fully black.
    vec3 N = normalize(normalF);
    vec3 L = normalize(vec3(0.45, 0.85, 0.35));
    float diffuse = max(dot(N, L), 0.0);
    float lighting = 0.30 + 0.70 * diffuse;

    fragColor = vec4(base.rgb * lighting, base.a);
}
