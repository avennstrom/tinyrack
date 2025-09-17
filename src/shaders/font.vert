#version 300 es

uniform mat4 uView;
uniform mat4 uProjection;

layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;
layout(location=2) in uint aTexcoord;

out vec4 vColor;
out vec2 vTexcoord;

void main()
{
    gl_Position = uProjection * uView * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
    uint u = aTexcoord & 0xffffu;
    uint v = aTexcoord >> 16u;
    vTexcoord = vec2(u, v);
}