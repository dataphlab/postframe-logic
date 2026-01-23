#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Shader.h"
#include "Texture.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Framebuffer.h"

#include "Camera.h"

Camera camera(glm::vec3(0.0f, 2.0f, 6.0f));
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float lastX = 640, lastY = 360;
bool firstMouse = true;

void defaultTheme() {
ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- ГЕОМЕТРИЯ ---
    style.WindowRounding    = 0.0f;
    style.FrameRounding     = 2.0f;
    style.PopupRounding     = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding      = 2.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;

    // --- ЦВЕТОВАЯ ПАЛИТРА ---
    
    // Текст
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Фоновые элементы
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Поля ввода
    colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);

    // Заголовки
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

    // Кнопки
    colors[ImGuiCol_Button]                 = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(1.00f, 1.00f, 1.00f, 0.15f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);

    // Слайдеры и прокрутка
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    // Разное
    colors[ImGuiCol_Separator]              = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Path Tracing Engine", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    if (!gladLoadGL(glfwGetProcAddress)) return -1;

    // Инициализация ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // Ресурсы
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO); glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    Shader ptShader("assets/shaders/screen_v.glsl", "assets/shaders/pt_fragment.glsl");
    Shader screenShader("assets/shaders/screen_v.glsl", "assets/shaders/screen_f.glsl");
    
    Texture logoTex("assets/program_base/logo-bg.png", true); 
    Texture floorTex("assets/base_tex.png", false);

    // 4. Настройка накопительных буферов Ping-Pong
    Framebuffer fb1(1280, 720);
    Framebuffer fb2(1280, 720);
    Framebuffer* prevFB = &fb1;
    Framebuffer* currFB = &fb2;

    glm::vec3 lastPos = camera.Position;
    glm::mat4 lastView = camera.GetViewMatrix();
    float accumulationFrame = 1.0f;

    defaultTheme();

    // --- ПОДГОТОВКА ПЕРЕД ЦИКЛОМ ---
    float logoRotation = 0.0f;

    // Текстуры для Path Tracer
    floorTex.bind(1);
    logoTex.bind(2);

    float logoRot = 0.0f;
    glm::vec3 lastCamPos = camera.Position;
    glm::mat4 lastCamView = camera.GetViewMatrix();

    // Состояние
    bool firstMouse = true;

    float lastFrame = 0.0f;
    float lastX = 400, lastY = 300; // Центр экрана

    bool isPaused = false;
    bool pPressed = false;

    int targetFPS = 60;

    int maxSamplesPerFrame = 64;

    while (!glfwWindowShouldClose(window)) {
        float frameStartTime = (float)glfwGetTime();
        float deltaTime = frameStartTime - lastFrame;
        lastFrame = frameStartTime;

        glfwPollEvents();
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        float frameBudget = 1.0f / (float)targetFPS;

        bool moved = false;

        // --- УПРАВЛЕНИЕ КАМЕРОЙ ---
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { camera.ProcessKeyboard(1, deltaTime); moved = true; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { camera.ProcessKeyboard(2, deltaTime); moved = true; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { camera.ProcessKeyboard(3, deltaTime); moved = true; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { camera.ProcessKeyboard(4, deltaTime); moved = true; }

        // --- УПРАВЛЕНИЕ МЫШЬЮ ---
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);

            if (firstMouse) {
                lastX = (float)xpos;
                lastY = (float)ypos;
                firstMouse = false;
            }

            float xoffset = (float)xpos - lastX;
            float yoffset = lastY - (float)ypos;

            lastX = (float)xpos;
            lastY = (float)ypos;

            if (abs(xoffset) > 0.05f || abs(yoffset) > 0.05f) {
                camera.ProcessMouseMovement(xoffset, yoffset);
                moved = true;
            }
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            firstMouse = true;
        }

        // --- ПАУЗА ВРАЩЕНИЯ (Клавиша P) ---
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS && !pPressed) {
            isPaused = !isPaused;
            pPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_P) == GLFW_RELEASE) pPressed = false;

        float oldRotation = logoRotation;

        if (!isPaused) {
            logoRotation += deltaTime * 0.8f;
        }

        if (glm::length(camera.Position - lastCamPos) > 0.01f || abs(logoRotation - oldRotation) > 0.001f) {
            moved = true;
            lastCamPos = camera.Position; 
        }

        if (moved) accumulationFrame = 1.0f;

        // --- РЕНДЕР ПАСС ---
        ptShader.use();
        ptShader.setVec2("u_resolution", glm::vec2((float)width, (float)height));
        ptShader.setVec3("u_pos", camera.Position);
        ptShader.setMat4("u_view", camera.GetViewMatrix());
        ptShader.setVec3("u_logoPos", glm::vec3(0.0f, 0.0f, -2.0f));
        ptShader.setFloat("u_logoRot", logoRotation);

        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, logoTex.ID);
        ptShader.setInt("u_logoTex", 1);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, floorTex.ID);
        ptShader.setInt("u_floorTex", 2);

        int samplesThisFrame = 0;
        do {
            currFB->bind();
            glViewport(0, 0, width, height);

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

        } while ((glfwGetTime() - frameStartTime) < (frameBudget - 0.001f) && samplesThisFrame < maxSamplesPerFrame);

        // --- ВЫВОД НА ЭКРАН ---
        glViewport(0, 0, width, height);
        glClear(GL_COLOR_BUFFER_BIT);
        screenShader.use(); 
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, prevFB->textureColor);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // --- GUI ---
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Settings");
        ImGui::SliderInt("Target FPS", &targetFPS, 10, 240);
        ImGui::SliderInt("Samples", &maxSamplesPerFrame, 1, 1024);
        ImGui::Text("Total Samples: %.0f", accumulationFrame);
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);

        // Ограничение FPS
        while ((glfwGetTime() - frameStartTime) < frameBudget) {
            // Просто ждем
        }
    }

    glfwTerminate();
    return 0;
}