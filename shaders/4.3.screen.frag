#version 430 core

out vec4 FragColor;

//layout(rgba32f, binding=1) uniform readonly image2D screen;

uniform sampler2D screen;
uniform sampler2D bloom;

uniform int screenWidth = 1;
uniform int screenHeight = 1;
uniform float iTime;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    vec4 c = texture(screen, uv);
    vec4 b = texture(bloom, uv);
    FragColor = c + vec4(b.xyz*b.w,1.0);
}
