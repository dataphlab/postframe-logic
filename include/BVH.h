#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "GPUMeshTriangle.h"

struct GPUBVHNode {
    glm::vec3 minBounds; int leftFirst;
    glm::vec3 maxBounds; int triCount;
};

struct GPUMeshObject {
    glm::vec3 minAABB; float pad1;
    glm::vec3 maxAABB; float pad2;
    int bvhRootIndex;
    int pad3; int pad4; int pad5;
};

extern std::vector<GPUBVHNode> allBVHNodes;
extern std::vector<GPUMeshObject> allObjects;

void UpdateNodeBounds(int nodeIdx, std::vector<GPUBVHNode>& nodes, const std::vector<GPUMeshTriangle>& tris);
void Subdivide(int nodeIdx, std::vector<GPUBVHNode>& nodes, std::vector<GPUMeshTriangle>& tris);