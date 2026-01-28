#pragma once

#include <glm/glm.hpp>
#include <glad/gl.h>
#include <vector>

namespace lightsys {
    struct GPULight {
    glm::vec3 position;
    float radius;
    glm::vec3 emission;
    float pad; 
};

extern GLuint lightSSBO;
extern std::vector<GPULight> allLights;

void AddLight(glm::vec3 pos, float rad, glm::vec3 power);
}