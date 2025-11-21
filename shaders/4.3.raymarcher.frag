#version 430 core

layout(std430, binding = 0) buffer BlockData {
    uint chunkMask[65536];
    uint blockData[];
};

out vec4 FragColor;


// player position
uniform float pPosX;
uniform float pPosY;
uniform float pPosZ;

// player direction
uniform float pDirX;
uniform float pDirY;
uniform float pDirZ;

// screen
uniform int screenWidth = 800;
uniform int screenHeight = 600;

// time
uniform float iTime;

// constants
vec3 colors[3] = {vec3(0.1,0.7,0.1), vec3(0.6,0.3,0.0), vec3(0.5,0.5,0.5)};

uint getData(uint m) {
    uint i = m >> 2u; // divide by 4
    uint byteShift = (m & 3u) * 8u; // which byte in that uint
    uint term = blockData[i];
    return (term >> byteShift) & 0xFFu;
}

// morton encoding/decoding
uint part1by2(uint x) {
    x &= 0x000003FFu;
    x = (x | (x << 16)) & 0x030000FFu;
    x = (x | (x << 8))  & 0x0300F00Fu;
    x = (x | (x << 4))  & 0x030C30C3u;
    x = (x | (x << 2))  & 0x09249249u;
    return x;
}

uint morton3D(uvec3 p) {
    return part1by2(p.x) | (part1by2(p.y) << 1) | (part1by2(p.z) << 2);
}

// camera shizzle
vec3 getRayDir(vec2 fragCoord, vec2 res, vec3 lookAt, float zoom) {
    vec2 uv = (fragCoord - 0.5 * res) / res.y;
    vec3 f = normalize(lookAt);
    vec3 r = normalize(cross(vec3(0.0,1.0,0.0), f));
    vec3 u = cross(f,r);
    return normalize(f + zoom * (uv.x*r + uv.y*u));
}

// main raymarching loop
void main() {
    // camera setup
    FragColor = vec4(0.0);
    vec3 ro = vec3(pPosX,pPosY,pPosZ);
    vec3 lookAt = vec3(pDirX, pDirY, pDirZ);
    vec3 rd = getRayDir(gl_FragCoord.xy, vec2(screenWidth,screenHeight), lookAt, 1.0);
    
    // voxel space setup
    vec3 stride = sign(rd);
    ivec3 istride = ivec3(stride);
    ivec3 vp = ivec3(floor(ro)); //starting position
    // inverse of rd
    vec3 dr = 1.0 / max(abs(rd), vec3(1e-6));

    // distance to first voxel boundary
    vec3 bound;
    bound.x = (rd.x > 0.0) ? (float(vp.x) + 1.0 - ro.x) : (ro.x - float(vp.x));
    bound.y = (rd.y > 0.0) ? (float(vp.y) + 1.0 - ro.y) : (ro.y - float(vp.y));
    bound.z = (rd.z > 0.0) ? (float(vp.z) + 1.0 - ro.z) : (ro.z - float(vp.z));

    vec3 tMax = bound * dr; // how far to first boundary per axis
    vec3 tDelta = dr;    
    float t = 0.0;

    for (int i = 0; i < 512; i++) {
        uint m = morton3D(vp);
        uint data = getData(m);

        if (data > 0) { // temporary bounds, will increase
            vec3 c = colors[data-1];
            FragColor = vec4(c-length(ro-vp)*0.001,1.0);
            return;
        }

		if (tMax.x < tMax.y && tMax.x < tMax.z) { // X is closest
			vp.x += istride.x;
            tMax.x += tDelta.x;
		} else if (tMax.y < tMax.z) {             // Y is closest
			vp.y += istride.y;
            tMax.y += tDelta.y;
		} else {                                  // Z is closest
			vp.z += istride.z;
            tMax.z += tDelta.z;
		}
	}
}