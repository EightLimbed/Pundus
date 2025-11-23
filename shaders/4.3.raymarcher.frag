#version 430 core

layout(std430, binding = 0) buffer BlockData {
    uint chunkMask[524288];
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
const vec3 colors[5] = {vec3(0.1,0.7,0.1), vec3(0.6,0.3,0.0), vec3(0.5,0.5,0.5), vec3(0.4,0.6,200.0), vec3(2.0)};
const int chunkSize = 4;
const float fchunkSize = 4.0;
const float renderDist = 2048;

// block data getter
uint getData(uint m) {
    uint idx = m >> 2u; // divide by 4
    uint bit = (m & 3u) * 8u; // which byte in that uint
    return (blockData[idx] >> bit) & 0xFFu;
}

// chunk mask getter
bool checkChunk(uint m) {
    uint idx = m >> 5u; // which 32-bit term (divide by 32)
    uint bit = m & 31u; // which bit in that term (mod 32 or whatever)
    return ((chunkMask[idx] >> bit) & 1u) != 0u;
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
    // camera setup.
    vec3 ro = vec3(pPosX,pPosY,pPosZ);
    vec3 lookAt = vec3(pDirX, pDirY, pDirZ);
    vec3 rd = getRayDir(gl_FragCoord.xy, vec2(screenWidth,screenHeight), lookAt, 1.0);
    
    // voxel space setup.
    ivec3 stride = ivec3(sign(rd));
    ivec3 vp = ivec3(floor(ro)); //starting position.
    // inverse of rd, made to be non 0.
    vec3 dr = 1.0 / max(abs(rd), vec3(1e-6));

    // distance to first voxel boundary.
    vec3 bound;
    bound.x = (rd.x > 0.0) ? (float(vp.x) + 1.0 - ro.x) : (ro.x - float(vp.x));
    bound.y = (rd.y > 0.0) ? (float(vp.y) + 1.0 - ro.y) : (ro.y - float(vp.y));
    bound.z = (rd.z > 0.0) ? (float(vp.z) + 1.0 - ro.z) : (ro.z - float(vp.z));

    ivec3 cp = ivec3(floor(vec3(vp) / fchunkSize));

    // distance to first chunk boundary.
    vec3 cBound;
    cBound.x = (rd.x > 0.0) ? (float(cp.x)*fchunkSize + fchunkSize - ro.x) : (ro.x - float(cp.x)*fchunkSize);
    cBound.y = (rd.y > 0.0) ? (float(cp.y)*fchunkSize + fchunkSize - ro.y) : (ro.y - float(cp.y)*fchunkSize);
    cBound.z = (rd.z > 0.0) ? (float(cp.z)*fchunkSize + fchunkSize - ro.z) : (ro.z - float(cp.z)*fchunkSize);

    vec3 tMax = bound * dr; // how far to first voxel boundary per axis.
    vec3 cTMax = cBound * dr; // how far to first chunk boundary per axis.
    vec3 tDelta = dr; // distance for voxels.
    vec3 cTDelta = dr*fchunkSize; // distance for chunks.

    float border = 0.0;

    for (int i = 0; i < renderDist; i++) {

        vec3 d = abs(vec3(vp)-ro);
        if (d.x > renderDist || d.y > renderDist || d.z > renderDist) return; // early out with distance.

        uint m = morton3D(vp);
        uint data = getData(m);

        ivec3 cp = ivec3(floor(vec3(vp) / fchunkSize));
        // chunk-level DDA removed temporarily, steps chunk distance when chunk is empty.
        uint cm = morton3D(cp)%16777216; // make it not read things from data buffer

        if (data > 0u) {
            vec3 c = colors[data-1]; // -1 to go to 0 in array when 0 is air.
            FragColor = vec4(c,1.0)+border;
            FragColor.xy -= ((data == 4 || data == 5) ? 0.0 : length(vp-ro)*0.0001);
            return;
        }

        if (!checkChunk(cm)) { // chunk boarder marcher
            border += 0.1;
        }

        // chunk-level DDA, steps chunk distance when chunk is empty.

		if (tMax.x < tMax.y && tMax.x < tMax.z) { // X is closest
			vp.x += stride.x;
            tMax.x += tDelta.x;
		} else if (tMax.y < tMax.z) {             // Y is closest
			vp.y += stride.y;
            tMax.y += tDelta.y;
		} else {                                  // Z is closest
			vp.z += stride.z;
            tMax.z += tDelta.z;

		}
	}
    FragColor = vec4(0.4,0.6,2.0,1.0)+border;
}