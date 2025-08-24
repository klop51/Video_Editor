#include "app/application.hpp"
// #include "core/log.hpp"  // Temporarily removed to test initialization
#include <iostream>
#include <QApplication>

int main(int argc, char** argv) {
    std::cout << "Video Editor Main starting..." << std::endl;
    std::cout.flush();
    
    // Fix Qt warning about DPI policy
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    
    try {
        std::cout << "Creating ve::app::Application..." << std::endl;
        std::cout.flush();
        ve::app::Application app(argc, argv);
        
        std::cout << "Running application..." << std::endl;
        std::cout.flush();
        int result = app.run();
        
        std::cout << "Application finished with code: " << result << std::endl;
        return result;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        std::cerr.flush();
        return -1;
    } catch (...) {
        std::cerr << "Unknown exception caught" << std::endl;
        std::cerr.flush();
        return -1;
    }
}
