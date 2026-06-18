#version 300 es

layout(location=0) in vec3 position;
layout(location=1) in vec4 color;
layout(location=2) in vec3 normal;
layout(location=3) in vec2 texCoord;
layout(location=4) in uint texIndex;

uniform mat4 projection;
uniform mat4 view;

out vec4 colorF;
out vec3 normalF;
out vec2 texCoordF;
flat out uint texIndexF;
out vec3 fragPos; // world-space position (geometry is pre-transformed on the CPU)

void main()
{
    colorF = color;
    normalF = normal;
    texCoordF = texCoord;
    texIndexF = texIndex;
    fragPos = position;
    gl_Position = projection * view * vec4(position, 1.0);
}
