/**
 * @file simple_audio_widget_test.cpp
 * @brief Simple test to verify audio widgets can be created and displayed
 */

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <iostream>

// Include our Week 8 audio widgets
#include "ui/minimal_waveform_widget.hpp"
#include "ui/minimal_audio_track_widget.hpp"
#include "ui/minimal_audio_meters_widget.hpp"

class SimpleTestWindow : public QMainWindow {
public:
    SimpleTestWindow() {
        setWindowTitle("Audio Widget Test - Week 8");
        setMinimumSize(600, 400);
        
        auto* central = new QWidget(this);
        setCentralWidget(central);
        
        auto* layout = new QVBoxLayout(central);
        
        // Add status label
        auto* status = new QLabel("Week 8 Audio Widgets Test", this);
        layout->addWidget(status);
        
        try {
            // Test MinimalAudioMetersWidget
            auto* meters = new ve::ui::MinimalAudioMetersWidget(this);
            layout->addWidget(meters);
            std::cout << "✓ MinimalAudioMetersWidget created successfully" << std::endl;
            
            // Test MinimalWaveformWidget
            auto* waveform = new ve::ui::MinimalWaveformWidget(this);
            layout->addWidget(waveform);
            std::cout << "✓ MinimalWaveformWidget created successfully" << std::endl;
            
            // Test MinimalAudioTrackWidget
            auto* track = new ve::ui::MinimalAudioTrackWidget(this);
            layout->addWidget(track);
            std::cout << "✓ MinimalAudioTrackWidget created successfully" << std::endl;
            
            status->setText("✓ All Week 8 audio widgets created successfully!");
            
        } catch (const std::exception& e) {
            std::cerr << "Error creating widgets: " << e.what() << std::endl;
            status->setText("✗ Error creating audio widgets");
        }
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    std::cout << "=== Week 8 Audio Widget Runtime Test ===" << std::endl;
    
    SimpleTestWindow window;
    window.show();
    
    std::cout << "Window shown, widgets should be visible" << std::endl;
    std::cout << "Press Ctrl+C to exit..." << std::endl;
    
    return app.exec();
}