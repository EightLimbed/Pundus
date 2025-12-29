#version 430 core

struct chunk { // 64^3 chunks
    uint occuMask[128]; // 1 bit per 64 voxels.
    uint blockData[65536]; // 8 bits per voxel.
};

layout(std430, binding = 0) buffer BlockData {
    chunk chunks[];
};

// ligthing precompute data
layout(std430, binding = 1) buffer LightingData {
    uint AOcells;
    ivec3 AOoffsets[][6];
};

out vec4 FragColor;

// positions from coarse prepass
layout(rgba32f, binding=0) uniform readonly image2D prePass;

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

uniform float AOchange;

// constants
const float renderDist = 1024.0;
const float passRes = 4.0;
const uint chunkVoxels = 64*64*64;
const vec3 colors[8] = {vec3(0.1,0.7,0.1), vec3(0.1,0.8,0.0), vec3(1.0,0.3,0.5), vec3(1.0,0.5,0.1), vec3(0.6,0.3,0.0), vec3(0.5,0.5,0.5), vec3(1.0), vec3(0.4,0.6,1.0)};

// precompute constants
const ivec2 nOffsets[4] = {ivec2(0,1), ivec2(0,-1), ivec2(1,0), ivec2(-1,0)}; // offsets for low res pass sampling.
const int colorLen = colors.length()-1;


// block data getter
uint getData(uint m) {
    uint chunkIndex = m / chunkVoxels;
    uint localIndex = m % chunkVoxels;

    uint idx = localIndex >> 2u;
    uint bit = (localIndex & 3u) * 8u;

    return (chunks[chunkIndex].blockData[idx] >> bit) & 0xFFu;
}

// chunk mask getter.
bool checkChunk(uint m) {
    uint chunkIndex = m / chunkVoxels;
    uint localIndex = m % chunkVoxels;

    uint idx = (localIndex >> 6u) >> 5u; // (voxel / 64) / 32
    uint bit = (localIndex >> 6u) & 31u; // (voxel / 64) % 32

    return ((chunks[chunkIndex].occuMask[idx] >> bit) & 1u) == 0u;
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

float getAmbientOcclusion(ivec3 vp, vec3 normal) {
    float occ = 1.0;
    vp -= ivec3(normal);
    // face id
    int face = (normal.x > 0.0) ? 0 : (normal.y > 0.0) ? 1 : (normal.z > 0.0) ? 2 : (normal.x < 0.0) ? 3 : (normal.y < 0.0) ? 4 : 5;
    
    for (int i = 0; i < AOcells; i++) {

        ivec3 offset = AOoffsets[i][face];

        uint m = morton3D(vp - offset);
        uint data = getData(m);
        if (data > 0u) {
            occ -= AOchange;
        }
    }
    return occ;
}

float getSkyLight(ivec3 vp, vec3 normal, vec3 rd, vec3 ld) {
    // early return for instant intercept.
    if (dot(normal, ld) > 0.0) return 0.3;

    // add normal offset to vp.
    vp -= ivec3(normal);

    // specular.
    rd.xy = -rd.xy;
    vec3 halfDir = normalize(-rd + ld);

    float specularStrength = max(dot(normal, -halfDir), 0.0);
    float specular = pow(specularStrength, 32.0)*2.0;

    // diffuse raymarched.
    float diffuse = 1.0;
    vec3 ro = vp;

    // voxel space setup.
    ivec3 stride = ivec3(sign(ld));
    // inverse of rd, made to be non 0.
    vec3 dr = 1.0 / max(abs(ld), vec3(1e-6));

    // distance to first voxel boundary.
    vec3 bound;
    bound.x = (ld.x > 0.0) ? (1.0) : (0.0);
    bound.y = (ld.y > 0.0) ? (1.0) : (0.0);
    bound.z = (ld.z > 0.0) ? (1.0) : (0.0);

    
    vec3 tMax = bound * dr; // how far to first voxel boundary per axis.
    for (int i = 0; i < 128; i++) {

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
    
        // check chunk
        uint m = morton3D(vp);
        uint data = getData(m);
        if (data > 0u) {
            if (data < colorLen) diffuse *= 0.9; // in shadow
            if (diffuse < 0.3) return 0.3;
        }
    
    }
    return diffuse+specular; // full light
}

// main raymarching loop. get rid of normals here when lighting working.
void main() {
    vec2 adjustFrag = gl_FragCoord.xy - vec2(screenWidth,screenHeight)/2;
    if (dot(adjustFrag,adjustFrag) < 16.0) return;

    ivec2 texel = ivec2(gl_FragCoord.xy) / int(passRes); // integer division, gets image coordinate.
    ivec2 preSizeOffset = ivec2(0,(imageSize(prePass).y)/2); // offset to bottom half of prepass, where light is stored.

    // light data loading.
    //vec4 l = imageLoad(prePass, texel+preSizeOffset);
    //float light = (l.x+l.y+l.z+l.w)*0.25;

    float dist = imageLoad(prePass, texel).x;
    
    // prevents skipping with neighbor distances.
    for (int i = 0; i < 4; i++) {
        dist = min(dist, imageLoad(prePass, texel+nOffsets[i]).x);
    }

    FragColor = vec4(colors[colorLen],1.0); // background color.
    if (dist > renderDist) return;

    dist = max(dist-8.0, 0.0); // safety

    // camera setup.
    vec3 lookAt = vec3(pDirX, pDirY, pDirZ);
    vec3 rd = getRayDir(gl_FragCoord.xy, vec2(screenWidth,screenHeight), lookAt, 1.0);

    vec3 ro = vec3(pPosX,pPosY,pPosZ) + rd*dist;

    // voxel space setup.
    ivec3 stride = ivec3(sign(rd));
    // inverse of rd, made to be non 0.
    vec3 dr = 1.0 / max(abs(rd), vec3(1e-6));

    ivec3 vp = ivec3(floor(ro)); //starting position.

    // distance to first voxel boundary.
    vec3 bound;
    bound.x = (rd.x > 0.0) ? (float(vp.x) + 1.0 - ro.x) : (ro.x - float(vp.x));
    bound.y = (rd.y > 0.0) ? (float(vp.y) + 1.0 - ro.y) : (ro.y - float(vp.y));
    bound.z = (rd.z > 0.0) ? (float(vp.z) + 1.0 - ro.z) : (ro.z - float(vp.z));

    vec3 tMax = bound * dr; // how far to first voxel boundary per axis.

    // normals
    vec3 normal;
    if (tMax.x <= tMax.y && tMax.x <= tMax.z) {
        normal = vec3(stride.x,0.0,0.0);
	} else if (tMax.y <= tMax.z) {
        normal = vec3(0.0,stride.y,0.0);
	} else {
        normal = vec3(0.0,0.0,stride.z);
    }

    for (int i = 0; i < 10000; i++) {

        float t = distance(ro,vp)+dist;
        if (t > renderDist) return; // no artifact

        // check voxel
        uint m = morton3D(vp);
        uint data = getData(m);
        if (data > 0u) {
            vec3 c = colors[data-1]; // -1 to go to 0 in array when 0 is air.
            float skyLight = getSkyLight(vp, normal, rd, vec3(cos(1.0*0.5), 0.717,sin(1.0*0.5))); // light from sun direction.
            float ambientOcclusion = getAmbientOcclusion(vp, normal);
            vec3 shaded = c*((data < colorLen) ? ambientOcclusion*skyLight : 1.0); // shading.
            // apply distance fog.
            float percent = t/float(renderDist);
            float atten = percent*percent*percent*percent*percent;
            FragColor = vec4(shaded * (1.0 - atten) + atten * colors[colorLen], 1.0);
            return;
        }

		if (tMax.x <= tMax.y && tMax.x <= tMax.z) { // X is closest
			vp.x += stride.x;
            tMax.x += dr.x;
            normal = vec3(stride.x,0.0,0.0);
		} else if (tMax.y <= tMax.z) {             // Y is closest
			vp.y += stride.y;
            tMax.y += dr.y;
            normal = vec3(0.0,stride.y,0.0);
		} else {                                  // Z is closest
			vp.z += stride.z;
            tMax.z += dr.z;
            normal = vec3(0.0,0.0,stride.z);
		}

	}
}