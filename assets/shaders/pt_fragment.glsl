#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform vec2 u_resolution;
uniform vec3 u_pos;
uniform float u_sample_part;
uniform vec2 u_seed1;
uniform mat4 u_view;

uniform sampler2D u_sample;   
uniform sampler2D u_logoTex;  
uniform sampler2D u_floorTex; 

uniform vec3 u_logoPos;
uniform float u_logoRot;

uint seed;
uint pcg_hash() {
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
float rand() {
    seed = pcg_hash();
    return float(seed) * (1.0 / 4294967296.0);
}

vec3 cosWeightedRandom(vec3 n) {
    float r1 = rand(), r2 = rand();
    float r = sqrt(r1), phi = 6.283185 * r2;
    vec3 w = n, u = normalize(cross((abs(w.x) > 0.1 ? vec3(0, 1, 0) : vec3(1, 0, 0)), w)), v = cross(w, u);
    return normalize(u * r * cos(phi) + v * r * sin(phi) + w * sqrt(1.0 - r1));
}

struct Hit { float t; vec3 p, n, color, emi; float rough, ior; };

void checkScene(vec3 ro, vec3 rd, inout Hit hit) {
    // 1. ПОЛ
    float tp = -(ro.y + 1.0) / rd.y;
    if(tp > 0.001 && tp < hit.t) {
        hit.t = tp; hit.p = ro + rd*tp; hit.n = vec3(0,1,0);
        hit.color = texture(u_floorTex, hit.p.xz * 0.1).rgb;
        hit.emi = vec3(0); hit.rough = 1.0; hit.ior = 0.0;
    }

    // 2. СВЕТЯЩАЯСЯ СФЕРА (Лампа)
    vec3 lp = vec3(-4, 3, -6);
    vec3 oc = ro - lp;
    float b = dot(oc, rd), c = dot(oc, oc) - 2.0; 
    float h = b*b - c;
    if(h > 0.0) {
        float t = -b - sqrt(h);
        if(t > 0.001 && t < hit.t) {
            hit.t = t; hit.p = ro + rd*t; hit.n = normalize(hit.p - lp);
            hit.emi = vec3(12, 10, 7); hit.color = vec3(1); hit.ior = 0;
        }
    }

    // 3. СТЕКЛО
    vec3 gp = vec3(0, 0, -4);
    oc = ro - gp; b = dot(oc, rd); c = dot(oc, oc) - 1.0;
    h = b*b - c;
    if(h > 0.0) {
        float t = -b - sqrt(h);
        if(t > 0.001 && t < hit.t) {
            hit.t = t; hit.p = ro + rd*t; hit.n = normalize(hit.p - gp);
            hit.color = vec3(1); hit.emi = vec3(0); hit.ior = 1.5; hit.rough = 0;
        }
    }

    // 4. ГЛЯНЦЕВОЕ ЗЕРКАЛО
    vec3 mp = vec3(3, 0, -3);
    oc = ro - mp; b = dot(oc, rd); c = dot(oc, oc) - 1.2;
    h = b*b - c;
    if(h > 0.0) {
        float t = -b - sqrt(h);
        if(t > 0.001 && t < hit.t) {
            hit.t = t; hit.p = ro + rd*t; hit.n = normalize(hit.p - mp);
            hit.color = vec3(0.8, 0.9, 1.0); hit.emi = vec3(0); hit.ior = 0; hit.rough = 0.02;
        }
    }

    // 5. ЛОГОТИП
    vec3 qp = ro - u_logoPos;
    float sn = sin(-u_logoRot), cs = cos(-u_logoRot);
    vec3 r_ro = vec3(qp.x * cs - qp.z * sn, qp.y, qp.x * sn + qp.z * cs);
    vec3 r_rd = vec3(rd.x * cs - rd.z * sn, rd.y, rd.x * sn + rd.z * cs);
    float tq = -r_ro.z / r_rd.z;
    if(tq > 0.001 && tq < hit.t) {
        vec3 p = r_ro + r_rd * tq;
        if(abs(p.x) < 1.0 && abs(p.y) < 1.0) {
            vec4 tex = texture(u_logoTex, p.xy * 0.5 + 0.5);
            if(tex.a > 0.5) {
                hit.t = tq; hit.p = ro + rd*tq; hit.n = vec3(sn, 0, cs);
                hit.color = tex.rgb; hit.emi = vec3(0); hit.ior = 0; hit.rough = 0.9;
            }
        }
    }
}

vec3 trace(vec3 ro, vec3 rd) {
    vec3 col = vec3(0), mask = vec3(1);
    for(int i=0; i<5; i++) {
        Hit hit; hit.t = 1e20;
        checkScene(ro, rd, hit);
        if(hit.t > 1e19) {
            // Светлое небо
            float skyT = 0.5 * (rd.y + 1.0);
            col += mask * mix(vec3(0.5, 0.7, 1.0), vec3(1.0), skyT) * 0.5;
            break;
        }
        col += mask * hit.emi;
        vec3 p = hit.p + hit.n * 0.001; 
        if(hit.ior > 0.0) {
            float fresnel = 0.04 + 0.96 * pow(1.0 + dot(rd, hit.n), 5.0);
            if(rand() < fresnel) rd = reflect(rd, hit.n);
            else {
                bool outSide = dot(rd, hit.n) < 0.0;
                rd = refract(rd, outSide ? hit.n : -hit.n, outSide ? 1.0/1.5 : 1.5);
                p = hit.p - hit.n * 0.002;
            }
        } else {
            rd = mix(reflect(rd, hit.n), cosWeightedRandom(hit.n), hit.rough);
            mask *= hit.color;
        }
        ro = p;
    }
    return col;
}

void main() {
    seed = uint(gl_FragCoord.x) * 1973u + uint(gl_FragCoord.y) * 9277u + uint(u_seed1.x * 1000.0) * 26699u;
    vec2 uv = (TexCoords * 2.0 - 1.0) * vec2(u_resolution.x/u_resolution.y, 1.0);
    vec3 rd = mat3(inverse(u_view)) * normalize(vec3(uv, -1.5));
    vec3 color = trace(u_pos, rd);
    vec3 last = texture(u_sample, TexCoords).rgb;
    FragColor = vec4(mix(last, color, u_sample_part), 1.0);
}