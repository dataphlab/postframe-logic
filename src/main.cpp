#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "Texture.h"
#include "LightSystem.h"
#include "ModelLoader.h"
#include "BVH.h"

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

using namespace lightsys;

#define RESET   "\033[0m"
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ---

Camera camera(glm::vec3(0.0f, 2.0f, 6.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float lastX = 640, lastY = 360;
bool firstMouse = true;

enum AppState {
    STATE_LAUNCHER,
    STATE_ENGINE,
    STATE_RENDER
};

AppState currentState = STATE_LAUNCHER; 

char currentProjectName[128] = "Untitled Project";
ImFont* launcherFont = nullptr;

int loadMax = 10;

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

        if (ImGui::Button("RENDER", ImVec2(btnWidth, 60))) {
            currentState = STATE_RENDER;
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

glm::vec3 meshMin(1e9), meshMax(-1e9);

void PrintGPUInfo() {
    std::cout <<  YELLOW << "\n================ GPU INFO ================" << std::endl;
    
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

    std::cout << "==========================================\n" << RESET << std::endl;
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

    GLFWwindow* window = glfwCreateWindow(1280, 720, "PostFrame Logic V0.0.62", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    if (!gladLoadGL(glfwGetProcAddress)) return -1;

    loadNow++;
    std::cout <<"Window Created [" << loadNow << "/" << loadMax << "]" << std::endl;

    PrintGPUInfo();

    // Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
    theme::defaultTheme();

    loadNow++;
    std::cout << "Init ImGUI [" << loadNow << "/" << loadMax << "]" << std::endl;

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

    loadNow++;
    std::cout << "Objects Sent to GPU [" << loadNow << "/" << loadMax << "]" << std::endl;

    GLuint bvhSSBO;
    glGenBuffers(1, &bvhSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvhSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, allBVHNodes.size() * sizeof(GPUBVHNode), allBVHNodes.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, bvhSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    loadNow++;
    std::cout << "BVH Sent to GPU [" << loadNow << "/" << loadMax << "]" << std::endl;


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
    std::cout << GREEN << "Ready to Render! [" << loadNow << "/" << loadMax << "]" << RESET << std::endl;

    bool showLearnWindow = true;

    AddLight(glm::vec3(-4, 3, -6), 1.0f, glm::vec3(30, 24, 18));  // Теплый свет
    AddLight(glm::vec3(5, 2, 0), 0.5f, glm::vec3(0, 20, 40));     // Синий акцент
    AddLight(glm::vec3(0, 10, -5), 2.0f, glm::vec3(4, 4, 4));    // Тусклый заполняющий сверху

    loadNow++;
    std::cout << "Light created [" << loadNow << "/" << loadMax << "]" << std::endl;

    glGenBuffers(1, &lightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, lightSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, allLights.size() * sizeof(GPULight), allLights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, lightSSBO);

    loadNow++;
    std::cout << "Lights Sent to GPU [" << loadNow << "/" << loadMax << "]" << RESET << std::endl;

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
            ImGui::SliderInt("Samples", &maxSamplesPerFrame, 1, 1024);
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
        else if (currentState == STATE_RENDER) {
            ImGui::Begin("RENDER");
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