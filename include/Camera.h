#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    float Yaw;
    float Pitch;
    float Speed = 15.0f;
    float Sensitivity = 0.1f;

    Camera(glm::vec3 position) : Position(position), Yaw(-90.0f), Pitch(0.0f) {
        updateCameraVectors();
    }

    glm::mat4 GetViewMatrix() {
        return glm::lookAt(Position, Position + Front, Up);
    }

    void ProcessKeyboard(int direction, float deltaTime) {
        float velocity = Speed * deltaTime;
        if (direction == 1) Position += Front * velocity; // W
        if (direction == 2) Position -= Front * velocity; // S
        if (direction == 3) Position -= glm::normalize(glm::cross(Front, Up)) * velocity; // A
        if (direction == 4) Position += glm::normalize(glm::cross(Front, Up)) * velocity; // D
    }

    void ProcessMouseMovement(float xoffset, float yoffset) {
        xoffset *= Sensitivity;
        yoffset *= Sensitivity;

        Yaw += xoffset;
        Pitch += yoffset;

        if (Pitch > 89.0f) Pitch = 89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;

        updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        Up = glm::vec3(0.0f, 1.0f, 0.0f);
    }
};
#endif