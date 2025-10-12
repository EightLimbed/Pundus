#version 430 core
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
const vec3 s = vec3(0.4,0.4,0.4);
const float e = 0.001;

vec3 getDomainID(vec3 p) {
    return clamp(round(p/s),-3.0,3.0);
}

float getPlanet(vec3 p) {
    vec3 q = abs(p - s*getDomainID(p))-0.1;
    return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
    //vec3 q = p - s*getDomainID(p);
    //return length(q)-0.1;
}

vec3 getNormal(vec3 p) {
    // surface normals
    return normalize(vec3(
        getPlanet(p + vec3(e,0,0)) - getPlanet(p - vec3(e,0,0)),
        getPlanet(p + vec3(0,e,0)) - getPlanet(p - vec3(0,e,0)),
        getPlanet(p + vec3(0,0,e)) - getPlanet(p - vec3(0,0,e))
    ));
}

// camera shizzle
vec3 getRayDir(vec2 fragCoord, vec2 res, vec3 lookAt, float zoom) {
    vec2 uv = (fragCoord - 0.5 * res) / res.y;
    vec3 f = normalize(lookAt);
    vec3 r = normalize(cross(vec3(0.0,1.0,0.0), f));
    vec3 u = cross(f,r);
    return normalize(f + zoom * (uv.x*r + uv.y*u));
}

float lightMarch(vec3 ro, vec3 lightPos, float lightStren) {
    float lightMod = 1.0/lightStren;
    vec3 rd = normalize(lightPos-ro);
    float t = 0.0;
    float dist = length(ro-lightPos);
    float tsid = 1.0/dist;
    float atten = 0.0;
    // normal offset
    ro += getNormal(ro)*e;

    for (int i = 0; i < 128; i++) {
        vec3 p = ro + t * rd;
        float d = getPlanet(p);
        bool hit = (d<=e);
        if (hit) atten -= d; // ray hits something in way of light source
        if (t > dist - e) break;
        t += hit ? 0.01 : d;
    }
    return dist*lightMod+min(atten*8.0*tsid,dist*lightMod*0.6);
}

// main raymarching loop
void main() {
    // camera setup
    FragColor = vec4(0.0);
    vec3 ro = vec3(pPosX,pPosY,pPosZ);
    vec3 lookAt = vec3(pDirX, pDirY, pDirZ);
    vec3 rd = getRayDir(gl_FragCoord.xy, vec2(800,600), lookAt, 1.0);

    float t = 0.0;

    for (int i = 0; i < 128; i++) {
        vec3 p = ro + t * rd;
        float d = getPlanet(p);
        if (d < e) {
            // does lighting if hits
            float tL = lightMarch(p, 3.5*vec3(cos(iTime),1.0,sin(iTime)), 10.0);
            //FragColor = vec4(vec3(tL,tL,tL),1.0);
            FragColor = vec4(vec3(0.7,1.0,1.0)-vec3(tL),1.0);
            break;
        }
        t += d;
        if (t > 40.0) break;
    }
}