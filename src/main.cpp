#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <classes/GLshader.h>
#include <classes/PlayerController.h>

#include <iostream>
#include <array>
#include <string>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

Shader* screenPtr;

// settings
unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 600;

const uint32_t AXIS_SIZE = 1024;
const uint32_t CHUNK_SIZE = 8;
const uint32_t NUM_VOXELS = AXIS_SIZE * AXIS_SIZE * AXIS_SIZE;
const uint32_t NUM_VUINTS = (NUM_VOXELS + 3) / 4; // ceil division, amount of uints total.
// because morton is recursive, chunks will always fit within cube.
const uint32_t AXIS_CHUNKS = (AXIS_SIZE + 3) / CHUNK_SIZE; // every 2*4 uints forms a 4^3 chunk that can be bitmasked.

const uint32_t NUM_CUINTS = AXIS_CHUNKS*AXIS_CHUNKS*AXIS_CHUNKS/32; 

// sizes
const size_t SSBO0_SIZE = sizeof(GLuint) * (NUM_VUINTS + NUM_CUINTS);

int main() {
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    std::cout<<NUM_CUINTS<<" cuints needed."<<std::endl; // prints amount of uints for chunk buffer.
    // glfw window creation
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Pundus", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_STICKY_KEYS, 1);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // initialize player
    PlayerController Player(window);
    // hide mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // build and compile shader program
    Shader ScreenShader("shaders/4.3.screenquad.vert","shaders/4.3.raymarcher.frag");
    Shader TerrainShader("shaders/4.3.terrain.comp");
    Shader ChunkMask("shaders/4.3.chunkmask.comp");
    screenPtr = &ScreenShader; // pointer for screen resizing

    // vaos need to be bound because of biolerplating shizzle (even if not used)
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    //construct main buffer (voxel data)
    GLuint ssbo0;
    glGenBuffers(1, &ssbo0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo0);
    glBufferData(GL_SHADER_STORAGE_BUFFER, SSBO0_SIZE, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo0); // very important, don't forget, deleted accidentally once and could not figure out what was going wrong for like an hour.

    // generate terrain
    TerrainShader.use();

    // dispatch compute shader threads, based on thread pool size of 64.
    glDispatchCompute(AXIS_SIZE/4, AXIS_SIZE/4, AXIS_SIZE/4);

    // make sure writes are visible to everything else CHECK IF NECESSARY
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // generate terrain
    ChunkMask.use();

    // dispatch compute shader threads, based on thread pool size of 64.
    glDispatchCompute(AXIS_CHUNKS/4, AXIS_CHUNKS/4, AXIS_CHUNKS/4);

    // make sure writes are visible to everything else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // render loop
    float deltaTime = 0.0f;
    float lastTime = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        // delta time
        float currentTime = float(glfwGetTime());
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;
        // process input
        Player.HandleInputs(window, deltaTime);
        Player.HandleMouseInput(window);
        // upload player data to GPU
        ScreenShader.setFloat("iTime", currentTime);
        ScreenShader.setFloat("pPosX", Player.posX);
        ScreenShader.setFloat("pPosY", Player.posY);
        ScreenShader.setFloat("pPosZ", Player.posZ);
        ScreenShader.setFloat("pDirX", Player.dirX);
        ScreenShader.setFloat("pDirY", Player.dirY);
        ScreenShader.setFloat("pDirZ", Player.dirZ);
        processInput(window);

        // render
        glClear(GL_COLOR_BUFFER_BIT);
        
        // compute shaders go here

        // render the screen
        ScreenShader.use();
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // glfw: swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    std::cout << "\nFPS: " << 1.0/deltaTime; // print fps at time of closing. Good for benchmarking as prints don't slow things down.
    // glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glfwSetCursorPos(window, 0.0,0.0);
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        glfwFocusWindow(window);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    Shader ScreenShader = *screenPtr;
    std::cout<<"resized to: "<<width<<", "<<height<<std::endl;
    // resize window when necessary
    ScreenShader.setInt("screenWidth", width);
    ScreenShader.setInt("screenHeight", height);
    glViewport(0, 0, width, height);
}