// main.cpp

#include <iostream>
#include <string>
#include <thread>
#include <exception>
#include "Supervisor2.h"

void main_function(const std::string& json_file_path, const std::string& consumername) {
    try {
        // Create an instance of Supervisor1
        Supervisor2 supervisor_instance(json_file_path, consumername);

        // Start the supervisor
        supervisor_instance.start();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Check if a JSON file path is provided as a command-line argument
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <json_file_path>" << std::endl;
        return 1;
    }

    // Get the JSON file path from the command-line arguments
    std::string json_file_path = argv[1];
    std::string consumername = "RTADP2";

    // Call the main function with the provided JSON file path
    main_function(json_file_path, consumername);

    return 0;
}
