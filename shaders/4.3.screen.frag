#version 430 core

out vec4 FragColor;

//layout(rgba32f, binding=1) uniform readonly image2D screen;
uniform sampler2D screen;

uniform int screenWidth = 1;
uniform int screenHeight = 1;
uniform int imageWidth = 1;
uniform int imageHeight = 1;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    FragColor = texture(screen, uv);
}
