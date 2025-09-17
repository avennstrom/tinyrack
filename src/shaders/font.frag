#version 300 es
precision mediump float;

uniform sampler2D uFont;

in vec4 vColor;
in vec2 vTexcoord;

out vec4 fragColor;

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec2 uv = (vTexcoord + 0.5) * (1.0 / 208.0);
    vec3 msdf = texture(uFont, uv).rgb;
    float sd = median(msdf.r, msdf.g, msdf.b);
    float w = fwidth(sd);
    float alpha = smoothstep(0.5 - w, 0.5 + w, sd);
    fragColor = vec4(vColor.rgb, vColor.a * alpha);
}