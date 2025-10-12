#ifndef PLAYER_H
#define PLAYER_H

#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>

#include <classes/GLshader.h>

#include <iostream>

class PlayerController
{
private:
    double oldMouseX;
    double oldMouseY;
    float yaw = -1.571f; // initial yaw, looking along negative z
    float pitch = 0.0f;
    // adjust sensitivity as needed
    float sensitivity = 0.001f;
public:
    // up direction
    float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
    // move speed
    float speed = 1.0;
    // properties
    float posX;
    float posY;
    float posZ;
    float dirX;
    float dirY;
    float dirZ;

    PlayerController(GLFWwindow *window) {
        posX = 3.5;
        posY = 3.5;
        posZ = 3.5;
    }

    void HandleInputs(GLFWwindow *window, float deltaTime) {
        // forward from yaw/pitch
        float forwardX = cosf(yaw); //* cosf(pitch);
        float forwardY = 0.0f;
        float forwardZ = sinf(yaw); //* cosf(pitch);
    
        float len = sqrtf(forwardX*forwardX + forwardY*forwardY + forwardZ*forwardZ);
        forwardX /= len;
        forwardY /= len;
        forwardZ /= len;

        // right = normalize(cross(forward, up))
        float rightX = forwardZ*upY - forwardY*upZ;   // f × u
        float rightY = forwardX*upZ - forwardZ*upX;
        float rightZ = forwardY*upX - forwardX*upY;

        float rlen = sqrtf(rightX*rightX + rightY*rightY + rightZ*rightZ);
        rightX /= rlen;
        rightY /= rlen;
        rightZ /= rlen;

        // forward/back
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            posX += forwardX * speed * deltaTime;
            posY += forwardY * speed * deltaTime;
            posZ += forwardZ * speed * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            posX -= forwardX * speed * deltaTime;
            posY -= forwardY * speed * deltaTime;
            posZ -= forwardZ * speed * deltaTime;
        }

        // strafe
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            posX -= rightX * speed * deltaTime;
            posY -= rightY * speed * deltaTime;
            posZ -= rightZ * speed * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            posX += rightX * speed * deltaTime;
            posY += rightY * speed * deltaTime;
            posZ += rightZ * speed * deltaTime;
        }

        // up/down
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            posX += upX * speed * deltaTime;
            posY += upY * speed * deltaTime;
            posZ += upZ * speed * deltaTime;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            posX -= upX * speed * deltaTime;
            posY -= upY * speed * deltaTime;
            posZ -= upZ * speed * deltaTime;
        }
    }

    void HandleMouseInput(GLFWwindow *window) {
        // delta mouse movement
        double mousePosX;
        double mousePosY;
        double mouseDeltaX;
        double mouseDeltaY;
        int inputMode;
        if (glfwGetInputMode(window,GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
            glfwGetCursorPos(window, &mousePosX, &mousePosY);
            mouseDeltaX = mousePosX - oldMouseX;
            mouseDeltaY = mousePosY - oldMouseY;
            oldMouseX = mousePosX;
            oldMouseY = mousePosY;
        } else {
            mouseDeltaX = 0.0;
            mouseDeltaY = 0.0;
        }

        // update pitch and yaw
        mouseDeltaX *= sensitivity;
        mouseDeltaY *= sensitivity;
        yaw -= mouseDeltaX;
        pitch -= mouseDeltaY;

        // clamp pitch to prevent flipping
        if (pitch > 1.57f) pitch = 1.57f;
        if (pitch < -1.57f) pitch = -1.57f;

        // trigonometry to project to vector direction
        dirX = cos(yaw) * cos(pitch);
        dirY = sin(pitch);
        dirZ = sin(yaw) * cos(pitch);
    }
};

#endif