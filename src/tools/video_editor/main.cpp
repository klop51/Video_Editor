// Ensure high DPI policy is applied before constructing QApplication-derived instance.
#include <QApplication>
#include "app/application.hpp"
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "Video Editor Main starting..." << std::endl;
    std::cout.flush();
    
    // Apply DPI rounding policy BEFORE any Qt application creation - must be very first Qt call
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    std::cout << "High DPI policy set" << std::endl;
    
    try {
        std::cout << "Creating Application object..." << std::endl;
        ve::app::Application app(argc, argv);
        std::cout << "Application object created, calling run()..." << std::endl;
        
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
