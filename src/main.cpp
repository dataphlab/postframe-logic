#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tiny_gltf.h" 

#include "Shader.h"
#include "Texture.h"

#include "themes.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Framebuffer.h"
#include "Camera.h"

#include <algorithm>
#include <vector>
#include <iostream>
#include <thread>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ---
Camera camera(glm::vec3(0.0f, 2.0f, 6.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float lastX = 640, lastY = 360;
bool firstMouse = true;

enum AppState {
    STATE_LAUNCHER,
    STATE_ENGINE,
    STATE_MAIN_APP
};

AppState currentState = STATE_LAUNCHER; 

char currentProjectName[128] = "Untitled Project";
ImFont* launcherFont = nullptr;

int loadMax = 6;

struct GPULight {
    glm::vec3 position;
    float radius;
    glm::vec3 emission;
    float pad; 
};

GLuint lightSSBO;
std::vector<GPULight> allLights;

// Функция для добавления света
void AddLight(glm::vec3 pos, float rad, glm::vec3 power) {
    allLights.push_back({pos, rad, power, 0.0f});
}

// --- СТРУКТУРЫ ДЛЯ МЕШЕЙ ---
struct GPUMeshTriangle {
    glm::vec3 v0; float pad1;
    glm::vec3 v1; float pad2;
    glm::vec3 v2; float pad3;
    glm::vec3 color; float pad4;
};

std::vector<GPUMeshTriangle> allTriangles;

// Функции лаунчера
void RenderLauncherGUI(GLFWwindow* window) {
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("PostFrame Launcher", nullptr, flags);

        // Заголовок
        float windowWidth = ImGui::GetWindowSize().x;
        float textWidth = ImGui::CalcTextSize("POSTFRAME LOGIC").x;
        
        ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
        ImGui::Text("POSTFRAME LOGIC");
        
        ImGui::Separator();
        ImGui::Spacing(); ImGui::Spacing();

        ImGui::Text("Select Mode:");
        ImGui::Spacing();

        // Кнопки делаем широкими
        float btnWidth = -FLT_MIN;

        if (ImGui::Button("ENGINE MODE", ImVec2(btnWidth, 60))) {
            currentState = STATE_ENGINE;
        }
        
        ImGui::Spacing();

        if (ImGui::Button("MAIN APP", ImVec2(btnWidth, 60))) {
            currentState = STATE_MAIN_APP;
        }

        float currY = ImGui::GetCursorPosY();
        float maxY = ImGui::GetWindowHeight() - 50.0f;
        if (currY < maxY) ImGui::SetCursorPosY(maxY);

        ImGui::Separator();
        
        // Кнопка Exit
        if (ImGui::Button("EXIT", ImVec2(btnWidth, 30))) {
            glfwSetWindowShouldClose(window, true);
        }

    ImGui::End();
}

// --- ФУНКЦИИ ЗАГРУЗКИ ---

// Рекурсивная функция для обхода дерева узлов
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

// --- BVH СТРУКТУРЫ ---

struct GPUBVHNode {
    glm::vec3 minBounds; int leftFirst;
    glm::vec3 maxBounds; int triCount;
};

std::vector<GPUBVHNode> allBVHNodes;

struct GPUMeshObject {
    glm::vec3 minAABB; float pad1;
    glm::vec3 maxAABB; float pad2;
    int bvhRootIndex;
    int pad3; int pad4; int pad5;
};

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

void LoadGLTF(const std::string& filename, glm::vec3 offset, float scale) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    bool ret = false;

    if (filename.find(".glb") != std::string::npos) ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    else ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!ret) { std::cout << "Failed: " << filename << std::endl; return; }

    // Временный вектор для сортировки треугольников
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

glm::vec3 meshMin(1e9), meshMax(-1e9);

void PrintGPUInfo() {
    std::cout << "\n================ GPU INFO ================" << std::endl;
    
    std::cout << "Vendor:       " << glGetString(GL_VENDOR) << std::endl;
    std::cout << "Renderer:     " << glGetString(GL_RENDERER) << std::endl;
    std::cout << "OpenGL Ver:   " << glGetString(GL_VERSION) << std::endl;
    std::cout << "GLSL Ver:     " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

    int maxTexSize;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    std::cout << "Max Tex Size: " << maxTexSize << "x" << maxTexSize << " px" << std::endl;

    int maxSSBOSize;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxSSBOSize);
    std::cout << "Max SSBO Size: " << (float)maxSSBOSize / 1024.0f / 1024.0f << " MB" << std::endl;

    int maxSSBOBindings;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxSSBOBindings);
    std::cout << "Max SSBO Slots: " << maxSSBOBindings << std::endl;

    GLint totalMemKb = 0;
    glGetIntegerv(0x9048, &totalMemKb);
    if (totalMemKb > 0) {
        std::cout << "Free VRAM:    " << totalMemKb / 1024 << " MB (NVIDIA Detect)" << std::endl;
    }

    std::cout << "==========================================\n" << std::endl;
}

// --- MAIN ---
int main() {
    int loadNow = 0;

    // Init GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    loadNow++;
    std::cout << "Init GLFW [" << loadNow << "/" << loadMax << "]" << std::endl;

    GLFWwindow* window = glfwCreateWindow(1280, 720, "PostFrame Logic V0.0.6", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    if (!gladLoadGL(glfwGetProcAddress)) return -1;

    loadNow++;
    std::cout << "Window Created [" << loadNow << "/" << loadMax << "]" << std::endl;

    PrintGPUInfo();

    // Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
    theme::defaultTheme();

    ImGuiIO& io = ImGui::GetIO();
    // Загружаем шрифт
    launcherFont = io.Fonts->AddFontFromFileTTF("assets/fonts/SpaceGrotesk-Regular.ttf", 16.0f);
    
    if (launcherFont == nullptr) {
        std::cout << "ERROR: Failed to load SpaceGrotesk font!" << std::endl;
        launcherFont = io.Fonts->AddFontDefault();
    }

    // Screen Quad
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f, -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f, -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,  1.0f,  1.0f,  1.0f, 1.0f
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // 4. Shaders & Textures
    Shader ptShader("assets/shaders/screen_v.glsl", "assets/shaders/pt_fragment.glsl");
    Shader screenShader("assets/shaders/screen_v.glsl", "assets/shaders/screen_f.glsl");
    Texture logoTex("assets/program_base/logo-bg.png", true); 
    Texture floorTex("assets/base_tex.png", false);

    LoadGLTF("assets/logo.glb", glm::vec3(-2.0f, 0.5f, 0.0f), 1.0f);
    LoadGLTF("assets/monkey.glb", glm::vec3(2.0f, -0.5f, 0.0f), 1.0f);

    if (allTriangles.empty()) {
        std::cout << "No GLTF loaded, using Test Pyramid." << std::endl;
        CreateTestPyramid();
    }

    loadNow++;
    std::cout << "Assets Loaded [" << loadNow << "/" << loadMax << "]" << std::endl;

    // Создаем SSBO и грузим данные в видеокарту
    GLuint meshSSBO;
    glGenBuffers(1, &meshSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, meshSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, allTriangles.size() * sizeof(GPUMeshTriangle), allTriangles.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, meshSSBO); // Binding = 2
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0); // Unbind

    loadNow++;
    std::cout << "Meshes Sent to GPU [" << loadNow << "/" << loadMax << "]" << std::endl;

    GLuint objectSSBO;
    glGenBuffers(1, &objectSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, objectSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, allObjects.size() * sizeof(GPUMeshObject), allObjects.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, objectSSBO); // Binding 3
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    GLuint bvhSSBO;
    glGenBuffers(1, &bvhSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, allBVHNodes.size() * sizeof(GPUBVHNode), allBVHNodes.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bvhSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Framebuffers
    int currentRenderW = 1280;
    int currentRenderH = 720;
    
    Framebuffer* fb1 = new Framebuffer(currentRenderW, currentRenderH);
    Framebuffer* fb2 = new Framebuffer(currentRenderW, currentRenderH);
    Framebuffer* prevFB = fb1;
    Framebuffer* currFB = fb2;

    // Переменные состояния
    float accumulationFrame = 1.0f;
    float logoRotation = 0.0f;
    glm::vec3 lastCamPos = camera.Position;
    
    bool isPaused = false;
    bool pPressed = false;
    int targetFPS = 60;
    int maxSamplesPerFrame = 64;
    float renderScalePercent = 75.0f; 
    bool useRayTracing = false; 

    // Биндим текстуры
    floorTex.bind(1);
    logoTex.bind(2);

    loadNow++;
    std::cout << "Ready to Render! [" << loadNow << "/" << loadMax << "]" << std::endl;

    bool showLearnWindow = true;

    AddLight(glm::vec3(-4, 3, -6), 1.0f, glm::vec3(30, 24, 18));  // Теплый свет
    AddLight(glm::vec3(5, 2, 0), 0.5f, glm::vec3(0, 20, 40));     // Синий акцент
    AddLight(glm::vec3(0, 10, -5), 2.0f, glm::vec3(4, 4, 4));    // Тусклый заполняющий сверху

    glGenBuffers(1, &lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, allLights.size() * sizeof(GPULight), allLights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, lightSSBO);

    loadNow++;
    std::cout << "Light created [" << loadNow << "/" << loadMax << "]" << std::endl;

    // --- MAIN LOOP ---
    while (!glfwWindowShouldClose(window)) {
        float frameStartTime = (float)glfwGetTime();
        deltaTime = frameStartTime - lastFrame;
        lastFrame = frameStartTime;

        glfwPollEvents();

        // ============================================================
        // 1. РЕЖИМ ЛАУНЧЕРА (МЕНЮ)
        // ============================================================
        if (currentState == STATE_LAUNCHER) {
            int w, h; glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
            glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            // Рисуем GUI
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            RenderLauncherGUI(window);
            
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glfwSwapBuffers(window);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        // ============================================================
        // 2. РЕЖИМ ДВИЖКА (ТЯЖЕЛЫЙ РЕНДЕР)
        // ============================================================

        int windowWidth, windowHeight;
        glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

        // --- ЛОГИКА РЕСАЙЗА И БУФЕРОВ ---
        int renderW = std::max(1, (int)(windowWidth * (renderScalePercent / 100.0f)));
        int renderH = std::max(1, (int)(windowHeight * (renderScalePercent / 100.0f)));

        if (renderW != currentRenderW || renderH != currentRenderH) {
            delete fb1; delete fb2;
            fb1 = new Framebuffer(renderW, renderH);
            fb2 = new Framebuffer(renderW, renderH);
            prevFB = fb1; currFB = fb2;
            currentRenderW = renderW; currentRenderH = renderH;
            accumulationFrame = 1.0f;
        }

        bool moved = false;

        float t = (float)glfwGetTime();
        float speed = 3.0f;
        float radius = 5.0f;
        float height = 3.0f;

        // Первый свет
        allLights[0].position.x = sin(t * speed) * radius;
        allLights[0].position.z = cos(t * speed) * radius - 5.0f;
        allLights[0].position.y = height;

        // Второй свет
        allLights[1].position.x = sin(t * speed + 3.1415f) * radius;
        allLights[1].position.z = cos(t * speed + 3.1415f) * radius - 5.0f;
        allLights[1].position.y = height;

        // Обновляем данные в видеокарте
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, allLights.size() * sizeof(GPULight), allLights.data());

        if (useRayTracing) accumulationFrame = 1.0f;

        // --- ВВОД ---
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { camera.ProcessKeyboard(1, deltaTime); moved = true; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { camera.ProcessKeyboard(2, deltaTime); moved = true; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { camera.ProcessKeyboard(3, deltaTime); moved = true; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { camera.ProcessKeyboard(4, deltaTime); moved = true; }

        // Захват мыши (ПКМ)
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
            float xoffset = (float)xpos - lastX; float yoffset = lastY - (float)ypos;
            lastX = (float)xpos; lastY = (float)ypos;
            if (abs(xoffset) > 0.05f || abs(yoffset) > 0.05f) { camera.ProcessMouseMovement(xoffset, yoffset); moved = true; }
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            firstMouse = true;
        }

        // Пауза вращения лого
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && !pPressed) { isPaused = !isPaused; pPressed = true; }
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) pPressed = false;

        float oldRotation = logoRotation;
        if (!isPaused) logoRotation += deltaTime * 0.8f;

        if (glm::length(camera.Position - lastCamPos) > 0.01f || abs(logoRotation - oldRotation) > 0.001f) {
            moved = true; lastCamPos = camera.Position; 
        }
        if (moved || !useRayTracing) accumulationFrame = 1.0f;

        // --- RENDER PASS (RAY TRACING) ---
        ptShader.use();
        ptShader.setVec2("u_resolution", glm::vec2((float)renderW, (float)renderH));
        ptShader.setVec3("u_pos", camera.Position);
        ptShader.setMat4("u_view", camera.GetViewMatrix());
        
        ptShader.setInt("u_sample", 0);
        ptShader.setInt("u_floorTex", 2);
        ptShader.setInt("u_useRayTracing", useRayTracing ? 1 : 0);

        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, logoTex.ID);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, floorTex.ID);

        // Биндинг SSBO
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, meshSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, objectSSBO);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bvhSSBO);

        int samplesThisFrame = 0;
        float frameBudget = 1.0f / (float)targetFPS;

        // Цикл накопления сэмплов
        do {
            currFB->bind();
            glViewport(0, 0, renderW, renderH);

            glActiveTexture(GL_TEXTURE0); 
            glBindTexture(GL_TEXTURE_2D, prevFB->textureColor);
            ptShader.setInt("u_sample", 0);

            ptShader.setFloat("u_sample_part", 1.0f / accumulationFrame);
            ptShader.setVec2("u_seed1", glm::vec2((float)rand() / RAND_MAX, (float)rand() / RAND_MAX));

            glBindVertexArray(quadVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            currFB->unbind();

            std::swap(prevFB, currFB);
            accumulationFrame += 1.0f;
            samplesThisFrame++;

            if (!useRayTracing) break;

        } while ((glfwGetTime() - frameStartTime) < (frameBudget - 0.001f) && samplesThisFrame < maxSamplesPerFrame);

        // --- SCREEN PASS (Upscaling) ---
        glViewport(0, 0, windowWidth, windowHeight);
        glClear(GL_COLOR_BUFFER_BIT);
        
        screenShader.use(); 
        screenShader.setVec2("u_resolution", glm::vec2((float)windowWidth, (float)windowHeight));

        glActiveTexture(GL_TEXTURE0); 
        glBindTexture(GL_TEXTURE_2D, prevFB->textureColor);
        screenShader.setInt("screenTexture", 0);

        glBindVertexArray(quadVAO); 
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // --- GUI (ДВИЖОК) ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (currentState == STATE_ENGINE) {
            if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Learn", NULL, &showLearnWindow);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
        }

            ImGui::Begin("Engine Settings");
            
            // --- КНОПКА ВОЗВРАТА В МЕНЮ ---
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("<< Back to Launcher", ImVec2(-1, 0))) {
                currentState = STATE_LAUNCHER;
                // Сбрасываем курсор
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                firstMouse = true;
            }
            ImGui::PopStyleColor();
            ImGui::Separator();

            // ... Твои старые настройки ...
            float btnWidth4 = ImGui::GetContentRegionAvail().x / 4.0f - 5.0f;
            if (ImGui::Checkbox("ENABLE PATH TRACING", &useRayTracing)) accumulationFrame = 1.0f;
            ImGui::Separator();

            ImGui::Text("Global Presets");
            if (ImGui::Button("LOW", ImVec2(btnWidth4, 0))) { renderScalePercent = 50.0f; maxSamplesPerFrame = 8; accumulationFrame = 1.0f; } ImGui::SameLine();
            if (ImGui::Button("MID", ImVec2(btnWidth4, 0))) { renderScalePercent = 75.0f; maxSamplesPerFrame = 16; accumulationFrame = 1.0f; } ImGui::SameLine();
            if (ImGui::Button("HIGH", ImVec2(btnWidth4, 0))) { renderScalePercent = 100.0f; maxSamplesPerFrame = 32; accumulationFrame = 1.0f; } ImGui::SameLine();
            if (ImGui::Button("ULTRA", ImVec2(btnWidth4, 0))) { renderScalePercent = 125.0f; maxSamplesPerFrame = 64; accumulationFrame = 1.0f; }
            
            ImGui::Separator();
            ImGui::SliderInt("Target FPS", &targetFPS, 1, 240);
            ImGui::Separator();

            if (!useRayTracing) ImGui::BeginDisabled();
            ImGui::SliderInt("Samples", &maxSamplesPerFrame, 0, 1024);
            if (ImGui::Button("8 smp", ImVec2(btnWidth4, 0))) maxSamplesPerFrame = 8; ImGui::SameLine();
            if (ImGui::Button("16 smp", ImVec2(btnWidth4, 0))) maxSamplesPerFrame = 16; ImGui::SameLine();
            if (ImGui::Button("32 smp", ImVec2(btnWidth4, 0))) maxSamplesPerFrame = 32; ImGui::SameLine();
            if (ImGui::Button("64 smp", ImVec2(btnWidth4, 0))) maxSamplesPerFrame = 64;
            if (!useRayTracing) ImGui::EndDisabled();

            ImGui::Separator();
            if (ImGui::SliderFloat("Scale %", &renderScalePercent, 1.0f, 200.0f, "%.0f%%")) accumulationFrame = 1.0f;
            
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Res: %dx%d | Tris: %lu", renderW, renderH, allTriangles.size());
            
            float fps = ImGui::GetIO().Framerate;

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "FPS: %.1f", fps);

            ImGui::End();

            if (showLearnWindow) 
            {
                if (ImGui::Begin("Learn.", &showLearnWindow)) {

                ImGui::Text("THE BASICS");
                ImGui::Separator();
                ImGui::Separator();
                ImGui::Text("Change the graphics in the settings window");
                ImGui::Separator();
                ImGui::Text("Use the P-button to pause");

                }

                ImGui::End();
            }
        } 
        else if (currentState == STATE_MAIN_APP) {
            ImGui::Begin("Main Application");
            ImGui::Text("E");
            if (ImGui::Button("Back to Launcher")) {
                currentState = STATE_LAUNCHER;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        float timeToWait = frameBudget - (float)(glfwGetTime() - frameStartTime);
        if (timeToWait > 0.001f) {
            std::this_thread::sleep_for(std::chrono::milliseconds((int)(timeToWait * 1000)));
        }
        while ((glfwGetTime() - frameStartTime) < frameBudget) {}
    }

    delete fb1; delete fb2;
    glfwTerminate();
    return 0;
}