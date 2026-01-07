#version 430 core

out vec4 FragColor;

//layout(rgba32f, binding=1) uniform readonly image2D screen;
uniform sampler2D screen;

uniform int screenWidth = 800;
uniform int screenHeight = 600;
uniform int imageWidth = 800;
uniform int imageHeight = 600;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    FragColor = texture(screen, uv);
}
