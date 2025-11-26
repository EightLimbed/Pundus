#version 430 core

layout(std430, binding = 0) buffer BlockData {
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
const int passRes = 4;
const float renderDist = 1024;

// block data getter
uint getData(uint m) {
    uint idx = m >> 2u; // divide by 4
    uint bit = (m & 3u) * 8u; // which byte in that uint
    return (blockData[idx] >> bit) & 0xFFu;
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

bool posWithin(vec3 p, vec3 mini, vec3 maxi) {
    return p.x > 0 && p.y > 0 && p.z > 0 && p.x < 1024 && p.y < 1024 && p.z < 1024;
}

// main raymarching loop.
void main() {
    FragColor = vec4(-10e22, vec2(0.0), 1.0);
    
    // early out for rays that arent part of prepass
    if (int(gl_FragCoord.x) % passRes != 0) return; // if not multiple of whatever, skip
    if (int(gl_FragCoord.y) % passRes != 0) return;
    FragColor = vec4(10e22, vec2(0.0), 1.0);
    return;
    // camera setup.
    vec3 ro = vec3(pPosX,pPosY,pPosZ);
    vec3 lookAt = vec3(pDirX, pDirY, pDirZ);
    vec3 rd = getRayDir(gl_FragCoord.xy, vec2(screenWidth,screenHeight), lookAt, 1.0);
    
    // voxel space setup.
    ivec3 stride = ivec3(sign(rd))*passRes;
    // inverse of rd, made to be non 0.
    vec3 dr = 1.0 / max(abs(rd), vec3(1e-6));

    ivec3 vp = ivec3(floor(ro)); //starting position.
    // distance to first voxel boundary.
    vec3 bound;
    bound.x = (rd.x > 0.0) ? (float(vp.x) + 1.0 - ro.x) : (ro.x - float(vp.x));
    bound.y = (rd.y > 0.0) ? (float(vp.y) + 1.0 - ro.y) : (ro.y - float(vp.y));
    bound.z = (rd.z > 0.0) ? (float(vp.z) + 1.0 - ro.z) : (ro.z - float(vp.z));

    vec3 tMax = bound * dr; // how far to first voxel boundary per axis.

    for (int i = 0; i < renderDist/passRes; i++) {
        
        //float d = length(vp-ro);
        //if (d*d>renderDist*renderDist) return; // early out with distance.

        uint m = morton3D(vp);
        uint data = getData(m);
        if (data > 0u) {
            FragColor = vec4(length(ro-vp), vec2(0.0), 1.0);
            return;
        }

        // voxel-level DDA, steps chunk distance when chunk is empty.

		if (tMax.x <= tMax.y && tMax.x <= tMax.z) { // X is closest
			vp.x += stride.x;
            tMax.x += dr.x;
		} else if (tMax.y <= tMax.z) {             // Y is closest
			vp.y += stride.y;
            tMax.y += dr.y;
		} else {                                  // Z is closest
			vp.z += stride.z;
            tMax.z += dr.z;
		}

	}
}
