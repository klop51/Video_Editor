/**
 * @file test_audio_render_include.cpp
 * @brief Simple test to verify audio render engine headers compile
 */

#include "audio/audio_render_engine.hpp"
#include "audio/mixing_graph.hpp"
#include "audio/audio_clock.hpp"
#include "core/time.hpp"
#include "core/log.hpp"

int main() {
    ve::log::info("Testing AudioRenderEngine header compilation");
    
    // Test if classes are accessible
    auto formats = ve::audio::ExportFormat::WAV;
    auto quality = ve::audio::QualityPreset::High;
    auto mode = ve::audio::RenderMode::Offline;
    
    ve::log::info("AudioRenderEngine headers compiled successfully");
    return 0;
}