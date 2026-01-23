#ifndef STARTUP_H
#define STARTUP_H

#include <iostream>
#include <filesystem>
#include <vector>
namespace fs = std::filesystem;
const std::string worldsPath  = "./Worlds"; 

class Menu
{
private:

public:
    bool newWorld = false;

    Menu(std::string* userInput, float* res, float* dist, unsigned int* sim, unsigned int* tick, unsigned int* diam, unsigned int* skip) {
        std::cout<<"\033[1m"<<"PUNDUS VOXEL ENGINE" <<"\033[0m"<<"\n"<<std::endl; // title
        while (true) {
        std::cout << "Type 'help' for a description and guide 'settings' for options, or 'worlds' to continue to worlds management." << "\n" << std::endl;
        std::cout<<"Input: ";
        std::getline(std::cin, *userInput); // read line of input
        if (*userInput == "help") {
            std::cout<<"\n\033[1m"<<"PUNDUS HELP" <<"\033[0m"<<"\n"<<std::endl; // title
            std::cout<<"Pundus is a voxel engine made with openGL and C++, with the purpose of enabling visualization and interaction with dynamic worlds."<<std::endl; // description
            std::cout<<"It features a 1024^3 voxel environment with raytraced lighting, along with a tunable custom ambient occlusion algorithm. There is a rudimentary building system, along with a cellular automata fluid physics engine."<<std::endl; // features
            std::cout<<"\nTo play, use WASD for movement in XZ plane, space to ascend, and shift to descend."<<std::endl; // how to play
            std::cout<<"Use left and right click to place and break, and scroll wheel to resize interaction."<<std::endl; // how to play
            std::cout<<"Other keys include number keys for changing block type, and P for toggling physics.\n"<<std::endl; // how to play
        } else if (*userInput == "settings") {
            while (true) {
            std::cout<<"\n\033[1m"<<"PUNDUS SETTINGS" <<"\033[0m"<<"\n"<<std::endl; // title
            std::cout<<"Settings should be tuned to balance the programs performance with effect for on your computer. Type their keyword to access them:\n"<<std::endl;
            std::cout<<"Resolution modifier: 'res'                 Currently at: "<<*res<<std::endl;
            std::cout<<"Render distance: 'dist'                    Currently at: "<<*dist<<std::endl;
            std::cout<<"Physics simulation distance: 'sim'         Currently at: "<<*sim<<std::endl;
            std::cout<<"Physics ticks per frame: 'tick'            Currently at: "<<*tick<<std::endl;
            std::cout<<"Ambient occlusion diameter: 'diam'         Currently at: "<<*diam<<std::endl;
            std::cout<<"Ambient occlusion frame skipping: 'skip'   Currently at: "<<*skip<<std::endl;
            std::cout<<"Exit settings: 'exit'"<<std::endl;
            std::cout<<"\nSetting: ";
            std::getline(std::cin, *userInput); // read line of input
            if (*userInput == "exit") {
                std::cout<<""<<std::endl;
                std::cout<<"\033[1m"<<"PUNDUS VOXEL ENGINE" <<"\033[0m"<<"\n"<<std::endl; // title
                break;
            } else if (*userInput == "res") {
                std::cout<<"The resolution modifier divides your screens resolution, lowering visual quality, but also greatly reducing rendering costs."<<std::endl;
                std::cout<<"\nValue: ";
                std::getline(std::cin, *userInput); // read line of input
                // attempt to set res mod from input.
                try {
                    *res = std::stof(*userInput);
                    std::cout << "\nResolution modifier set to: " << *res << std::endl;
                } catch (...) {
                    std::cout << "\nInvalid value." << std::endl;
                }
            } else if (*userInput == "dist") {
                std::cout<<"Render distance determines how far you can see."<<std::endl;
                std::cout<<"\nValue: ";
                std::getline(std::cin, *userInput); // read line of input
                // attempt to set res mod from input.
                try {
                    *dist = std::stof(*userInput);
                    std::cout << "\nResolution modifier set to: " << *dist << std::endl;
                } catch (...) {
                    std::cout << "\nInvalid value." << std::endl;
                }
            } else if (*userInput == "sim") {
                std::cout<<"Simulation distance determines the distance in which physics are simulated around you (XZ, they are always simulated fully in Y)."<<std::endl;
                std::cout<<"\nValue: ";
                std::getline(std::cin, *userInput); // read line of input
                // attempt to set res mod from input.
                try {
                    *sim = std::stoi(*userInput);
                    std::cout << "\nResolution modifier set to: " << *sim << std::endl;
                } catch (...) {
                    std::cout << "\nInvalid value." << std::endl;
                }
            } else if (*userInput == "tick") {
                std::cout<<"Ticks determine the amount of times physics are calculated each frame. Higher numbers can be very performance intensive."<<std::endl;
                std::cout<<"\nValue: ";
                std::getline(std::cin, *userInput); // read line of input
                // attempt to set res mod from input.
                try {
                    *tick = std::stoi(*userInput);
                    std::cout << "\nResolution modifier set to: " << *tick << std::endl;
                } catch (...) {
                    std::cout << "\nInvalid value." << std::endl;
                }
            } else if (*userInput == "diam") {
                std::cout<<"Ambient occlusion diameter determines the distance in which voxels occlude eachother."<<std::endl;
                std::cout<<"\nValue: ";
                std::getline(std::cin, *userInput); // read line of input
                // attempt to set res mod from input.
                try {
                    *diam = std::stoi(*userInput);
                    std::cout << "\nResolution modifier set to: " << *diam << std::endl;
                } catch (...) {
                    std::cout << "\nInvalid value." << std::endl;
                }
            } else if (*userInput == "skip") {
                std::cout<<"Ambient occlusion skipping allows signifigantly less occlusion checks. Higher values may cause flickering."<<std::endl;
                std::cout<<"\nValue: ";
                std::getline(std::cin, *userInput); // read line of input
                // attempt to set res mod from input.
                try {
                    *skip = std::stoi(*userInput);
                    std::cout << "\nResolution modifier set to: " << *skip << std::endl;
                } catch (...) {
                    std::cout << "\nInvalid value." << std::endl;
                }
            } else std::cout << "\nInvalid setting.\n" << std::endl;
            }
        } else if (*userInput == "worlds") {
            // world manager.
            std::cout<<"\n\033[1m"<<"PUNDUS WORLD MANAGER" <<"\033[0m"<<"\n"<<std::endl; // title
            std::cout << "To create world, type a new name." << std::endl;
            std::vector<std::string> worldNames = {};
            
            if (!fs::is_empty(worldsPath)) std::cout<<"Or type an existing name to load:"<<std::endl;
            std::cout<<""<<std::endl; // extra line
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

            // input block
            std::cout<<""<<std::endl;
            std::cout<<"World name: ";
            std::getline(std::cin, *userInput); // read line of input
            std::cout<<""<<std::endl;
            if (*userInput == "exit") {
                std::cout<<""<<std::endl;
                std::cout<<"\033[1m"<<"PUNDUS VOXEL ENGINE" <<"\033[0m"<<"\n"<<std::endl; // title
                continue;
            }

            // check if world exists.
            newWorld = true;
            for (const std::string& name : worldNames) {
                if (name == *userInput) {
                    newWorld = false;
                    break;
                }
            }
            return;
        } else std::cout << "\nInvalid command.\n" << std::endl;
    }
    }
};

#endif