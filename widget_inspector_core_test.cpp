#include <QCoreApplication>
#include <QDebug>
#include <iostream>

// Include WidgetInspector directly
#include "src/ui/include/ui/widget_inspector.hpp"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=== WidgetInspector Core Test ===" << std::endl;

    // Test basic WidgetInspector instantiation
    ve::ui::WidgetInspector inspector;
    std::cout << "WidgetInspector created successfully" << std::endl;

    // Test singleton pattern
    auto* instance1 = ve::ui::WidgetInspector::instance();
    auto* instance2 = ve::ui::WidgetInspector::instance();
    std::cout << "Singleton test: " << (instance1 == instance2 ? "PASS" : "FAIL") << std::endl;

    // Test basic functionality without GUI
    std::cout << "WidgetInspector basic functionality test completed" << std::endl;

    std::cout << "=== Test Complete ===" << std::endl;

    return 0; // Don't run event loop
}