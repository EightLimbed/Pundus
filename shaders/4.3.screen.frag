#version 430 core

out vec4 FragColor;

//layout(rgba32f, binding=1) uniform readonly image2D screen;
const vec3 colors[8] = {vec3(0.1,0.7,0.1), vec3(0.1,0.8,0.0), vec3(1.0,0.3,0.5), vec3(1.0,0.5,0.1), vec3(0.6,0.3,0.0), vec3(0.5,0.5,0.5), vec3(1.0), vec3(0.4,0.6,1.0)};

uniform sampler2D screen;

uniform int screenWidth = 1;
uniform int screenHeight = 1;
uniform float iTime;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(screenWidth, screenHeight);
    vec4 c = texture(screen, uv);
    FragColor = c;
}
