#version 430 core

out vec4 FragColor;

//layout(rgba32f, binding=1) uniform readonly image2D screen;

uniform sampler2D screen;
uniform sampler2D bloom;

uniform int screenWidth = 1;
uniform int screenHeight = 1;
uniform float iTime;

const int kernelRadius = 10;

void main() {
    vec2 ezis = vec2(1.0) / vec2(screenWidth, screenHeight);
    vec2 uv = gl_FragCoord.xy * ezis;
    vec4 c = texture(screen, uv);

    vec4 b = texture(bloom, uv);
    float avgMod = 0.0;
    for (int x = -kernelRadius; x < kernelRadius; x++) {
    for (int y = -kernelRadius; y < kernelRadius; y++) {
        if (dot(vec2(x,y),vec2(x,y)) < kernelRadius*kernelRadius) continue;
        b += texture(bloom,uv+ezis*vec2(x,y));
        avgMod += 1.0;
    }
    }
    b/=avgMod;
    
    FragColor = c + vec4(b.xyz*b.w,1.0);
}
