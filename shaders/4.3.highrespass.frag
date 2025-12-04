#version 430 core

layout(std430, binding = 0) buffer BlockData {
    uint occuMask[524288];
    uint blockData[];
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

// constants
const float passRes = 4.0;
const vec3 colors[5] = {vec3(0.1,0.7,0.1), vec3(0.6,0.3,0.0), vec3(0.5,0.5,0.5), vec3(0.4,0.6,1.0), vec3(1.0)};
const float renderDist = 1024.0;
const ivec2 nOffsets[4] = {ivec2(0,1), ivec2(0,-1), ivec2(1,0), ivec2(-1,0)};

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
    return ((occuMask[idx] >> bit) & 1u) == 0u;
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

// main raymarching loop.
void main() {

    ivec2 preSize = imageSize(prePass);
    ivec2 texel = ivec2(gl_FragCoord.xy) / int(passRes); // integer division, gets image coordinate.
    float dist = imageLoad(prePass, texel).x; // big distance.
    
    // prevents skipping with neighbor distances.
    for (int i = 0; i < 4; i++) {
        float nDist = imageLoad(prePass, texel+nOffsets[i]).x;
        if (nDist < dist) {
            dist = nDist;
            break;
        }
    }

    //FragColor.x = dist/1024.0;
    //return;
    // makes sure no close neighbors of dist are hits. corners seem unnecessary, but I might as well.
    dist = max(dist-8.0, 0.0); // safety
    
    FragColor = vec4(colors[3],1.0); // background color.
    if (dist > renderDist) return;

    // camera setup.
    vec3 lookAt = vec3(pDirX, pDirY, pDirZ);
    vec3 rd = getRayDir(gl_FragCoord.xy, vec2(screenWidth,screenHeight), lookAt, 1.0);

    vec3 ro = vec3(pPosX,pPosY,pPosZ)+ rd*dist;
    vec3 normal = vec3(0.0);
    
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

    // pre step normal calculations
    if (tMax.x <= tMax.y && tMax.x <= tMax.z) {
            normal = vec3(stride.x,0.0,0.0);
		} else if (tMax.y <= tMax.z) {
            normal = vec3(0.0,stride.y,0.0);
		} else {
            normal = vec3(0.0,0.0,stride.z);
    }

    for (int i = 0; i < 10000; i++) {

        float t = length(ro-vp)+dist;
        if (t > renderDist) return; // no artifact

        // check voxel
        uint m = morton3D(vp);
        uint data = getData(m);
        if (data > 0u) {
            vec3 c = colors[data-1]; // -1 to go to 0 in array when 0 is air.
            vec3 shaded = c-((data <4) ? (dot(normal, normalize(vp-vec3(500.0,1000.0,0.0))))*0.3 : 0.0); // normal shading.
            // apply distance fog.
            float percent = t/float(renderDist);
            float atten = percent*percent*percent*percent*percent;
            FragColor = vec4(shaded * (1.0 - atten) + atten * colors[3], 1.0);
            FragColor += imageLoad(prePass, texel).y;
            return;
        }
        
        //ivec3 cp = ivec3(floor(vec3(vp) / passRes)); // occupancy mask debug
        //if (checkChunk(morton3D(cp) % 16777216) && posWithin(vec3(vp), vec3(0.0), vec3(1024.0))) {
            //vec3 c = colors[2]; // -1 to go to 0 in array when 0 is air.
            //float percent = (float(i)+t)/float(renderDist+2048);
            //float atten = percent*percent*percent*percent;
            //FragColor = vec4(c-dot(normal, normalize(vp-vec3(500.0,1000.0,0.0)))*0.3 ,1.0);
            //return;
        //}

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