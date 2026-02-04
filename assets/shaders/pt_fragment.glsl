#version 460 core
out vec4 FragColor;
in vec2 TexCoords;

uniform vec2 u_resolution;
uniform vec3 u_pos;
uniform float u_sample_part;
uniform vec2 u_seed1;
uniform mat4 u_view;
uniform sampler2D u_sample, u_floorTex;
uniform int u_useRayTracing;
uniform float floorSize;
uniform int u_showLightGizmos;
uniform int u_selectedId;

// --- СТРУКТУРЫ И БУФЕРЫ ---

struct Light {
    vec3 position;
    float radius;
    vec3 emission;
    float pad;
};

layout(std430, binding = 5) buffer LightBuffer {
    Light lights[];
};

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
    int pad3, pad4, pad5;
};

layout(std430, binding = 2) buffer MeshBuffer { Triangle triangles[]; };
layout(std430, binding = 3) buffer ObjectBuffer { MeshObject objects[]; };
layout(std430, binding = 4) buffer BVHBuffer { BVHNode bvhNodes[]; };
layout(std430, binding = 6) buffer SelectionBuffer {
    int hoverId;
};

uniform vec2 u_mousePos;

struct Hit { 
    float t; 
    vec3 p, n, albedo, emi; 
    float rough;
    int objId; 
};

struct OverlayHit {
    float t;
    vec3 color;
};

// --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---

uint seed;
float rand() {
    seed = seed * 1664525u + 1013904223u;
    return float(seed & 0x00FFFFFFu) / float(0x01000000u);
}

const float PI = 3.14159265;

float intersectAABB_dist(vec3 ro, vec3 invRd, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - ro) * invRd;
    vec3 tMax = (boxMax - ro) * invRd;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return (tNear <= tFar && tFar > 0.0) ? tNear : 1e30;
}

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

void checkMeshBVH(vec3 ro, vec3 rd, int rootNodeIdx, int globalObjId, inout Hit hit) {
    vec3 invRd = 1.0 / rd;
    int stack[32]; int stackPtr = 0;
    stack[stackPtr++] = rootNodeIdx;
    
    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        BVHNode node = bvhNodes[nodeIdx];
        
        if (intersectAABB_dist(ro, invRd, node.minBounds, node.maxBounds) >= hit.t) continue;
        
        if (node.triCount > 0) { 
            for (int i = 0; i < node.triCount; i++) {
                int triIdx = node.leftFirst + i;
                Triangle tri = triangles[triIdx];
                float t = intersectTriangle(ro, rd, tri.v0, tri.v1, tri.v2);
                if (t < hit.t) {
                    hit.t = t; 
                    hit.p = ro + rd * t;
                    hit.n = normalize(cross(tri.v1 - tri.v0, tri.v2 - tri.v0));
                    if(dot(rd, hit.n) > 0.0) hit.n = -hit.n;
                    hit.albedo = tri.color; 
                    hit.emi = vec3(0);
                    // ТЕПЕРЬ ПРИСВАИВАЕМ ID ОБЪЕКТА, А НЕ ТРЕУГОЛЬНИКА
                    hit.objId = globalObjId; 
                }
            }
        } else {
            stack[stackPtr++] = node.leftFirst; 
            stack[stackPtr++] = node.leftFirst + 1;
        }
    }
}

// Функция для отрисовки чисто визуальных штук
void checkOverlays(vec3 ro, vec3 rd, inout OverlayHit ohit) {
    if (u_showLightGizmos == 0) return;

    for(int i = 0; i < lights.length(); i++) {
        vec3 lp = lights[i].position;
        
        // Рисуем сферу
        vec3 oc = ro - lp;
        float b = dot(oc, rd);
        float c = dot(oc, oc) - 0.05;
        float h = b*b - c;
        if (h > 0.0) {
            float t = -b - sqrt(h);
            if (t > 0.0 && t < ohit.t) {
                ohit.t = t;
                ohit.color = vec3(1.0, 1.0, 1.0); // Желтый
            }
        }
    }
}

void checkScene(vec3 ro, vec3 rd, inout Hit hit) {
    // Пол (ID = 10)
    float tp = -(ro.y + 1.0) / rd.y;
    if(tp > 0.001 && tp < hit.t) {
        vec3 intersectPoint = ro + rd * tp;
        if(abs(intersectPoint.x) < floorSize && abs(intersectPoint.z) < floorSize) {
            hit.t = tp; 
            hit.p = intersectPoint; 
            hit.n = vec3(0, 1, 0);
            hit.albedo = texture(u_floorTex, hit.p.xz * 0.1).rgb;
            hit.emi = vec3(0); 
            hit.objId = 10;
        }
    }

    // Меши (ID = индекс в массиве objects)
    vec3 invRd = 1.0 / rd;
    for(int i = 0; i < objects.length(); i++) {
        // Сначала быстрая проверка по общему AABB объекта
        if (intersectAABB_dist(ro, invRd, objects[i].minAABB, objects[i].maxAABB) < hit.t) {
            // Передаем индекс 'i' как ID объекта
            checkMeshBVH(ro, rd, objects[i].bvhRootIndex, i, hit);
        }
    }
}

vec3 randomOnSphere() {
    float z = 1.0 - 2.0 * rand(), r = sqrt(max(0.0, 1.0 - z*z)), phi = 2.0 * PI * rand();
    return vec3(r * cos(phi), r * sin(phi), z);
}

bool isVisible(vec3 from, vec3 to) {
    vec3 dir = to - from;
    float maxT = length(dir) - 0.002;
    Hit hit; hit.t = 1e10;
    checkScene(from, normalize(dir), hit);
    return hit.t > maxT;
}

// СЭМПЛИНГ ВСЕХ ИСТОЧНИКОВ ИЗ SSBO
vec3 sampleAllLights(vec3 p, vec3 n, vec3 albedo) {
    vec3 total = vec3(0);
    for(int i = 0; i < lights.length(); i++) {
        vec3 lightPoint = lights[i].position + randomOnSphere() * lights[i].radius;
        vec3 toLight = lightPoint - p;
        float dist = length(toLight);
        vec3 L = toLight / dist;
        float NdotL = max(dot(n, L), 0.0);
        
        if(NdotL > 0.0 && isVisible(p + n * 0.001, lightPoint)) {
            float lightArea = 4.0 * PI * lights[i].radius * lights[i].radius;
            // Упрощенная модель затухания
            float atten = lightArea / (dist * dist + 0.01);
            total += albedo * lights[i].emission * NdotL * atten * 0.1;
        }
    }
    return total;
}

vec3 randomCosineHemisphere(vec3 n) {
    float r1 = 2.0 * PI * rand(), r2 = rand(), r2s = sqrt(r2);
    vec3 w = n, u = normalize(cross(abs(w.x) > 0.1 ? vec3(0,1,0) : vec3(1,0,0), w)), v = cross(w, u);
    return normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1.0 - r2));
}

vec3 trace(vec3 ro, vec3 rd) {
    if (u_useRayTracing == 0) {
        Hit hit; hit.t = 1e10; hit.objId = -1;
        checkScene(ro, rd, hit);
        if (hit.t > 1e9) return mix(vec3(0.5, 0.7, 1.0), vec3(1.0), rd.y * 0.5 + 0.5) * 0.5;
        vec3 sunDir = normalize(vec3(0.5, 1.0, 0.5));
        return hit.albedo * max(dot(hit.n, sunDir), 0.2);
    }

    vec3 col = vec3(0), mask = vec3(1);
    for(int i = 0; i < 2; i++) {
        Hit hit; hit.t = 1e10; hit.objId = -1;
        checkScene(ro, rd, hit);
        
        if(hit.t > 1e9) {
            col += mask * mix(vec3(0.5, 0.7, 1.0), vec3(1.0), rd.y * 0.5 + 0.5) * 0.2;
            break;
        }

        col += mask * sampleAllLights(hit.p, hit.n, hit.albedo);
        
        rd = randomCosineHemisphere(hit.n);
        mask *= hit.albedo;
        ro = hit.p + hit.n * 0.001;
        
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

    Hit sceneHit; 
    sceneHit.t = 1e10; 
    sceneHit.objId = -1;
    checkScene(u_pos, rd, sceneHit);

    if (int(gl_FragCoord.x) == int(u_mousePos.x) && int(gl_FragCoord.y) == int(u_mousePos.y)) {
        hoverId = sceneHit.objId; 
    }

    vec3 physicalColor = trace(u_pos, rd); 

    OverlayHit ohit;
    ohit.t = 1e10;
    ohit.color = vec3(0);
    checkOverlays(u_pos, rd, ohit);

    vec3 finalColor;
    if (ohit.t < sceneHit.t) {
        finalColor = ohit.color;
    } else {
        finalColor = physicalColor;
    }

    if (u_selectedId != -1 && sceneHit.objId == u_selectedId) {
        finalColor = mix(finalColor, vec3(1.0, 0.6, 0.0), 0.3); 
        finalColor += vec3(0.1);
    }

    vec3 last = texture(u_sample, TexCoords).rgb;
    FragColor = vec4(mix(last, finalColor, u_sample_part), 1.0);
}