#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/gl.h>
#include "stb_image.h"
#include <string>

class Texture {
public:
    unsigned int ID;
    Texture(const char* path, bool alpha = false);
    void bind(unsigned int unit = 0) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, ID);
    }
};
#endif