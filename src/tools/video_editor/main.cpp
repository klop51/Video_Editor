// Ensure high DPI policy is applied before constructing QApplication-derived instance.
#include <QApplication>
#include "app/application.hpp"
#include "../../core/crash_trap.hpp"
#include <iostream>
#include <exception>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif


namespace {
void ve_terminate_handler() {
    std::cerr << "std::terminate invoked" << std::endl;
    if (auto current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception& e) {
            std::cerr << "  exception: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "  exception: unknown" << std::endl;
        }
    } else {
        std::cerr << "  no current exception" << std::endl;
    }
    std::abort();
}
}

int main(int argc, char** argv) {
    std::cout << "Video Editor Main starting..." << std::endl;
    
    // Install crash traps FIRST to catch crashes during startup
    ve::install_crash_traps();
    std::cout << "Crash traps installed" << std::endl;
    
    std::set_terminate(ve_terminate_handler);
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
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
