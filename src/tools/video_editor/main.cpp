#include "app/application.hpp"
#include "core/log.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "Video Editor Main starting..." << std::endl;
    
    try {
        std::cout << "Creating ve::app::Application..." << std::endl;
        ve::app::Application app(argc, argv);
        
        std::cout << "Running application..." << std::endl;
        int result = app.run();
        
        std::cout << "Application finished with code: " << result << std::endl;
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
        return -1;
    }
}
