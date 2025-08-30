// Ensure high DPI policy is applied before constructing QApplication-derived instance.
#include <QApplication>
#include "app/application.hpp"
#include <iostream>

int main(int argc, char** argv) {
    // Apply DPI rounding policy early (before any Q(Gui)Application is created)
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    std::cout << "Video Editor Main starting..." << std::endl;
    std::cout.flush();
    try {
        ve::app::Application app(argc, argv);
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
