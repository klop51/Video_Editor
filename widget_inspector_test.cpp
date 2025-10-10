#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QDebug>

// Include WidgetInspector directly instead of through ve_ui
#include "src/ui/include/ui/widget_inspector.hpp"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Create a test window with some widgets
    QMainWindow window;
    window.setWindowTitle("WidgetInspector Test");
    window.resize(400, 300);

    QWidget* centralWidget = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);

    // Add some test widgets
    QPushButton* button1 = new QPushButton("Play");
    button1->setObjectName("playButton");
    layout->addWidget(button1);

    QPushButton* button2 = new QPushButton("Pause");
    button2->setObjectName("pauseButton");
    layout->addWidget(button2);

    QLabel* label1 = new QLabel("Status: Ready");
    label1->setObjectName("statusLabel");
    layout->addWidget(label1);

    QLabel* label2 = new QLabel("FPS: 30");
    label2->setObjectName("fpsLabel");
    layout->addWidget(label2);

    window.setCentralWidget(centralWidget);
    window.show();

    // Test WidgetInspector
    ve::ui::WidgetInspector inspector;

    qDebug() << "=== WidgetInspector Test ===";

    // Test finding widgets by type
    auto buttons = inspector.findWidgets("QPushButton", &window);
    qDebug() << "Found" << buttons.size() << "QPushButton widgets:";

    for (auto* btn : buttons) {
        QString text = inspector.getProperty(btn, "text").toString();
        qDebug() << "  -" << btn->objectName() << "(" << btn->metaObject()->className() << "):" << text;
    }

    // Test finding widgets by name
    auto playBtn = inspector.findWidget("#playButton", &window);
    if (playBtn) {
        qDebug() << "Found play button by name:" << playBtn->objectName();
    }

    // Test getting properties
    auto props = inspector.getProperties(button1);
    qDebug() << "Play button properties (" << props.size() << "total):";
    for (const auto& prop : props) {
        qDebug() << "  " << QString::fromStdString(prop.first) << ":" << QString::fromStdString(prop.second);
    }

    // Test widget tree
    QString tree = inspector.getWidgetTree(&window, 2);
    qDebug() << "Widget tree (depth 2):";
    qDebug().noquote() << tree;

    qDebug() << "=== Test Complete ===";

    return app.exec();
}