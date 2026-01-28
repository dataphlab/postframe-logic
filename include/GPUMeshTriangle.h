#pragma once
#include <glm/glm.hpp>

struct GPUMeshTriangle {
    glm::vec3 v0; float pad1;
    glm::vec3 v1; float pad2;
    glm::vec3 v2; float pad3;
    glm::vec3 color; float pad4;
};