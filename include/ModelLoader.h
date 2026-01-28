#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "GPUMeshTriangle.h"

extern std::vector<GPUMeshTriangle> allTriangles;

void LoadGLTF(const std::string& filename, glm::vec3 offset, float scale);

void CreateTestPyramid();