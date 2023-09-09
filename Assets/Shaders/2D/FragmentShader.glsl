#version 300 es

precision mediump float;
out vec4 fragColor;
in vec4 colorF;
in vec3 normalF;
in vec2 texCoordF;
flat in uint texIndexF;

uniform sampler2D tex0, tex1, tex2, tex3, tex4, tex5, tex6, tex7, tex8, tex9, tex10, tex11, tex12, tex13, tex14, tex15;
//uniform sampler2D tex15, tex16, tex17, tex18, tex19, tex20, tex21, tex22, tex23, tex24, tex25, tex26, tex27, tex28, tex29;
//uniform sampler2D tex30, tex31;


void main()
{
    float ambient = 0.1;
    
    switch (int(texIndexF))
    {
        case 0: fragColor = colorF;
        case 1: fragColor = colorF * texture(tex0, texCoordF); break;
        case 2: fragColor = colorF * texture(tex1, texCoordF); break;
        case 3: fragColor = colorF * texture(tex2, texCoordF); break;
        case 4: fragColor = colorF * texture(tex3, texCoordF); break;
        case 5: fragColor = colorF * texture(tex4, texCoordF); break;
        case 6: fragColor = colorF * texture(tex5, texCoordF); break;
        case 7: fragColor = colorF * texture(tex6, texCoordF); break;
        case 8: fragColor = colorF * texture(tex7, texCoordF); break;
        case 9: fragColor = colorF * texture(tex8, texCoordF); break;
        case 10: fragColor = colorF * texture(tex9, texCoordF); break;
        case 11: fragColor = colorF * texture(tex10, texCoordF); break;
        case 12: fragColor = colorF * texture(tex11, texCoordF); break;
        case 13: fragColor = colorF * texture(tex12, texCoordF); break;
        case 14: fragColor = colorF * texture(tex13, texCoordF); break;
        case 15: fragColor = colorF * texture(tex14, texCoordF); break;
        case 16: fragColor = colorF * texture(tex15, texCoordF); break;
        
    /*
        
        case 18: fragColor = colorF * texture(tex17, texCoordF); break;
        case 19: fragColor = colorF * texture(tex18, texCoordF); break;
        case 20: fragColor = colorF * texture(tex19, texCoordF); break;
        case 21: fragColor = colorF * texture(tex20, texCoordF); break;
        case 22: fragColor = colorF * texture(tex21, texCoordF); break;
        case 23: fragColor = colorF * texture(tex22, texCoordF); break;
        case 24: fragColor = colorF * texture(tex23, texCoordF); break;
        case 25: fragColor = colorF * texture(tex24, texCoordF); break;
        case 26: fragColor = colorF * texture(tex25, texCoordF); break;
        case 27: fragColor = colorF * texture(tex26, texCoordF); break;
        case 28: fragColor = colorF * texture(tex27, texCoordF); break;
        case 29: fragColor = colorF * texture(tex28, texCoordF); break;
        case 30: fragColor = colorF * texture(tex29, texCoordF); break;
        case 31: fragColor = colorF * texture(tex30, texCoordF); break;
        case 32: fragColor = colorF * texture(tex31, texCoordF); break;
    */
        
    }

}