#include "LightSystem.h"

namespace lightsys {
    GLuint lightSSBO = 0; 
    std::vector<GPULight> allLights;

    void AddLight(glm::vec3 pos, float rad, glm::vec3 power) {
    allLights.push_back({pos, rad, power, 0.0f});
    }
}