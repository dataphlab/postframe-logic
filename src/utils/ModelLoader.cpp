#define GLM_ENABLE_EXPERIMENTAL
#include "tiny_gltf.h"

#include "ModelLoader.h"
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "BVH.h"

std::vector<GPUMeshTriangle> allTriangles;

void ProcessNode(const tinygltf::Model& model, const tinygltf::Node& node, glm::mat4 currentTransform, std::vector<GPUMeshTriangle>& outTriangles) {
    
    // Вычисляем матрицу
    glm::mat4 localTransform = glm::mat4(1.0f);
    if (node.matrix.size() == 16) {
        float m[16];
        for (int i = 0; i < 16; i++) m[i] = (float)node.matrix[i];
        localTransform = glm::make_mat4(m);
    } else {
        if (node.translation.size() == 3) {
            localTransform = glm::translate(localTransform, glm::vec3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]));
        }
        if (node.rotation.size() == 4) {
            glm::quat q = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
            localTransform = localTransform * glm::mat4_cast(q);
        }
        if (node.scale.size() == 3) {
            localTransform = glm::scale(localTransform, glm::vec3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]));
        }
    }
    glm::mat4 globalTransform = currentTransform * localTransform;

    // Обрабатываем Меш
    if (node.mesh >= 0) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];

        for (const auto& primitive : mesh.primitives) {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) continue;

            // --- ЧТЕНИЕ МАТЕРИАЛА ---
            glm::vec3 meshColor = glm::vec3(0.8f); // Серый по умолчанию
            
            if (primitive.material >= 0) {
                const tinygltf::Material& mat = model.materials[primitive.material];
                // PBR Metallic Roughness -> Base Color Factor (RGBA)
                if (mat.pbrMetallicRoughness.baseColorFactor.size() == 4) {
                    meshColor.r = (float)mat.pbrMetallicRoughness.baseColorFactor[0];
                    meshColor.g = (float)mat.pbrMetallicRoughness.baseColorFactor[1];
                    meshColor.b = (float)mat.pbrMetallicRoughness.baseColorFactor[2];
                    // Alpha игнор
                }
            }

            const float* positionBuffer = nullptr;
            if (primitive.attributes.find("POSITION") != primitive.attributes.end()) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
                positionBuffer = reinterpret_cast<const float*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
            } else continue;

            if (primitive.indices >= 0) {
                const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
                const tinygltf::BufferView& bufferView = model.bufferViews[indexAccessor.bufferView];
                const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                for (size_t i = 0; i < indexAccessor.count; i += 3) {
                    int i0, i1, i2;
                    if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                        const unsigned short* buf = reinterpret_cast<const unsigned short*>(&buffer.data[bufferView.byteOffset + indexAccessor.byteOffset]);
                        i0 = buf[i]; i1 = buf[i+1]; i2 = buf[i+2];
                    } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        const unsigned int* buf = reinterpret_cast<const unsigned int*>(&buffer.data[bufferView.byteOffset + indexAccessor.byteOffset]);
                        i0 = buf[i]; i1 = buf[i+1]; i2 = buf[i+2];
                    } else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                        const unsigned char* buf = reinterpret_cast<const unsigned char*>(&buffer.data[bufferView.byteOffset + indexAccessor.byteOffset]);
                        i0 = buf[i]; i1 = buf[i+1]; i2 = buf[i+2];
                    } else continue;

                    auto getVert = [&](int index) {
                        glm::vec4 v = glm::vec4(positionBuffer[index*3], positionBuffer[index*3+1], positionBuffer[index*3+2], 1.0f);
                        return glm::vec3(globalTransform * v);
                    };

                    GPUMeshTriangle tri;
                    tri.v0 = getVert(i0);
                    tri.v1 = getVert(i1);
                    tri.v2 = getVert(i2);
                    tri.color = meshColor; // ПРИМЕНЯЕМ ЦВЕТ
                    
                    outTriangles.push_back(tri);
                }
            }
        }
    }

    // Дети
    for (int childIndex : node.children) {
        ProcessNode(model, model.nodes[childIndex], globalTransform, outTriangles);
    }
}

void LoadGLTF(const std::string& filename, glm::vec3 offset, float scale) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool ret = false;

    if (filename.find(".glb") != std::string::npos) ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    else ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!ret) { std::cout << "Failed: " << filename << std::endl; return; }

    std::vector<GPUMeshTriangle> localTris;

    glm::mat4 rootTransform = glm::mat4(1.0f);
    rootTransform = glm::translate(rootTransform, offset);
    rootTransform = glm::scale(rootTransform, glm::vec3(scale));

    const tinygltf::Scene& scene = model.scenes[model.defaultScene > -1 ? model.defaultScene : 0];
    for (int nodeIndex : scene.nodes) {
        ProcessNode(model, model.nodes[nodeIndex], rootTransform, localTris);
    }

    if (localTris.empty()) return;

    // --- СТРОИМ BVH ---
    GPUBVHNode rootNode;
    rootNode.leftFirst = 0;
    rootNode.triCount = localTris.size();
    
    int bvhStartIndex = allBVHNodes.size(); 
    allBVHNodes.push_back(rootNode);
    
    UpdateNodeBounds(bvhStartIndex, allBVHNodes, localTris);
    Subdivide(bvhStartIndex, allBVHNodes, localTris);

    // --- ОБЪЕДИНЯЕМ ---
    int globalTriOffset = allTriangles.size();
    
    // Сдвигаем индексы треугольников в узлах
    for (size_t i = bvhStartIndex; i < allBVHNodes.size(); i++) {
        if (allBVHNodes[i].triCount > 0) {
            allBVHNodes[i].leftFirst += globalTriOffset;
        }
    }

    allTriangles.insert(allTriangles.end(), localTris.begin(), localTris.end());

    GPUMeshObject obj;
    obj.minAABB = allBVHNodes[bvhStartIndex].minBounds;
    obj.maxAABB = allBVHNodes[bvhStartIndex].maxBounds;
    obj.bvhRootIndex = bvhStartIndex;
    
    allObjects.push_back(obj);

    std::cout << "Loaded: " << filename << " | Nodes: " << (allBVHNodes.size() - bvhStartIndex) << std::endl;
}

void CreateTestPyramid() {
    int startIndex = allTriangles.size();
    
    GPUMeshTriangle t; 

    t.color = glm::vec3(0.0f, 0.8f, 0.2f);
    t.v0 = glm::vec3(-1, 0, -3); 
    t.v1 = glm::vec3( 1, 0, -3); 
    t.v2 = glm::vec3( 0, 2, -3);
    allTriangles.push_back(t);

    t.color = glm::vec3(1.0f, 0.0f, 0.0f);
    t.v0 = glm::vec3( 1, 0, -3); 
    t.v1 = glm::vec3( 0, 0, -5); 
    t.v2 = glm::vec3( 0, 2, -3);
    allTriangles.push_back(t);
}