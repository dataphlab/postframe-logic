#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    std::string vCode, fCode;
    std::ifstream vFile, fFile;
    vFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        vFile.open(vertexPath); fFile.open(fragmentPath);
        std::stringstream vStream, fStream;
        vStream << vFile.rdbuf(); fStream << fFile.rdbuf();
        vCode = vStream.str(); fCode = fStream.str();
    } catch (std::ifstream::failure& e) {
        std::cout << "ERROR::SHADER::FILE_NOT_READ: " << e.what() << std::endl;
    }
    const char* vShaderCode = vCode.c_str();
    const char* fShaderCode = fCode.c_str();

    unsigned int v, f;
    v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vShaderCode, NULL);
    glCompileShader(v);

    f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fShaderCode, NULL);
    glCompileShader(f);

    ID = glCreateProgram();
    glAttachShader(ID, v); glAttachShader(ID, f);
    glLinkProgram(ID);
    glDeleteShader(v); glDeleteShader(f);
}