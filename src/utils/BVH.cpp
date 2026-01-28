#include "BVH.h"

std::vector<GPUBVHNode> allBVHNodes;
std::vector<GPUMeshObject> allObjects;

// Функция обновления AABB для узла
void UpdateNodeBounds(int nodeIdx, std::vector<GPUBVHNode>& nodes, const std::vector<GPUMeshTriangle>& tris) {
    GPUBVHNode& node = nodes[nodeIdx];
    node.minBounds = glm::vec3(1e9f);
    node.maxBounds = glm::vec3(-1e9f);

    for (int i = 0; i < node.triCount; i++) {
        const GPUMeshTriangle& leafTri = tris[node.leftFirst + i];
        node.minBounds = glm::min(node.minBounds, leafTri.v0);
        node.minBounds = glm::min(node.minBounds, leafTri.v1);
        node.minBounds = glm::min(node.minBounds, leafTri.v2);

        node.maxBounds = glm::max(node.maxBounds, leafTri.v0);
        node.maxBounds = glm::max(node.maxBounds, leafTri.v1);
        node.maxBounds = glm::max(node.maxBounds, leafTri.v2);
    }
}

// Рекурсивная функция разделения
void Subdivide(int nodeIdx, std::vector<GPUBVHNode>& nodes, std::vector<GPUMeshTriangle>& tris) {

    int nodeLeftFirst = nodes[nodeIdx].leftFirst;
    int nodeTriCount  = nodes[nodeIdx].triCount;
    glm::vec3 nodeMin = nodes[nodeIdx].minBounds;
    glm::vec3 nodeMax = nodes[nodeIdx].maxBounds;

    if (nodeTriCount <= 2) return;

    // Логика разделения
    glm::vec3 extent = nodeMax - nodeMin;
    int axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > extent[axis]) axis = 2;

    float splitPos = nodeMin[axis] + extent[axis] * 0.5f;

    int i = nodeLeftFirst;
    int j = i + nodeTriCount - 1;

    while (i <= j) {
        glm::vec3 centroid = (tris[i].v0 + tris[i].v1 + tris[i].v2) * 0.3333f;
        if (centroid[axis] < splitPos) {
            i++;
        } else {
            std::swap(tris[i], tris[j]);
            j--;
        }
    }

    int leftCount = i - nodeLeftFirst;
    if (leftCount == 0 || leftCount == nodeTriCount) return;

    // Создаем детей
    int leftChildIdx = nodes.size();
    nodes.push_back({});
    nodes.push_back({});
    
    // Обновляем текущий узел
    nodes[nodeIdx].leftFirst = leftChildIdx;
    nodes[nodeIdx].triCount = 0;

    // Настраиваем левого ребенка
    nodes[leftChildIdx].leftFirst = nodeLeftFirst;
    nodes[leftChildIdx].triCount = leftCount;
    UpdateNodeBounds(leftChildIdx, nodes, tris);
    
    // Настраиваем правого ребенка
    nodes[leftChildIdx + 1].leftFirst = i;
    nodes[leftChildIdx + 1].triCount = nodeTriCount - leftCount;
    UpdateNodeBounds(leftChildIdx + 1, nodes, tris);

    // Рекурсия
    Subdivide(leftChildIdx, nodes, tris);
    Subdivide(leftChildIdx + 1, nodes, tris);
}