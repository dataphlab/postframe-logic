#version 460 core
out vec4 FragColor;
in vec2 TexCoords;

uniform vec2 u_resolution;
uniform vec3 u_pos;
uniform float u_sample_part;
uniform vec2 u_seed1;
uniform mat4 u_view;
uniform sampler2D u_sample, u_logoTex, u_floorTex;
uniform vec3 u_logoPos;
uniform float u_logoRot;
uniform int u_useRayTracing;

// --- СТРУКТУРЫ И БУФЕРЫ ---

struct Triangle {
    vec3 v0; float pad1;
    vec3 v1; float pad2;
    vec3 v2; float pad3;
    vec3 color; float pad4;
};

struct BVHNode {
    vec3 minBounds; int leftFirst;
    vec3 maxBounds; int triCount;
};

struct MeshObject {
    vec3 minAABB; float pad1;
    vec3 maxAABB; float pad2;
    int bvhRootIndex;
    int pad3; int pad4; int pad5;
};

// Буферы данных
layout(std430, binding = 2) buffer MeshBuffer {
    Triangle triangles[];
};

layout(std430, binding = 3) buffer ObjectBuffer {
    MeshObject objects[];
};

layout(std430, binding = 4) buffer BVHBuffer {
    BVHNode bvhNodes[];
};

struct Hit { 
    float t; 
    vec3 p, n, albedo, emi; 
    float rough, ior; 
    int objId; 
};

// --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---

uint seed;
float rand() {
    seed = seed * 1664525u + 1013904223u;
    return float(seed & 0x00FFFFFFu) / float(0x01000000u);
}

const float PI = 3.14159265;

// Пересечение с AABB
float intersectAABB_dist(vec3 ro, vec3 invRd, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - ro) * invRd;
    vec3 tMax = (boxMax - ro) * invRd;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return (tNear <= tFar && tFar > 0.0) ? tNear : 1e30;
}

// Пересечение с Треугольником
float intersectTriangle(vec3 ro, vec3 rd, vec3 v0, vec3 v1, vec3 v2) {
    vec3 v0v1 = v1 - v0;
    vec3 v0v2 = v2 - v0;
    vec3 pvec = cross(rd, v0v2);
    float det = dot(v0v1, pvec);
    if (abs(det) < 0.00001) return 1e10;
    float invDet = 1.0 / det;
    vec3 tvec = ro - v0;
    float u = dot(tvec, pvec) * invDet;
    if (u < 0.0 || u > 1.0) return 1e10;
    vec3 qvec = cross(tvec, v0v1);
    float v = dot(rd, qvec) * invDet;
    if (v < 0.0 || u + v > 1.0) return 1e10;
    float t = dot(v0v2, qvec) * invDet;
    return (t > 0.001) ? t : 1e10;
}

// --- ЛОГИКА BVH ---

void checkMeshBVH(vec3 ro, vec3 rd, int rootNodeIdx, inout Hit hit) {
    vec3 invRd = 1.0 / rd;
    
    int stack[32];
    int stackPtr = 0;
    stack[stackPtr++] = rootNodeIdx;

    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        
        // Читаем узел
        BVHNode node = bvhNodes[nodeIdx];

        // Проверяем AABB узла. 
        float distBox = intersectAABB_dist(ro, invRd, node.minBounds, node.maxBounds);
        if (distBox >= hit.t) continue;

        if (node.triCount > 0) { 
            for (int i = 0; i < node.triCount; i++) {
                int triIdx = node.leftFirst + i;
                
                Triangle tri = triangles[triIdx];
                
                float t = intersectTriangle(ro, rd, tri.v0, tri.v1, tri.v2);
                if (t < hit.t) {
                    hit.t = t; 
                    hit.p = ro + rd * t;
                    
                    vec3 e1 = tri.v1 - tri.v0; 
                    vec3 e2 = tri.v2 - tri.v0;
                    hit.n = normalize(cross(e1, e2));
                    if(dot(rd, hit.n) > 0.0) hit.n = -hit.n;
                    
                    hit.albedo = tri.color; 
                    hit.emi = vec3(0); 
                    hit.rough = 1.0; 
                    hit.ior = 0.0; 
                    hit.objId = 100 + triIdx;
                }
            }
        } else {
            stack[stackPtr++] = node.leftFirst;
            stack[stackPtr++] = node.leftFirst + 1;
            
            // Защита от переполнения стека
            if (stackPtr >= 32) break; 
        }
    }
}

// --- СЦЕНА И СВЕТ ---

const vec3 LIGHT_POS = vec3(-4, 3, -6);
const float LIGHT_RAD = 1.4;
const vec3 LIGHT_EMI = vec3(15, 12, 9);

void sphere(vec3 ro, vec3 rd, vec3 pos, float rad, vec3 col, vec3 emi, float rough, float ior, int id, inout Hit hit) {
    vec3 oc = ro - pos;
    float b = dot(oc, rd), c = dot(oc, oc) - rad * rad, h = b*b - c;
    if(h > 0.0) {
        float t = -b - sqrt(h);
        if(t > 0.001 && t < hit.t) {
            hit.t = t; hit.p = ro + rd*t; hit.n = (hit.p - pos) / rad;
            hit.albedo = col; hit.emi = emi; hit.rough = rough; hit.ior = ior;
            hit.objId = id;
        }
    }
}

void checkScene(vec3 ro, vec3 rd, inout Hit hit) {
    // Пол
    float tp = -(ro.y + 1.0) / rd.y;
    if(tp > 0.001 && tp < hit.t) {
        hit.t = tp; hit.p = ro + rd*tp; hit.n = vec3(0,1,0);
        hit.albedo = texture(u_floorTex, hit.p.xz * 0.1).rgb;
        hit.emi = vec3(0); hit.rough = 1.0; hit.ior = 0.0; hit.objId = 10;
    }
    
    // Сферы
    sphere(ro, rd, LIGHT_POS, LIGHT_RAD, vec3(1), LIGHT_EMI, 1.0, 0.0, 0, hit);

    // BVH ОБЪЕКТЫ
    vec3 invRd = 1.0 / rd;
    for(int i = 0; i < objects.length(); i++) {
        // Проверяем главную коробку объекта
        float dist = intersectAABB_dist(ro, invRd, objects[i].minAABB, objects[i].maxAABB);
        
        if (dist < hit.t) {
            // Если луч попадает в габариты объекта, запускаем трассировку BVH
            checkMeshBVH(ro, rd, objects[i].bvhRootIndex, hit);
        }
    }
}

vec3 randomCosineHemisphere(vec3 n) {
    float r1 = 2.0 * PI * rand(), r2 = rand(), r2s = sqrt(r2);
    vec3 w = n;
    vec3 u = normalize(cross(abs(w.x) > 0.1 ? vec3(0,1,0) : vec3(1,0,0), w));
    vec3 v = cross(w, u);
    return normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1.0 - r2));
}

vec3 randomOnSphere() {
    float z = 1.0 - 2.0 * rand();
    float r = sqrt(max(0.0, 1.0 - z*z));
    float phi = 2.0 * PI * rand();
    return vec3(r * cos(phi), r * sin(phi), z);
}

bool isVisible(vec3 from, vec3 to, int excludeId) {
    vec3 dir = to - from;
    float maxT = length(dir) - 0.002;
    dir = normalize(dir);
    
    Hit hit; hit.t = 1e10; hit.objId = -1;
    
    float tp = -(from.y + 1.0) / dir.y;
    if(tp > 0.001 && tp < hit.t) hit.t = tp;
    
    if(excludeId != 0) sphere(from, dir, LIGHT_POS, LIGHT_RAD, vec3(0), vec3(0), 0.0, 0.0, 0, hit);
    
    return hit.t > maxT;
}

vec3 sampleLight(vec3 p, vec3 n, vec3 albedo, float rough) {
    vec3 lightPoint = LIGHT_POS + randomOnSphere() * LIGHT_RAD;
    vec3 toLight = lightPoint - p;
    float dist = length(toLight);
    vec3 L = toLight / dist;
    float NdotL = dot(n, L);
    if(NdotL <= 0.0) return vec3(0);
    if(!isVisible(p + n * 0.001, lightPoint, -1)) return vec3(0);
    float lightArea = 4.0 * PI * LIGHT_RAD * LIGHT_RAD;
    float cosAtLight = max(dot(-L, normalize(lightPoint - LIGHT_POS)), 0.0);
    float solidAngle = (lightArea * cosAtLight) / (dist * dist);
    float brdf = NdotL / PI;
    return albedo * LIGHT_EMI * brdf * solidAngle;
}

vec3 trace(vec3 ro, vec3 rd) {
    // Режим "Без лучей"
    if (u_useRayTracing == 0) {
        Hit hit; hit.t = 1e10; hit.objId = -1;
        checkScene(ro, rd, hit);

        if (hit.t > 1e9) return mix(vec3(0.5, 0.7, 1.0), vec3(1.0), rd.y * 0.5 + 0.5) * 0.5;

        vec3 sunDir = normalize(vec3(0.5, 1.0, 0.5));
        float diff = max(dot(hit.n, sunDir), 0.2);
        return hit.albedo * diff + hit.emi;
    }

    vec3 col = vec3(0), mask = vec3(1);
    bool lastSpecular = true;
    
    // -- !!!!!! ---
    for(int i = 0; i < 16; i++) {
        Hit hit; hit.t = 1e10; hit.objId = -1;
        checkScene(ro, rd, hit);
        
        if(hit.t > 1e9) {
            col += mask * mix(vec3(0.5, 0.7, 1.0), vec3(1.0), rd.y * 0.5 + 0.5) * 0.5;
            break;
        }

        if(lastSpecular || hit.objId == 0) col += mask * hit.emi;
        
        col += mask * sampleLight(hit.p, hit.n, hit.albedo, hit.rough);
        rd = randomCosineHemisphere(hit.n);
        mask *= hit.albedo;
        ro = hit.p + hit.n * 0.001;
        lastSpecular = false;
        
        if(i > 2) {
            float p = max(mask.r, max(mask.g, mask.b));
            if(rand() > p) break;
            mask /= p;
        }
    }
    return col;
}

void main() {
    seed = uint(gl_FragCoord.x) * 1973u + uint(gl_FragCoord.y) * 9277u + uint(u_seed1.x * 1000.0) * 26699u;
    vec2 jitter = (u_useRayTracing == 1) ? (vec2(rand(), rand()) - 0.5) : vec2(0.0);
    vec2 uv = ((TexCoords + jitter / u_resolution) * 2.0 - 1.0) * vec2(u_resolution.x / u_resolution.y, 1.0);
    vec3 rd = normalize(mat3(inverse(u_view)) * vec3(uv, -1.5));
    vec3 color = trace(u_pos, rd);
    vec3 last = texture(u_sample, TexCoords).rgb;
    FragColor = vec4(mix(last, color, u_sample_part), 1.0);
}