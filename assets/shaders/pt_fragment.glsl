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

uint seed;
float rand() {
    seed = seed * 1664525u + 1013904223u;
    return float(seed & 0x00FFFFFFu) / float(0x01000000u);
}

const float PI = 3.14159265;

vec3 randomCosineHemisphere(vec3 n) {
    float r1 = 2.0 * PI * rand(), r2 = rand(), r2s = sqrt(r2);
    vec3 w = n;
    vec3 u = normalize(cross(abs(w.x) > 0.1 ? vec3(0,1,0) : vec3(1,0,0), w));
    vec3 v = cross(w, u);
    return normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + w * sqrt(1.0 - r2));
}

// Случайная точка на сфере
vec3 randomOnSphere() {
    float z = 1.0 - 2.0 * rand();
    float r = sqrt(max(0.0, 1.0 - z*z));
    float phi = 2.0 * PI * rand();
    return vec3(r * cos(phi), r * sin(phi), z);
}

struct Hit { 
    float t; 
    vec3 p, n, albedo, emi; 
    float rough, ior; 
    int objId; // ID объекта для исключения при NEE
};

// Источник света
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

bool isVisible(vec3 from, vec3 to, int excludeId) {
    vec3 dir = to - from;
    float maxT = length(dir) - 0.002;
    dir = normalize(dir);
    
    Hit hit; hit.t = 1e10; hit.objId = -1;
    
    // Пол
    float tp = -(from.y + 1.0) / dir.y;
    if(tp > 0.001 && tp < hit.t) hit.t = tp;
    
    // Сферы
    if(excludeId != 0) sphere(from, dir, LIGHT_POS, LIGHT_RAD, vec3(0), vec3(0), 0.0, 0.0, 0, hit);
    if(excludeId != 1) sphere(from, dir, vec3(0, 0, -4), 1.0, vec3(0), vec3(0), 0.0, 1.5, 1, hit);
    if(excludeId != 2) sphere(from, dir, vec3(3, 0, -3), 1.1, vec3(0), vec3(0), 0.0, 0.0, 2, hit);
    
    return hit.t > maxT;
}

// Прямая выборка света
vec3 sampleLight(vec3 p, vec3 n, vec3 albedo, float rough) {
    // Случайная точка на источнике света
    vec3 lightPoint = LIGHT_POS + randomOnSphere() * LIGHT_RAD;
    vec3 toLight = lightPoint - p;
    float dist = length(toLight);
    vec3 L = toLight / dist;
    
    float NdotL = dot(n, L);
    if(NdotL <= 0.0) return vec3(0);
    
    // Проверка видимости
    if(!isVisible(p + n * 0.001, lightPoint, -1)) return vec3(0);
    
    // Площадь сферы
    float lightArea = 4.0 * PI * LIGHT_RAD * LIGHT_RAD;
    float cosAtLight = max(dot(-L, normalize(lightPoint - LIGHT_POS)), 0.0);
    float solidAngle = (lightArea * cosAtLight) / (dist * dist);
    
    // Простая BRDF
    float brdf = NdotL / PI;
    
    return albedo * LIGHT_EMI * brdf * solidAngle;
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
    sphere(ro, rd, vec3(0, 0, -4), 1.0, vec3(1), vec3(0), 0.0, 1.5, 1, hit);
    sphere(ro, rd, vec3(3, 0, -3), 1.1, vec3(0.8, 0.9, 1), vec3(0), 0.05, 0.0, 2, hit);

    // Логотип
    float sn = sin(-u_logoRot), cs = cos(-u_logoRot);
    mat3 rot = mat3(cs, 0, sn, 0, 1, 0, -sn, 0, cs);
    vec3 r_ro = rot * (ro - u_logoPos), r_rd = rot * rd;
    float tq = -r_ro.z / r_rd.z;
    if(tq > 0.001 && tq < hit.t) {
        vec3 p = r_ro + r_rd * tq;
        if(all(lessThan(abs(p.xy), vec2(1.0)))) {
            vec4 tex = texture(u_logoTex, p.xy * 0.5 + 0.5);
            if(tex.a > 0.5) {
                hit.t = tq; hit.p = ro + rd*tq; hit.n = rot[2];
                hit.albedo = tex.rgb; hit.emi = vec3(0); hit.rough = 0.9; hit.ior = 0.0;
                hit.objId = 20;
            }
        }
    }
}

vec3 trace(vec3 ro, vec3 rd) {
    if (u_useRayTracing == 0) {
        Hit hit; hit.t = 1e10; hit.objId = -1;
        checkScene(ro, rd, hit);

        // Если попали в небо
        if (hit.t > 1e9) {
             return mix(vec3(0.5, 0.7, 1.0), vec3(1.0), rd.y * 0.5 + 0.5) * 0.5;
        }

        vec3 sunDir = normalize(vec3(0.5, 1.0, 0.5));
        float diff = max(dot(hit.n, sunDir), 0.2);

        return hit.albedo * diff + hit.emi;
    }

    vec3 col = vec3(0), mask = vec3(1);
    bool lastSpecular = true;
    
    for(int i = 0; i < 6; i++) {
        Hit hit; hit.t = 1e10; hit.objId = -1;
        checkScene(ro, rd, hit);
        
        if(hit.t > 1e9) {
            // Небо
            col += mask * mix(vec3(0.5, 0.7, 1.0), vec3(1.0), rd.y * 0.5 + 0.5) * 0.5;
            break;
        }

        // Эмиссия только для прямых попаданий
        if(lastSpecular || hit.objId == 0) {
            col += mask * hit.emi;
        }
        
        float cosTheta = dot(-rd, hit.n);
        float F0 = hit.ior > 0.0 ? pow((1.0 - hit.ior) / (1.0 + hit.ior), 2.0) : 0.04;
        float fresnel = F0 + (1.0 - F0) * pow(max(1.0 - abs(cosTheta), 0.0), 5.0);

        if(hit.ior > 0.0 && rand() > fresnel) {
            // Преломление
            bool outside = cosTheta > 0.0;
            float eta = outside ? 1.0 / hit.ior : hit.ior;
            vec3 refracted = refract(rd, outside ? hit.n : -hit.n, eta);
            
            if(length(refracted) < 0.5) {
                // Полное внутреннее отражение
                rd = reflect(rd, hit.n);
                ro = hit.p + hit.n * 0.001;
            } else {
                rd = refracted;
                ro = hit.p - hit.n * 0.001;
            }
            lastSpecular = true;
            
        } else if(hit.rough < 0.3) {
            // Зеркальное отражение
            vec3 reflected = reflect(rd, hit.n);
            // Размытие
            vec3 roughDir = randomCosineHemisphere(hit.n);
            rd = normalize(mix(reflected, roughDir, hit.rough));
            mask *= hit.albedo;
            ro = hit.p + hit.n * 0.001;
            lastSpecular = hit.rough < 0.1;
            
        } else {
            // Диффузная поверхность, используем NEE
            col += mask * sampleLight(hit.p, hit.n, hit.albedo, hit.rough);
            
            rd = randomCosineHemisphere(hit.n);
            mask *= hit.albedo;
            ro = hit.p + hit.n * 0.001;
            lastSpecular = false;
        }
        
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