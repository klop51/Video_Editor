#include <iostream>
#include <QApplication>

int main(int argc, char** argv) {
    std::cout << "Starting minimal Qt test..." << std::endl;
    
    try {
        QApplication app(argc, argv);
        std::cout << "QApplication created successfully" << std::endl;
        
        app.setApplicationName("Test App");
        std::cout << "Application name set" << std::endl;
        
        return 0; // Don't run event loop, just test creation
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        return -1;
    }
}
