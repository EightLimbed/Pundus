#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <classes/GLshader.h>
#include <classes/PlayerController.h>

#include <iostream>
#include <array>
#include <string>
#include <vector>
#include <cmath>
#include <random>

#include <filesystem>
namespace fs = std::filesystem;

// functions
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow *window);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processPlayer(PlayerController Player, Shader lowRes, Shader highRes);
void updateSettings();

// pointers
Shader* lowResPtr;
Shader* highResPtr;
Shader* screenPtr;

GLuint coarseFBO; // FBO for low resolution
GLuint coarseTex; // result of low res pass

GLuint prePassTex; // prepass texture
GLuint screenTex; // screen texture

// SETTINGS

// world
const unsigned int AXIS_SIZE = 1024;
const unsigned int PASS_RES = 4; // occupancy mask width, and step size of low res pass.
const unsigned int NUM_VOXELS = AXIS_SIZE * AXIS_SIZE * AXIS_SIZE;
const unsigned int NUM_VUINTS = (NUM_VOXELS + 3) / 4; // ceil division, amount of uints total.
const unsigned int NUM_GUINTS = (NUM_VUINTS)/(PASS_RES*PASS_RES*PASS_RES); // uints per group for low res pass.
const std::string worldsPath  = "./Worlds"; 

// screen
unsigned int SCR_WIDTH = 800;
unsigned int SCR_HEIGHT = 600;
float RES_MOD = 2.0;
unsigned int RES_WIDTH = int(float(SCR_WIDTH)/RES_MOD);
unsigned int RES_HEIGHT = int(float(SCR_HEIGHT)/RES_MOD);

unsigned int PRE_WIDTH = RES_WIDTH/PASS_RES;
unsigned int PRE_HEIGHT = RES_HEIGHT/PASS_RES;

float RENDER_DISTANCE = 768.0;

unsigned int AO_DIAMETER = 5;
unsigned int AO_SKIPPING = 2;
unsigned int AO_CELLS = (AO_DIAMETER+1)*(AO_DIAMETER+1)*((AO_DIAMETER+1)/2);

// physics
unsigned int SIM_AXIS_SIZE = 256; // only does x and z, physics simulated always vertically

// brushes
int brushSize = 1;

// buffer sizes
const size_t SSBO0_SIZE = sizeof(GLuint) * (NUM_VUINTS);
const size_t SSBO1_SIZE = sizeof(GLuint) * (NUM_GUINTS);
const size_t SSBO2_SIZE = 2*sizeof(GLuint) + sizeof(GL_INT_VEC3)*6*AO_CELLS; // cells amount, plus rectangle of 

int main() {
    // terminal loop.
    while (true) {
    // user input on startup.
    std::string userInput;

    std::cout << "To create world, type a name." << std::endl;
    std::cout << "Or type a world name to load:" << std::endl;
    std::vector<std::string> worldNames = {};

    try {
    // iterate over the entries in the directory
    for (const auto& entry : fs::directory_iterator(worldsPath)) {
        // check if the entry is a regular file and not a directory
        if (fs::is_regular_file(entry.status()) && (entry.path().extension() == ".pun")) {
            std::string str = entry.path().stem().string();
            std::cout << str << std::endl;
            worldNames.push_back(str);
        }
    }
    } catch (const fs::filesystem_error& ex) {
        std::cerr << "Error accessing Worlds directory: " << ex.what() << std::endl;
    }
    std::getline(std::cin, userInput); // read line of input

    // check if world exists.
    bool newWorld = true;
    for (const std::string& name : worldNames) {
        if (name == userInput) {
            newWorld = false;
            break;
        }
    }
    std::string worldFilePath = "Worlds/"+userInput+".pun";
    if (newWorld) std::cout << "Creating world: "<<worldFilePath<< std::endl;
    else std::cout << "Loading world: "<<worldFilePath<< std::endl;

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
    Shader physicsShader("shaders/4.3.physics.comp");
    Shader terrainMaskShader("shaders/4.3.terrainmask.comp");
    Shader precomputesShader("shaders/4.3.precomputes.comp");
    Shader lowResShader("shaders/4.3.lowrespass.comp");
    Shader highResShader("shaders/4.3.highrespass.comp");
    Shader blockEditShader("shaders/4.3.blockeditor.comp");
    Shader screenShader("shaders/4.3.screenquad.vert","shaders/4.3.screen.frag");
    lowResPtr = &lowResShader; // pointer for screen resizing
    highResPtr = &highResShader; // pointer for screen resizing
    screenPtr = &screenShader; // pointer for screen resizing

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
    
    updateSettings();

    // precompute AO hemispheres.
    precomputesShader.use();
    precomputesShader.setInt("AOdiameter",AO_DIAMETER);
    precomputesShader.setInt("AOskipping",AO_SKIPPING);
    glDispatchCompute(1, 1, 1);

    // make sure writes are visible to everything else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // if new world needed, create one, otherwise load file.
    if (newWorld) {
        // generate terrain
        terrainShader.use();

        // dispatch compute shader threads, based on thread pool size of 64.
        glDispatchCompute(AXIS_SIZE/4, AXIS_SIZE/4, AXIS_SIZE/4);

        // make sure writes are visible to everything else
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    } else { // load.
        std::vector<uint32_t> hostData(NUM_VUINTS);
        std::ifstream inFile(worldFilePath, std::ios::binary);
        inFile.read(reinterpret_cast<char*>(hostData.data()), SSBO0_SIZE);
        inFile.close();
        
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo0);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, SSBO0_SIZE, hostData.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    // generate terrain
    terrainMaskShader.use();

    // dispatch compute shader threads, based on thread pool size of 64. Second 4 is because only one thread per chunk is dispatched.
    glDispatchCompute((AXIS_SIZE)/(4*PASS_RES), (AXIS_SIZE)/(4*PASS_RES), (AXIS_SIZE)/(4*PASS_RES));

    // make sure writes are visible to everything else
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // random number setup
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 512); // multiple of 8

    // render loop
    float deltaTime = 0.0f;
    float lastTime = 0.0f;
    int lastClick = 0;
    int AOframeMod = 0;

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
            glfwSetScrollCallback(window, scroll_callback);

            blockEditShader.setBool("click", (Player.click==1));
            blockEditShader.setInt("brush", Player.brush);
            blockEditShader.setInt("brushSize", brushSize);
            blockEditShader.setFloat("pPosX", Player.posX);
            blockEditShader.setFloat("pPosY", Player.posY);
            blockEditShader.setFloat("pPosZ", Player.posZ);
            blockEditShader.setFloat("pDirX", Player.dirX);
            blockEditShader.setFloat("pDirY", Player.dirY);
            blockEditShader.setFloat("pDirZ", Player.dirZ);
            
            glDispatchCompute((brushSize+3)/4, (brushSize+3)/4, (brushSize+3)/4);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
        lastClick = Player.click;

        // physics pass.
        for (int i = 0; i < 1; i++) {
        physicsShader.use();
        auto random_number = dis(gen);
        physicsShader.setInt("random", random_number);
        //physicsShader.setFloat("iTime", currentTime);
        physicsShader.setInt("cPPosX", ((int(Player.posX)-SIM_AXIS_SIZE/2))/PASS_RES);
        physicsShader.setInt("cPPosZ", ((int(Player.posZ)-SIM_AXIS_SIZE/2))/PASS_RES);
        // first *4 is to fit in thread pool, second is to fit in chunk. Physics is done per chunk.
        glDispatchCompute(SIM_AXIS_SIZE/(4*PASS_RES), AXIS_SIZE/(4*PASS_RES), SIM_AXIS_SIZE/(4*PASS_RES));
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        // generate terrain
        terrainMaskShader.use();
        terrainMaskShader.setInt("cPPosX", ((int(Player.posX)-SIM_AXIS_SIZE/2))/PASS_RES);
        terrainMaskShader.setInt("cPPosZ", ((int(Player.posZ)-SIM_AXIS_SIZE/2))/PASS_RES);

        // dispatch compute shader threads, based on thread pool size of 64. Second 4 is because only one thread per chunk is dispatched.
        glDispatchCompute((SIM_AXIS_SIZE)/(4*PASS_RES), (AXIS_SIZE)/(4*PASS_RES), (SIM_AXIS_SIZE)/(4*PASS_RES));

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

        //glBindFramebuffer(GL_FRAMEBUFFER, 0); // default framebuffer
    
        //glClear(GL_COLOR_BUFFER_BIT);

        // calculates offset for AO frame skipping and physics ticks?.
        AOframeMod++; // increment framemod
        AOframeMod = AOframeMod % AO_SKIPPING;

        // high res pass.
        highResShader.use();
        highResShader.setInt("AOframeMod", AOframeMod);
        highResShader.setFloat("pPosX", Player.posX);
        highResShader.setFloat("pPosY", Player.posY);
        highResShader.setFloat("pPosZ", Player.posZ);
        highResShader.setFloat("pDirX", Player.dirX);
        highResShader.setFloat("pDirY", Player.dirY);
        highResShader.setFloat("pDirZ", Player.dirZ);
        highResShader.setFloat("iTime", 1.0);

        // dispatch high res compute shader threads, based on thread pool size of 64.
        glDispatchCompute((RES_WIDTH+7)/8, (RES_HEIGHT+7)/8, 1);

        // make sure writes are visible to everything else
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        // screen shader.
        screenShader.use(); // uses screen shader.
        screenShader.setFloat("iTime", currentTime);
        
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // swap / poll
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    std::cout << "FPS: " << 1.0/deltaTime << std::endl; // print fps at time of closing.

    // save world to worlds file.
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo0);
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, SSBO0_SIZE, GL_MAP_READ_BIT);
    if (ptr) {
        std::ofstream outFile(worldFilePath, std::ios::binary);
        outFile.write(reinterpret_cast<char*>(ptr), SSBO0_SIZE);
        outFile.close();
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); // Unmap after use
        std::cout<<"World saved to: "<<worldFilePath<<std::endl;
    } else {
        std::cout<<"failed to write"<<std::endl;
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();
    }
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetCursorPos(window, 0.0,0.0);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        glfwFocusWindow(window);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
}

void updateSettings() {
    RES_WIDTH = SCR_WIDTH/RES_MOD;
    RES_HEIGHT = SCR_HEIGHT/RES_MOD;
    PRE_WIDTH = RES_WIDTH/PASS_RES;
    PRE_HEIGHT = RES_HEIGHT/PASS_RES;

    Shader lowRes = *lowResPtr; // low res shader resize
    lowRes.use();
    lowRes.setInt("passWidth", PRE_WIDTH);
    lowRes.setInt("passHeight", PRE_HEIGHT);
    lowRes.setFloat("renderDist", RENDER_DISTANCE);

    Shader highRes = *highResPtr; // high res shader resize
    highRes.use();
    highRes.setInt("passWidth", RES_WIDTH);
    highRes.setInt("passHeight", RES_HEIGHT);
    highRes.setFloat("renderDist", RENDER_DISTANCE);

    Shader screen = *screenPtr; // screen shader resize.
    screen.use(); // uses screen shader.

    screen.setInt("screenWidth", SCR_WIDTH);
    screen.setInt("screenHeight", SCR_HEIGHT);
    glUniform1i(glGetUniformLocation(screen.ID, "screen"), 0);

    // resize textures
    // prepass texture (prepass depth data).
    glGenTextures(1, &prePassTex);
    glBindTexture(GL_TEXTURE_2D, prePassTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, PRE_WIDTH, PRE_HEIGHT);
    glBindImageTexture(0, prePassTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // screen texture (screen color data).
    glGenTextures(1, &screenTex);
    glBindTexture(GL_TEXTURE_2D, screenTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, RES_WIDTH, RES_HEIGHT);
    glBindImageTexture(1, screenTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    glUniform1i(glGetUniformLocation(screen.ID, "screen"), 0); // set sampler uniform.
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    if (yoffset > 0) {
        if (brushSize < 32) brushSize ++;
    } else {
        if (brushSize > 1) brushSize --;
    }
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    std::cout<<"screen resized to: "<<width<<", "<<height<<std::endl;

    // recalculates screen values.
    SCR_WIDTH = width;
    SCR_HEIGHT = height;

    std::cout<<"image resized to: "<<RES_WIDTH<<", "<<RES_HEIGHT<<std::endl;
    std::cout<<"low res pass resized to: "<<PRE_WIDTH<<", "<<PRE_HEIGHT<<std::endl;
    
    updateSettings(); // updates settings based on new values.

    // make sure the viewport matches the new window dimensions; note that width and height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height); // resize viewport
}