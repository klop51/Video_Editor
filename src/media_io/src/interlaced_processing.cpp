#include "media_io/interlaced_processing.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ve::media_io {

InterlacedProcessor::InterlacedProcessor() {
    // Set default parameters
    params_.method = DeinterlacingMethod::YADIF;
    params_.detection = FieldDetectionMethod::AUTO_DETECT;
    params_.preserve_timecode = true;
    params_.output_progressive = true;
    params_.motion_threshold = 0.3;
    params_.use_temporal_analysis = true;
    params_.adaptive_mode = true;
    params_.field_history_length = 5;
    params_.edge_enhancement = 0.0;
}

InterlacedProcessor::~InterlacedProcessor() = default;

void InterlacedProcessor::setParams(const DeinterlacingParams& params) {
    params_ = params;
}

void InterlacedProcessor::setInputFormat(const LegacyFormatInfo& format_info) {
    input_format_ = format_info;
}

bool InterlacedProcessor::detectFieldOrder(const Frame& frame) {
    if (!frame.isValid()) {
        ve::log::warning("Cannot detect field order: invalid frame");
        return false;
    }
    
    // Reset previous detection results
    interlaced_info_ = {};
    
    // Use multiple detection methods for robustness
    double motion_confidence = analyzeFieldDominance(frame);
    std::vector<double> field_diffs = calculateFieldDifferences(frame);
    
    // Determine field order based on analysis
    if (field_diffs.size() >= 2) {
        // Compare field differences to determine dominance
        double top_field_strength = field_diffs[0];
        double bottom_field_strength = field_diffs[1];
        
        if (top_field_strength > bottom_field_strength * 1.1) {
            interlaced_info_.field_order = FieldOrder::TOP_FIELD_FIRST;
            interlaced_info_.field_dominance_confidence = motion_confidence;
        } else if (bottom_field_strength > top_field_strength * 1.1) {
            interlaced_info_.field_order = FieldOrder::BOTTOM_FIELD_FIRST;
            interlaced_info_.field_dominance_confidence = motion_confidence;
        } else {
            interlaced_info_.field_order = FieldOrder::PROGRESSIVE;
            interlaced_info_.field_dominance_confidence = 1.0 - motion_confidence;
        }
    }
    
    interlaced_info_.fields_analyzed = 1;
    
    ve::log::info("Field order detected: " + 
                 std::to_string(static_cast<int>(interlaced_info_.field_order)) +
                 " (confidence: " + std::to_string(interlaced_info_.field_dominance_confidence) + ")");
    
    return interlaced_info_.field_dominance_confidence > 0.7;
}

InterlacedInfo InterlacedProcessor::analyzeInterlacing(const std::vector<Frame>& frames) {
    if (frames.empty()) {
        ve::log::warning("Cannot analyze interlacing: no frames provided");
        return {};
    }
    
    InterlacedInfo analysis_result = {};
    std::vector<FieldOrder> detected_orders;
    std::vector<double> confidences;
    
    // Analyze multiple frames for consistent detection
    for (const auto& frame : frames) {
        if (detectFieldOrder(frame)) {
            detected_orders.push_back(interlaced_info_.field_order);
            confidences.push_back(interlaced_info_.field_dominance_confidence);
        }
    }
    
    if (!detected_orders.empty()) {
        // Find most common field order
        std::map<FieldOrder, int> order_counts;
        for (auto order : detected_orders) {
            order_counts[order]++;
        }
        
        auto most_common = std::max_element(order_counts.begin(), order_counts.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        
        analysis_result.field_order = most_common->first;
        analysis_result.fields_analyzed = frames.size();
        
        // Calculate average confidence for the dominant field order
        double total_confidence = 0.0;
        int count = 0;
        for (size_t i = 0; i < detected_orders.size(); ++i) {
            if (detected_orders[i] == analysis_result.field_order) {
                total_confidence += confidences[i];
                count++;
            }
        }
        analysis_result.field_dominance_confidence = count > 0 ? total_confidence / count : 0.0;
        
        // Check for mixed content
        analysis_result.mixed_content = (order_counts.size() > 1) && 
                                      (static_cast<double>(most_common->second) / frames.size() < 0.8);
    }
    
    // Detect pulldown if requested
    if (params_.detection == FieldDetectionMethod::PULLDOWN_DETECT) {
        analysis_result.has_pulldown = detectPulldown(frames);
    }
    
    return analysis_result;
}

bool InterlacedProcessor::detectPulldown(const std::vector<Frame>& frames) {
    if (frames.size() < 10) {
        return false;  // Need sufficient frames for pulldown detection
    }
    
    bool pulldown_detected = analyzePulldownPattern(frames);
    
    if (pulldown_detected) {
        interlaced_info_.is_telecined = true;
        interlaced_info_.pulldown_pattern = extractPulldownSequence(frames);
        interlaced_info_.pattern_confidence = calculatePulldownConfidence(interlaced_info_.pulldown_pattern);
        
        ve::log::info("3:2 pulldown detected with confidence: " + 
                     std::to_string(interlaced_info_.pattern_confidence));
    }
    
    return pulldown_detected;
}

std::vector<Frame> InterlacedProcessor::deinterlace(const Frame& interlaced_frame) {
    if (!interlaced_frame.isValid()) {
        ve::log::error("Cannot deinterlace: invalid frame");
        return {};
    }
    
    Frame progressive_frame;
    
    switch (params_.method) {
        case DeinterlacingMethod::YADIF:
            progressive_frame = deinterlaceYadif(interlaced_frame);
            break;
        case DeinterlacingMethod::BWDIF:
            progressive_frame = deinterlaceBwdif(interlaced_frame);
            break;
        case DeinterlacingMethod::LINEAR_BLEND:
            progressive_frame = deinterlaceLinear(interlaced_frame);
            break;
        case DeinterlacingMethod::MOTION_ADAPTIVE:
            progressive_frame = deinterlaceMotionAdaptive(interlaced_frame);
            break;
        case DeinterlacingMethod::BOB:
            // Bob deinterlacing produces two frames from one interlaced frame
            return {separateFields(interlaced_frame, true), separateFields(interlaced_frame, false)};
        case DeinterlacingMethod::WEAVE:
        case DeinterlacingMethod::NONE:
        default:
            progressive_frame = interlaced_frame;  // Pass through
            break;
    }
    
    return {progressive_frame};
}

Frame InterlacedProcessor::separateFields(const Frame& interlaced_frame, bool top_field_first) {
    if (!interlaced_frame.isValid()) {
        return {};
    }
    
    // Create frame for single field (half height)
    uint32_t field_height = interlaced_frame.getHeight() / 2;
    Frame field_frame(interlaced_frame.getWidth(), field_height, interlaced_frame.getFormat());
    
    const uint8_t* src_data = interlaced_frame.getData();
    uint8_t* dst_data = field_frame.getData();
    
    uint32_t src_stride = interlaced_frame.getStride();
    uint32_t dst_stride = field_frame.getStride();
    
    // Extract either top or bottom field lines
    int start_line = top_field_first ? 0 : 1;
    
    for (uint32_t y = 0; y < field_height; ++y) {
        const uint8_t* src_line = src_data + (start_line + y * 2) * src_stride;
        uint8_t* dst_line = dst_data + y * dst_stride;
        std::memcpy(dst_line, src_line, dst_stride);
    }
    
    return field_frame;
}

Frame InterlacedProcessor::weaveFields(const Frame& field1, const Frame& field2) {
    if (!field1.isValid() || !field2.isValid() || 
        field1.getWidth() != field2.getWidth() || 
        field1.getHeight() != field2.getHeight()) {
        ve::log::error("Cannot weave fields: invalid or mismatched field frames");
        return {};
    }
    
    // Create interlaced frame (double height)
    uint32_t interlaced_height = field1.getHeight() * 2;
    Frame interlaced_frame(field1.getWidth(), interlaced_height, field1.getFormat());
    
    const uint8_t* field1_data = field1.getData();
    const uint8_t* field2_data = field2.getData();
    uint8_t* dst_data = interlaced_frame.getData();
    
    uint32_t field_stride = field1.getStride();
    uint32_t dst_stride = interlaced_frame.getStride();
    
    // Interleave field lines
    for (uint32_t y = 0; y < field1.getHeight(); ++y) {
        // Top field (even lines)
        const uint8_t* field1_line = field1_data + y * field_stride;
        uint8_t* dst_line1 = dst_data + (y * 2) * dst_stride;
        std::memcpy(dst_line1, field1_line, field_stride);
        
        // Bottom field (odd lines)
        const uint8_t* field2_line = field2_data + y * field_stride;
        uint8_t* dst_line2 = dst_data + (y * 2 + 1) * dst_stride;
        std::memcpy(dst_line2, field2_line, field_stride);
    }
    
    return interlaced_frame;
}

Frame InterlacedProcessor::convertToProgressive(const Frame& interlaced_frame) {
    if (!interlaced_frame.isValid()) {
        return {};
    }
    
    // Use configured deinterlacing method
    auto deinterlaced_frames = deinterlace(interlaced_frame);
    
    if (!deinterlaced_frames.empty()) {
        return deinterlaced_frames[0];  // Return first frame for progressive output
    }
    
    return {};
}

double InterlacedProcessor::calculateInterlacingStrength(const Frame& frame) {
    if (!frame.isValid()) {
        return 0.0;
    }
    
    const uint8_t* data = frame.getData();
    uint32_t width = frame.getWidth();
    uint32_t height = frame.getHeight();
    uint32_t stride = frame.getStride();
    
    double total_diff = 0.0;
    uint32_t sample_count = 0;
    
    // Compare adjacent lines to detect interlacing artifacts
    for (uint32_t y = 1; y < height - 1; y += 2) {  // Sample odd lines
        const uint8_t* line_above = data + (y - 1) * stride;
        const uint8_t* current_line = data + y * stride;
        const uint8_t* line_below = data + (y + 1) * stride;
        
        for (uint32_t x = 0; x < width; x += 4) {  // Sample every 4th pixel
            int interpolated = (line_above[x] + line_below[x]) / 2;
            int actual = current_line[x];
            total_diff += std::abs(actual - interpolated);
            sample_count++;
        }
    }
    
    return sample_count > 0 ? total_diff / sample_count / 255.0 : 0.0;
}

std::vector<uint8_t> InterlacedProcessor::generateColorBars(uint32_t width, uint32_t height, bool interlaced) {
    std::vector<uint8_t> buffer(width * height * 3);  // RGB format
    
    if (input_format_.resolution == LegacyResolution::PAL_SD) {
        generateEBUBars(buffer.data(), width, height, interlaced);
    } else {
        generateSMPTEBars(buffer.data(), width, height, interlaced);
    }
    
    return buffer;
}

// Private implementation methods

Frame InterlacedProcessor::deinterlaceYadif(const Frame& frame) {
    // Simplified YADIF implementation
    // Real implementation would use temporal analysis with previous/next frames
    return deinterlaceLinear(frame);  // Fallback to linear for now
}

Frame InterlacedProcessor::deinterlaceLinear(const Frame& frame) {
    if (!frame.isValid()) {
        return {};
    }
    
    Frame progressive_frame(frame.getWidth(), frame.getHeight(), frame.getFormat());
    
    const uint8_t* src_data = frame.getData();
    uint8_t* dst_data = progressive_frame.getData();
    
    uint32_t width = frame.getWidth();
    uint32_t height = frame.getHeight();
    uint32_t stride = frame.getStride();
    
    // Copy first line as-is
    std::memcpy(dst_data, src_data, stride);
    
    // Interpolate missing lines
    for (uint32_t y = 1; y < height - 1; y += 2) {
        const uint8_t* line_above = src_data + (y - 1) * stride;
        const uint8_t* line_below = src_data + (y + 1) * stride;
        uint8_t* interpolated_line = dst_data + y * stride;
        
        for (uint32_t x = 0; x < width; ++x) {
            interpolated_line[x] = (line_above[x] + line_below[x]) / 2;
        }
    }
    
    // Copy even lines from original
    for (uint32_t y = 0; y < height; y += 2) {
        const uint8_t* src_line = src_data + y * stride;
        uint8_t* dst_line = dst_data + y * stride;
        std::memcpy(dst_line, src_line, stride);
    }
    
    return progressive_frame;
}

double InterlacedProcessor::analyzeFieldDominance(const Frame& frame) {
    if (!frame.isValid()) {
        return 0.0;
    }
    
    const uint8_t* data = frame.getData();
    uint32_t width = frame.getWidth();
    uint32_t height = frame.getHeight();
    uint32_t stride = frame.getStride();
    
    double odd_field_energy = 0.0;
    double even_field_energy = 0.0;
    
    // Calculate energy in odd and even fields
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* line = data + y * stride;
        
        double line_energy = 0.0;
        for (uint32_t x = 0; x < width; x += 4) {  // Sample every 4th pixel
            line_energy += line[x] * line[x];
        }
        
        if (y % 2 == 0) {
            even_field_energy += line_energy;
        } else {
            odd_field_energy += line_energy;
        }
    }
    
    double total_energy = odd_field_energy + even_field_energy;
    return total_energy > 0 ? std::abs(odd_field_energy - even_field_energy) / total_energy : 0.0;
}

bool InterlacedProcessor::analyzePulldownPattern(const std::vector<Frame>& frames) {
    if (frames.size() < 10) {
        return false;
    }
    
    // Simplified 3:2 pulldown detection
    // Look for repeating pattern of field similarities
    std::vector<double> field_similarities;
    
    for (size_t i = 1; i < frames.size(); ++i) {
        double similarity = calculateFrameSimilarity(frames[i-1], frames[i]);
        field_similarities.push_back(similarity);
    }
    
    // Look for 3:2 pattern (high-low-high-low-high sequence)
    int pattern_matches = 0;
    for (size_t i = 0; i < field_similarities.size() - 4; i += 5) {
        bool matches_pattern = 
            field_similarities[i] > 0.8 &&     // Repeat frame
            field_similarities[i+1] < 0.6 &&   // New frame
            field_similarities[i+2] > 0.8 &&   // Repeat frame
            field_similarities[i+3] < 0.6 &&   // New frame
            field_similarities[i+4] > 0.8;     // Repeat frame
        
        if (matches_pattern) {
            pattern_matches++;
        }
    }
    
    double pattern_ratio = static_cast<double>(pattern_matches) / (field_similarities.size() / 5);
    return pattern_ratio > 0.7;  // 70% pattern match threshold
}

double InterlacedProcessor::calculateFrameSimilarity(const Frame& frame1, const Frame& frame2) {
    if (!frame1.isValid() || !frame2.isValid() ||
        frame1.getWidth() != frame2.getWidth() ||
        frame1.getHeight() != frame2.getHeight()) {
        return 0.0;
    }
    
    const uint8_t* data1 = frame1.getData();
    const uint8_t* data2 = frame2.getData();
    uint32_t size = frame1.getWidth() * frame1.getHeight();
    
    double sum_squared_diff = 0.0;
    for (uint32_t i = 0; i < size; i += 16) {  // Sample every 16th pixel
        double diff = static_cast<double>(data1[i]) - static_cast<double>(data2[i]);
        sum_squared_diff += diff * diff;
    }
    
    double mse = sum_squared_diff / (size / 16);
    double psnr = 20.0 * std::log10(255.0 / std::sqrt(mse));
    
    // Convert PSNR to similarity (0-1 range)
    return std::min(1.0, psnr / 50.0);
}

void InterlacedProcessor::generateSMPTEBars(uint8_t* buffer, uint32_t width, uint32_t height, bool interlaced) {
    // SMPTE color bar pattern (simplified)
    std::vector<ColorBarSpec> smpte_bars = {
        {191, 191, 191, "White"},      // 75% white
        {191, 191, 0,   "Yellow"},     // 75% yellow
        {0,   191, 191, "Cyan"},       // 75% cyan
        {0,   191, 0,   "Green"},      // 75% green
        {191, 0,   191, "Magenta"},    // 75% magenta
        {191, 0,   0,   "Red"},        // 75% red
        {0,   0,   191, "Blue"},       // 75% blue
        {0,   0,   0,   "Black"}       // Black
    };
    
    uint32_t bar_width = width / smpte_bars.size();
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t bar_index = std::min(x / bar_width, static_cast<uint32_t>(smpte_bars.size() - 1));
            
            uint32_t pixel_offset = (y * width + x) * 3;
            buffer[pixel_offset + 0] = smpte_bars[bar_index].r;
            buffer[pixel_offset + 1] = smpte_bars[bar_index].g;
            buffer[pixel_offset + 2] = smpte_bars[bar_index].b;
        }
    }
    
    if (interlaced) {
        // Add slight field offset for interlaced patterns
        for (uint32_t y = 1; y < height; y += 2) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t pixel_offset = (y * width + x) * 3;
                // Slightly darken odd lines to simulate field differences
                buffer[pixel_offset + 0] = static_cast<uint8_t>(buffer[pixel_offset + 0] * 0.95);
                buffer[pixel_offset + 1] = static_cast<uint8_t>(buffer[pixel_offset + 1] * 0.95);
                buffer[pixel_offset + 2] = static_cast<uint8_t>(buffer[pixel_offset + 2] * 0.95);
            }
        }
    }
}

void InterlacedProcessor::generateEBUBars(uint8_t* buffer, uint32_t width, uint32_t height, bool interlaced) {
    // EBU color bar pattern (European standard)
    std::vector<ColorBarSpec> ebu_bars = {
        {191, 191, 191, "White"},      // 100% white
        {191, 191, 0,   "Yellow"},     // 100% yellow
        {0,   191, 191, "Cyan"},       // 100% cyan
        {0,   191, 0,   "Green"},      // 100% green
        {191, 0,   191, "Magenta"},    // 100% magenta
        {191, 0,   0,   "Red"},        // 100% red
        {0,   0,   191, "Blue"},       // 100% blue
        {0,   0,   0,   "Black"}       // Black
    };
    
    generateSMPTEBars(buffer, width, height, interlaced);  // Similar pattern for now
}

} // namespace ve::media_io
