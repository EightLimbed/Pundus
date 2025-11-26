#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <classes/GLshader.h>
#include <classes/PlayerController.h>

#include <iostream>
#include <array>
#include <string>

// functions
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void processPlayer(PlayerController Player, Shader lowRes, Shader highRes);

// pointers
Shader* lowResPtr;
Shader* highResPtr;

GLuint coarseFBO; // FBO for low resolution
GLuint coarseTex; // result of low res pass

// settings
unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 600;

const uint32_t AXIS_SIZE = 1024;
const uint32_t NUM_VOXELS = AXIS_SIZE * AXIS_SIZE * AXIS_SIZE;
const uint32_t NUM_VUINTS = (NUM_VOXELS + 3) / 4; // ceil division, amount of uints total.

// sizes
const size_t SSBO0_SIZE = sizeof(GLuint) * (NUM_VUINTS);

int main() {
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
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
    Shader lowResShader("shaders/4.3.screenquad.vert","shaders/4.3.lowrespass.frag");
    Shader highResShader("shaders/4.3.screenquad.vert","shaders/4.3.highrespass.frag");
    Shader TerrainShader("shaders/4.3.terrain.comp");
    lowResPtr = &lowResShader; // pointer for screen resizing
    highResPtr = &highResShader; // pointer for screen resizing

    // vaos need to be bound because of biolerplating shizzle (even if not used)
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // texture result binding
    glGenFramebuffers(1, &coarseFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, coarseFBO);

    glGenTextures(1, &coarseTex);
    glBindTexture(GL_TEXTURE_2D, coarseTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, coarseTex, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);  // restore

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

        // input and uniforms
        Player.HandleInputs(window, deltaTime);
        Player.HandleMouseInput(window);
        //lowResShader.setFloat("iTime", currentTime);
        //highResShader.setFloat("iTime", currentTime);
        processPlayer(Player, lowResShader, highResShader);
        processInput(window);

        // low res pass
        glBindFramebuffer(GL_FRAMEBUFFER, coarseFBO);
        glViewport(0, 0, SCR_WIDTH/4, SCR_HEIGHT/4);

        glClear(GL_COLOR_BUFFER_BIT);

        lowResShader.use();
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // high res pass
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // default framebuffer
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

        glClear(GL_COLOR_BUFFER_BIT);

        //highResShader.use();

        // bind coarseTex to texture unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, coarseTex);
        highResShader.setInt("coarseTex", 0); // pointer to sampler

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // swap / poll
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    std::cout << "FPS: " << 1.0/deltaTime << std::endl; // print fps at time of closing. Good for benchmarking as prints don't slow things down.
    // glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();
    return 0;
}

void processPlayer(PlayerController Player, Shader lowRes, Shader highRes) {
    // low res
    lowRes.setFloat("pPosX", Player.posX);
    lowRes.setFloat("pPosY", Player.posY);
    lowRes.setFloat("pPosZ", Player.posZ);
    lowRes.setFloat("pDirX", Player.dirX);
    lowRes.setFloat("pDirY", Player.dirY);
    lowRes.setFloat("pDirZ", Player.dirZ);
    // high res
    highRes.setFloat("pPosX", Player.posX);
    highRes.setFloat("pPosY", Player.posY);
    highRes.setFloat("pPosZ", Player.posZ);
    highRes.setFloat("pDirX", Player.dirX);
    highRes.setFloat("pDirY", Player.dirY);
    highRes.setFloat("pDirZ", Player.dirZ);
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
    std::cout<<"resized to: "<<width<<", "<<height<<std::endl;
    SCR_WIDTH = width;
    SCR_HEIGHT = height;
    // make sure the viewport matches the new window dimensions; note that width and height will be significantly larger than specified on retina displays.
    Shader lowRes = *lowResPtr; // low res shader resize
    lowRes.setInt("screenWidth", width);
    lowRes.setInt("screenHeight", height);

    Shader highRes = *highResPtr; // high res shader resize
    highRes.setInt("screenWidth", width);
    highRes.setInt("screenHeight", height);

    glViewport(0, 0, width, height);
}