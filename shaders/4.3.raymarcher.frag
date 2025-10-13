#version 430 core

layout(std430, binding = 0) buffer VoxelData {
    uint bitCloud[];
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

// time
uniform float iTime;

// constants


// morton encoding/decoding
uint part1by2(uint x) {
    x &= 0x000003FFu;
    x = (x | (x << 16)) & 0x030000FFu;
    x = (x | (x << 8))  & 0x0300F00Fu;
    x = (x | (x << 4))  & 0x030C30C3u;
    x = (x | (x << 2))  & 0x09249249u;
    return x;
}

uint morton3D(uint x, uint y, uint z) {
    return part1by2(x) | (part1by2(y) << 1) | (part1by2(z) << 2);
}

uint compact1by2(uint x) {
    x &= 0x09249249u; // 10 bits
    x = (x ^ (x >> 2)) & 0x030C30C3u;
    x = (x ^ (x >> 4)) & 0x0300F00Fu;
    x = (x ^ (x >> 8)) & 0x030000FFu;
    x = (x ^ (x >> 16)) & 0x000003FFu;
    return x;
}

uvec3 mortonDecode3D(uint m) {
    uint x = compact1by2(m);
    uint y = compact1by2(m >> 1);
    uint z = compact1by2(m >> 2);
    return uvec3(x, y, z);
}

// checks if voxel is solid
bool checkVoxel(uint m) {
    uint idx = m >> 5;           // which 32-bit word (divide by 32)
    uint bit = m & 31u;          // which bit in that word (mod 32)
    return ((bitCloud[idx] >> bit) & 1u) != 0u;
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
    vec3 rd = getRayDir(gl_FragCoord.xy, vec2(800,600), lookAt, 1.0);
    
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

    vec3 tMax = bound * dr;  // how far to first boundary per axis
    vec3 tDelta = dr;    
    float t = 0.0;

    for (int i = 0; i < 1024; i++) {
        uint m = morton3D(uint(vp.x),uint(vp.y),uint(vp.z));

        if (checkVoxel(m)) {
            FragColor = vec4(vec3(0.5,0.5,0.5)-length(vp-floor(ro))/100.0,1.0);
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