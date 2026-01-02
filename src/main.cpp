#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <classes/GLshader.h>
#include <classes/PlayerController.h>

#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <cmath>

// functions
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void processPlayer(PlayerController Player, Shader lowRes, Shader highRes);

// pointers
Shader* lowResPtr;
Shader* highResPtr;

GLuint coarseFBO; // FBO for low resolution
GLuint coarseTex; // result of low res pass

GLuint prePassTex; // prepass texture

// settings
const uint32_t AXIS_SIZE = 1023;
const uint32_t PASS_RES = 4;
const uint32_t NUM_VOXELS = AXIS_SIZE * AXIS_SIZE * AXIS_SIZE;
const uint32_t NUM_VUINTS = (NUM_VOXELS + 3) / 4; // ceil division, amount of uints total.
const uint32_t NUM_GUINTS = (NUM_VUINTS)/(PASS_RES*PASS_RES*PASS_RES); // uints per group for low res pass.

unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 600;
unsigned int PRE_WIDTH = SCR_WIDTH/PASS_RES;
unsigned int PRE_HEIGHT = SCR_HEIGHT/PASS_RES;
unsigned int AO_DIAMETER = 5;
unsigned int AO_CELLS = (AO_DIAMETER+1)*(AO_DIAMETER+1)*((AO_DIAMETER+1)/2);

// sizes
const size_t SSBO0_SIZE = sizeof(GLuint) * (NUM_VUINTS);
const size_t SSBO1_SIZE = sizeof(GLuint) * (NUM_GUINTS);
const size_t SSBO2_SIZE = sizeof(GLuint) + sizeof(GL_INT_VEC3)*6*AO_CELLS; // cells amount, plus rectangle of 

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
    Shader terrainShader("shaders/4.3.terrain.comp");
    Shader terrainMaskShader("shaders/4.3.terrainmask.comp");
    Shader lowResShader("shaders/4.3.lowrespass.comp");
    Shader highResShader("shaders/4.3.screenquad.vert","shaders/4.3.highrespass.frag");
    Shader blockEditShader("shaders/4.3.blockeditor.comp");
    Shader precomputesShader("shaders/4.3.precomputes.comp");
    lowResPtr = &lowResShader; // pointer for screen resizing
    highResPtr = &highResShader; // pointer for screen resizing

    // vaos need to be bound because of biolerplating shizzle (even if not used)
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // construct voxel data buffer.
    GLuint ssbo0;
    glGenBuffers(1, &ssbo0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo0);
    glBufferData(GL_SHADER_STORAGE_BUFFER, SSBO0_SIZE, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo0); // very important, don't forget, deleted accidentally once and could not figure out what was going wrong for like an hour.

    // occupancy mask data buffer.
    GLuint ssbo1;
    glGenBuffers(1, &ssbo1);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo1);
    glBufferData(GL_SHADER_STORAGE_BUFFER, SSBO1_SIZE, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo1); // very important, don't forget, deleted accidentally once and could not figure out what was going wrong for like an hour.

    // construct precomputes buffer. currently holds AO cell generation.
    GLuint ssbo2;
    glGenBuffers(1, &ssbo2);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo2);
    glBufferData(GL_SHADER_STORAGE_BUFFER, SSBO2_SIZE, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssbo2); // very important, don't forget, deleted accidentally once and could not figure out what was going wrong for like an hour.

    // prepass texture (prepass depth data) combined with shadow map texture
    glGenTextures(1, &prePassTex);
    glBindTexture(GL_TEXTURE_2D, prePassTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, PRE_WIDTH, PRE_HEIGHT);
    glBindImageTexture(0, prePassTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // precompute AO hemispheres.
    precomputesShader.use();
    precomputesShader.setInt("AOdiameter",AO_DIAMETER);
    glDispatchCompute(1, 1, 1);

    // sets AOchange in shader.
    highResShader.use();
    highResShader.setFloat("AOchange",2.0/float(AO_CELLS));

    // make sure writes are visible to everything else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // generate terrain
    terrainShader.use();

    // dispatch compute shader threads, based on thread pool size of 64.
    glDispatchCompute(AXIS_SIZE/4, AXIS_SIZE/4, AXIS_SIZE/4);

    // make sure writes are visible to everything else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // generate terrain
    terrainMaskShader.use();

    // dispatch compute shader threads, based on thread pool size of 64.
    glDispatchCompute((AXIS_SIZE)/(4*PASS_RES), (AXIS_SIZE)/(4*PASS_RES), (AXIS_SIZE)/(4*PASS_RES));

    // make sure writes are visible to everything else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // render loop
    float deltaTime = 0.0f;
    float lastTime = 0.0f;
    int lastClick = 0;

    while (!glfwWindowShouldClose(window))
    {
        // delta time
        float currentTime = float(glfwGetTime());
        deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        Player.HandleInputs(window, deltaTime);
        Player.HandleMouseInput(window);
        processInput(window);

        // block editing.
        
        if (Player.click != 0 && lastClick != Player.click) {
            blockEditShader.use();
            blockEditShader.setBool("click", (Player.click==1));
            blockEditShader.setFloat("pPosX", Player.posX);
            blockEditShader.setFloat("pPosY", Player.posY);
            blockEditShader.setFloat("pPosZ", Player.posZ);
            blockEditShader.setFloat("pDirX", Player.dirX);
            blockEditShader.setFloat("pDirY", Player.dirY);
            blockEditShader.setFloat("pDirZ", Player.dirZ);
            
            glDispatchCompute(4, 4, 4);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            //std::cout<<(Player.click)<<std::endl;
        }
        lastClick = Player.click;

        // low res pass.
        lowResShader.use();
        lowResShader.setFloat("pPosX", Player.posX);
        lowResShader.setFloat("pPosY", Player.posY);
        lowResShader.setFloat("pPosZ", Player.posZ);
        lowResShader.setFloat("pDirX", Player.dirX);
        lowResShader.setFloat("pDirY", Player.dirY);
        lowResShader.setFloat("pDirZ", Player.dirZ);

        // dispatch low res compute shader threads, based on thread pool size of 64.
        glDispatchCompute((PRE_WIDTH+7)/8, (PRE_HEIGHT+7)/8, 1);

        // make sure writes are visible to everything else
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        // high res pass.
        //glBindFramebuffer(GL_FRAMEBUFFER, 0); // default framebuffer
    
        //glClear(GL_COLOR_BUFFER_BIT);

        highResShader.use();
        highResShader.setFloat("pPosX", Player.posX);
        highResShader.setFloat("pPosY", Player.posY);
        highResShader.setFloat("pPosZ", Player.posZ);
        highResShader.setFloat("pDirX", Player.dirX);
        highResShader.setFloat("pDirY", Player.dirY);
        highResShader.setFloat("pDirZ", Player.dirZ);
        highResShader.setFloat("iTime", currentTime);

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
    PRE_WIDTH = width/PASS_RES;
    PRE_HEIGHT = height/PASS_RES;
    
    std::cout<<"prepass at: "<<PRE_WIDTH<<", "<<PRE_HEIGHT<<std::endl;
    // make sure the viewport matches the new window dimensions; note that width and height will be significantly larger than specified on retina displays.

    Shader lowRes = *lowResPtr; // low res shader resize
    lowRes.use();
    lowRes.setInt("screenWidth", width);
    lowRes.setInt("screenHeight", height);

    Shader highRes = *highResPtr; // high res shader resize
    highRes.use();
    highRes.setInt("screenWidth", width);
    highRes.setInt("screenHeight", height);

    glViewport(0, 0, width, height); // resize viewport
    glGenTextures(1, &prePassTex); // resize prepass and lighting image
    glBindTexture(GL_TEXTURE_2D, prePassTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, PRE_WIDTH, PRE_HEIGHT);
}