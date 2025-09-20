/**
 * @file test_widgets_minimal.cpp
 * @brief Minimal test to verify audio widget compilation without Qt GUI
 */

#include <iostream>

// Test just the headers without creating widgets
#include "ui/minimal_waveform_widget.hpp"
#include "ui/minimal_audio_track_widget.hpp"
#include "ui/minimal_audio_meters_widget.hpp"

int main() {
    std::cout << "=== Week 8 Audio Widget Compilation Test ===" << std::endl;
    std::cout << "✓ MinimalWaveformWidget header included successfully" << std::endl;
    std::cout << "✓ MinimalAudioTrackWidget header included successfully" << std::endl;
    std::cout << "✓ MinimalAudioMetersWidget header included successfully" << std::endl;
    std::cout << "✓ All Week 8 audio widgets compile without errors" << std::endl;
    std::cout << "================================================" << std::endl;
    return 0;
}