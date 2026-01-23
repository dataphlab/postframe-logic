#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <glad/gl.h>

class Framebuffer {
public:
    unsigned int fbo;         // ID фреймбуфера
    unsigned int textureColor; // Текстура, куда рисуется кадр
    int width, height;

    Framebuffer(int width, int height);
    ~Framebuffer();

    void bind();   // Начать рисовать в этот буфер
    void unbind(); // Вернуться к обычному экрану
};

#endif