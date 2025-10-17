#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <classes/GLshader.h>
#include <classes/PlayerController.h>
#include <classes/PrefixConstructor.h>

#include <iostream>
#include <array>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

static constexpr int CHUNK_SIZE = 32;
static constexpr int NUM_VOXELS = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;
static constexpr int TERM_BITS  = 32;
static constexpr int NUM_TERMS  = NUM_VOXELS / TERM_BITS; // 32768 / 32 = 1024
static constexpr int PREFIX_BITS = 10; // 10-bit prefix values

int main()
{
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
    Shader ScreenShader("shaders/4.3.screenquad.vert","shaders/4.3.raymarcher.frag");
    Shader TerrainShader("shaders/4.3.terrain.comp");
    Shader DataShader("shaders/4.3.data.comp");

    // vaos need to be bound because of biolerplating shizzle (even if not used)
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // calculate buffer size: 32^3 chunk of single bits, with prefix array.
    size_t ssbo0Size = sizeof(GLuint)*(NUM_TERMS + NUM_TERMS + NUM_VOXELS); // 1024 for bit mask, 1024 for prefix array, more for voxels.

    //construct main buffer
    GLuint ssbo0;
    glGenBuffers(1, &ssbo0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo0);
    glBufferData(GL_SHADER_STORAGE_BUFFER, ssbo0Size, nullptr, GL_DYNAMIC_DRAW);

    // construct bitcloud array, in its own scope so it is cleared automatically.
    {
        std::array<uint32_t, NUM_TERMS> bitCloud = {};

        // bind bitcloud array
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(bitCloud.data()), bitCloud.data());
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo0);

    // generate terrain
    TerrainShader.use();

    // dispatch compute shader threads, based on thread pool size of 64.
    glDispatchCompute(CHUNK_SIZE/4, CHUNK_SIZE/4, CHUNK_SIZE/4);

    // make sure writes are visible to everything else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // initialize prefix constructor
    PrefixConstructor Prefixer;
    // generate and bind prefix array from bitcloud
    {
        std::array<uint32_t, NUM_TERMS> bitCloud; // should map this later
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GLuint)*NUM_TERMS, bitCloud.data());

        std::array<uint32_t, NUM_TERMS> Prefixes = Prefixer.GeneratePrefixes(bitCloud); // prefixes made from class
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(GLuint) * NUM_TERMS, sizeof(GLuint)*NUM_TERMS, Prefixes.data());
    }

    // generate block data
    DataShader.use();
    glDispatchCompute(CHUNK_SIZE/4, CHUNK_SIZE/4, CHUNK_SIZE/4);

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
void processInput(GLFWwindow *window)
{
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
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}