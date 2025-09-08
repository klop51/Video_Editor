#include "media_io/high_bitdepth_support.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <numeric>

namespace ve {
namespace media_io {

HighBitDepthSupport::HighBitDepthSupport() {
    initializeFormatInfo();
}

BitDepthInfo HighBitDepthSupport::getBitDepthInfo(HighBitDepthFormat format) const {
    size_t index = static_cast<size_t>(format);
    if (index < format_info_.size()) {
        return format_info_[index];
    }
    return BitDepthInfo{};
}

std::vector<HighBitDepthFormat> HighBitDepthSupport::getSupportedFormats() const {
    return {
        HighBitDepthFormat::YUV420P10LE,
        HighBitDepthFormat::YUV422P10LE,
        HighBitDepthFormat::YUV444P10LE,
        HighBitDepthFormat::YUV420P12LE,
        HighBitDepthFormat::YUV422P12LE,
        HighBitDepthFormat::YUV444P12LE,
        HighBitDepthFormat::YUV420P16LE,
        HighBitDepthFormat::YUV422P16LE,
        HighBitDepthFormat::YUV444P16LE,
        HighBitDepthFormat::RGB48LE,
        HighBitDepthFormat::RGBA64LE,
        HighBitDepthFormat::YUVA420P10LE,
        HighBitDepthFormat::YUVA422P10LE,
        HighBitDepthFormat::YUVA444P10LE,
        HighBitDepthFormat::V210,
        HighBitDepthFormat::V410
    };
}

bool HighBitDepthSupport::isFormatSupported(HighBitDepthFormat format) const {
    auto supported = getSupportedFormats();
    return std::find(supported.begin(), supported.end(), format) != supported.end();
}

HighBitDepthFormat HighBitDepthSupport::detectFormat([[maybe_unused]] const uint8_t* data, size_t size) const {
    // Simple format detection based on data patterns
    // In real implementation, this would analyze FFmpeg codec info
    
    if (size < 16) {
        return HighBitDepthFormat::UNKNOWN;
    }
    
    // Check for V210 packed format signature (specific bit patterns)
    if (size % 128 == 0) {  // V210 uses 128-byte alignment
        return HighBitDepthFormat::V210;
    }
    
    // Default to most common 10-bit format
    return HighBitDepthFormat::YUV420P10LE;
}

bool HighBitDepthSupport::requiresHighBitDepthProcessing(HighBitDepthFormat format) const {
    BitDepthInfo info = getBitDepthInfo(format);
    return info.bits_per_component > 8;
}

void HighBitDepthSupport::setProcessingPrecision(const ProcessingPrecision& precision) {
    processing_precision_ = precision;
}

ProcessingPrecision HighBitDepthSupport::getProcessingPrecision() const {
    return processing_precision_;
}

bool HighBitDepthSupport::convertBitDepth(
    const HighBitDepthFrame& source,
    HighBitDepthFrame& destination,
    HighBitDepthFormat target_format,
    DitheringMethod dithering) const {
    
    BitDepthInfo source_info = getBitDepthInfo(source.format);
    BitDepthInfo target_info = getBitDepthInfo(target_format);
    
    if (source_info.bits_per_component == 0 || target_info.bits_per_component == 0) {
        return false;
    }
    
    // Initialize destination frame
    destination.format = target_format;
    destination.width = source.width;
    destination.height = source.height;
    destination.bit_depth_info = target_info;
    destination.is_limited_range = source.is_limited_range;
    destination.gamma = source.gamma;
    destination.frame_number = source.frame_number;
    
    // Calculate plane count based on format
    size_t plane_count = target_info.has_alpha ? 4 : 3;
    if (target_format == HighBitDepthFormat::RGB48LE) {
        plane_count = 1;  // Packed RGB
    }
    
    destination.planes.resize(plane_count);
    destination.linesize.resize(plane_count);
    
    // Calculate memory requirements
    size_t bytes_per_sample = (target_info.bits_per_component + 7) / 8;
    
    for (size_t i = 0; i < plane_count; ++i) {
        uint32_t plane_width = destination.width;
        uint32_t plane_height = destination.height;
        
        // Adjust for chroma subsampling
        if (i > 0 && target_format != HighBitDepthFormat::YUV444P10LE && 
            target_format != HighBitDepthFormat::YUV444P12LE &&
            target_format != HighBitDepthFormat::YUV444P16LE) {
            plane_width /= 2;
            if (target_format == HighBitDepthFormat::YUV420P10LE ||
                target_format == HighBitDepthFormat::YUV420P12LE ||
                target_format == HighBitDepthFormat::YUV420P16LE) {
                plane_height /= 2;
            }
        }
        
        destination.linesize[i] = static_cast<uint32_t>(plane_width * bytes_per_sample);
        destination.planes[i].resize(destination.linesize[i] * plane_height);
    }
    
    // Perform actual conversion
    if (source_info.bits_per_component > target_info.bits_per_component) {
        // Reducing bit depth - apply dithering
        return convertWithDithering(source, destination, dithering);
    } else if (source_info.bits_per_component < target_info.bits_per_component) {
        // Increasing bit depth - simple scaling
        return convertWithUpscaling(source, destination);
    } else {
        // Same bit depth - direct copy with format conversion
        return convertSameBitDepth(source, destination);
    }
}

QualityMetrics HighBitDepthSupport::assessQuality(
    const HighBitDepthFrame& reference,
    const HighBitDepthFrame& processed) const {
    
    QualityMetrics metrics;
    
    if (reference.planes.empty() || processed.planes.empty()) {
        return metrics;
    }
    
    // Convert data to 16-bit for comparison
    std::vector<uint16_t> ref_data, proc_data;
    extractLumaData(reference, ref_data);
    extractLumaData(processed, proc_data);
    
    if (ref_data.size() != proc_data.size()) {
        return metrics;
    }
    
    // Calculate PSNR
    metrics.psnr = calculatePSNR(ref_data, proc_data);
    
    // Calculate SSIM
    metrics.ssim = calculateSSIM(ref_data, proc_data, reference.width, reference.height);
    
    // Count clipped pixels
    metrics.clipped_pixels = countClippedPixels(processed);
    
    // Count out-of-range pixels
    metrics.out_of_range_pixels = countOutOfRangePixels(processed);
    
    // Overall quality assessment
    metrics.quality_acceptable = (metrics.psnr > 40.0) && 
                                (metrics.ssim > 0.95) && 
                                (metrics.clipped_pixels < (reference.width * reference.height * 0.001));
    
    return metrics;
}

uint8_t HighBitDepthSupport::recommendInternalBitDepth(
    HighBitDepthFormat input_format,
    const std::vector<std::string>& processing_operations) const {
    
    BitDepthInfo input_info = getBitDepthInfo(input_format);
    uint8_t base_depth = input_info.bits_per_component;
    
    // Analyze processing operations for precision requirements
    uint8_t required_headroom = 0;
    
    for (const auto& operation : processing_operations) {
        if (operation.find("color_grade") != std::string::npos ||
            operation.find("exposure") != std::string::npos) {
            required_headroom = std::max(required_headroom, static_cast<uint8_t>(2));
        } else if (operation.find("composite") != std::string::npos ||
                   operation.find("blend") != std::string::npos) {
            required_headroom = std::max(required_headroom, static_cast<uint8_t>(1));
        } else if (operation.find("resize") != std::string::npos ||
                   operation.find("transform") != std::string::npos) {
            required_headroom = std::max(required_headroom, static_cast<uint8_t>(1));
        }
    }
    
    uint8_t recommended_depth = base_depth + required_headroom;
    
    // Clamp to available bit depths
    if (recommended_depth <= 8) return 8;
    if (recommended_depth <= 10) return 10;
    if (recommended_depth <= 12) return 12;
    return 16;
}

size_t HighBitDepthSupport::calculateMemoryRequirement(
    HighBitDepthFormat format,
    uint32_t width,
    uint32_t height) const {
    
    BitDepthInfo info = getBitDepthInfo(format);
    if (info.bits_per_component == 0) {
        return 0;
    }
    
    size_t bytes_per_sample = (info.bits_per_component + 7) / 8;
    [[maybe_unused]] size_t samples_per_pixel = info.components_per_pixel;
    
    // Account for chroma subsampling
    double chroma_factor = 1.0;
    if (format == HighBitDepthFormat::YUV420P10LE ||
        format == HighBitDepthFormat::YUV420P12LE ||
        format == HighBitDepthFormat::YUV420P16LE) {
        chroma_factor = 1.5;  // Y + 0.25*U + 0.25*V
    } else if (format == HighBitDepthFormat::YUV422P10LE ||
               format == HighBitDepthFormat::YUV422P12LE ||
               format == HighBitDepthFormat::YUV422P16LE) {
        chroma_factor = 2.0;  // Y + 0.5*U + 0.5*V
    }
    
    return static_cast<size_t>(static_cast<double>(width) * static_cast<double>(height) * static_cast<double>(bytes_per_sample) * chroma_factor);
}

void HighBitDepthSupport::applyDithering(
    const std::vector<uint16_t>& source,
    std::vector<uint8_t>& destination,
    DitheringMethod method,
    uint32_t width,
    uint32_t height) const {
    
    destination.resize(source.size());
    
    switch (method) {
        case DitheringMethod::NONE:
            // Simple truncation
            for (size_t i = 0; i < source.size(); ++i) {
                destination[i] = static_cast<uint8_t>(source[i] >> 8);
            }
            break;
            
        case DitheringMethod::ORDERED:
            applyOrderedDithering(source, destination, width, height);
            break;
            
        case DitheringMethod::ERROR_DIFFUSION:
            floydSteinbergDither(source, destination, width, height, 65535, 255);
            break;
            
        case DitheringMethod::TRIANGULAR_PDF:
            applyTriangularDithering(source, destination);
            break;
            
        default:
            // Fallback to simple truncation
            applyDithering(source, destination, DitheringMethod::NONE, width, height);
            break;
    }
}

void HighBitDepthSupport::convertRange(HighBitDepthFrame& frame, bool to_full_range) const {
    if (frame.is_limited_range == !to_full_range) {
        return;  // Already in target range
    }
    
    BitDepthInfo info = frame.bit_depth_info;
    uint32_t max_value = info.max_value;
    
    // Range conversion constants
    double scale_factor, offset;
    
    if (to_full_range) {
        // Limited to full range
        scale_factor = static_cast<double>(max_value) / (max_value - 256);  // Approximate
        offset = -16.0 * scale_factor / max_value;
    } else {
        // Full to limited range  
        scale_factor = static_cast<double>(max_value - 256) / max_value;
        offset = 16.0 / max_value;
    }
    
    // Apply conversion to all planes
    for (auto& plane : frame.planes) {
        if (info.bits_per_component <= 8) {
            for (uint8_t& pixel : plane) {
                double converted = pixel * scale_factor + offset * 255.0;
                pixel = static_cast<uint8_t>(std::clamp(converted, 0.0, 255.0));
            }
        } else {
            // 16-bit processing
            uint16_t* pixels = reinterpret_cast<uint16_t*>(plane.data());
            size_t pixel_count = plane.size() / 2;
            
            for (size_t i = 0; i < pixel_count; ++i) {
                double converted = pixels[i] * scale_factor + offset * max_value;
                pixels[i] = static_cast<uint16_t>(std::clamp(converted, 0.0, static_cast<double>(max_value)));
            }
        }
    }
    
    frame.is_limited_range = !to_full_range;
}

std::vector<std::pair<uint32_t, uint32_t>> HighBitDepthSupport::detectClipping(
    const HighBitDepthFrame& frame) const {
    
    std::vector<std::pair<uint32_t, uint32_t>> clipped_regions;
    
    if (frame.planes.empty()) {
        return clipped_regions;
    }
    
    BitDepthInfo info = frame.bit_depth_info;
    uint32_t max_value = info.max_value;
    uint32_t min_value = 0;
    
    // Adjust for limited range
    if (frame.is_limited_range) {
        min_value = static_cast<uint32_t>(16.0 * max_value / 255.0);
        max_value = static_cast<uint32_t>(235.0 * max_value / 255.0);
    }
    
    // Check luma plane for clipping
    const auto& luma_plane = frame.planes[0];
    
    if (info.bits_per_component <= 8) {
        for (size_t i = 0; i < luma_plane.size(); ++i) {
            uint8_t pixel = luma_plane[i];
            if (pixel <= min_value || pixel >= max_value) {
                uint32_t x = static_cast<uint32_t>(i % frame.width);
                uint32_t y = static_cast<uint32_t>(i / frame.width);
                clipped_regions.emplace_back(x, y);
            }
        }
    } else {
        const uint16_t* pixels = reinterpret_cast<const uint16_t*>(luma_plane.data());
        size_t pixel_count = luma_plane.size() / 2;
        
        for (size_t i = 0; i < pixel_count; ++i) {
            uint16_t pixel = pixels[i];
            if (pixel <= min_value || pixel >= max_value) {
                uint32_t x = static_cast<uint32_t>(i % frame.width);
                uint32_t y = static_cast<uint32_t>(i / frame.width);
                clipped_regions.emplace_back(x, y);
            }
        }
    }
    
    return clipped_regions;
}

bool HighBitDepthSupport::validateFormat(const HighBitDepthFrame& frame) const {
    if (frame.planes.empty() || frame.linesize.empty()) {
        return false;
    }
    
    BitDepthInfo info = getBitDepthInfo(frame.format);
    if (info.bits_per_component == 0) {
        return false;
    }
    
    // Validate plane count
    size_t expected_planes = info.has_alpha ? 4 : 3;
    if (frame.format == HighBitDepthFormat::RGB48LE || 
        frame.format == HighBitDepthFormat::RGBA64LE) {
        expected_planes = 1;  // Packed format
    }
    
    if (frame.planes.size() != expected_planes) {
        return false;
    }
    
    // Validate plane sizes
    for (size_t i = 0; i < frame.planes.size(); ++i) {
        if (frame.planes[i].empty() || frame.linesize[i] == 0) {
            return false;
        }
    }
    
    return true;
}

std::string HighBitDepthSupport::getFormatDescription(HighBitDepthFormat format) const {
    BitDepthInfo info = getBitDepthInfo(format);
    return info.description;
}

// Private implementation methods

void HighBitDepthSupport::initializeFormatInfo() {
    // Initialize format information array
    format_info_[static_cast<size_t>(HighBitDepthFormat::YUV420P10LE)] = {
        10, 3, false, true, false, 1023, 1023.0/255.0, "4:2:0 10-bit planar YUV"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::YUV422P10LE)] = {
        10, 3, false, true, false, 1023, 1023.0/255.0, "4:2:2 10-bit planar YUV"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::YUV444P10LE)] = {
        10, 3, false, true, false, 1023, 1023.0/255.0, "4:4:4 10-bit planar YUV"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::YUV420P12LE)] = {
        12, 3, false, true, false, 4095, 4095.0/255.0, "4:2:0 12-bit planar YUV"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::YUV422P12LE)] = {
        12, 3, false, true, false, 4095, 4095.0/255.0, "4:2:2 12-bit planar YUV"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::YUV444P12LE)] = {
        12, 3, false, true, false, 4095, 4095.0/255.0, "4:4:4 12-bit planar YUV"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::YUV420P16LE)] = {
        16, 3, false, true, false, 65535, 65535.0/255.0, "4:2:0 16-bit planar YUV"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::RGB48LE)] = {
        16, 3, false, false, false, 65535, 65535.0/255.0, "16-bit packed RGB"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::RGBA64LE)] = {
        16, 4, true, false, false, 65535, 65535.0/255.0, "16-bit packed RGBA"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::V210)] = {
        10, 3, false, false, false, 1023, 1023.0/255.0, "10-bit 4:2:2 packed (V210)"
    };
    
    format_info_[static_cast<size_t>(HighBitDepthFormat::V410)] = {
        10, 3, false, false, false, 1023, 1023.0/255.0, "10-bit 4:4:4 packed (V410)"
    };
}

void HighBitDepthSupport::floydSteinbergDither(
    const std::vector<uint16_t>& source,
    std::vector<uint8_t>& destination,
    uint32_t width,
    uint32_t height,
    uint32_t source_max,
    uint32_t dest_max) const {
    
    std::vector<int32_t> error_buffer(source.size(), 0);
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            
            // Apply accumulated error
            int32_t old_pixel = static_cast<int32_t>(source[idx]) + error_buffer[idx];
            old_pixel = std::clamp(old_pixel, 0, static_cast<int32_t>(source_max));
            
            // Quantize to destination bit depth
            uint8_t new_pixel = static_cast<uint8_t>(
                (static_cast<uint32_t>(old_pixel) * dest_max + source_max/2) / source_max
            );
            destination[idx] = new_pixel;
            
            // Calculate quantization error
            int32_t quant_error = old_pixel - static_cast<int32_t>((static_cast<uint32_t>(new_pixel) * source_max / dest_max));
            
            // Distribute error to neighboring pixels
            if (x + 1 < width) {
                error_buffer[idx + 1] += (quant_error * 7) / 16;
            }
            if (y + 1 < height) {
                if (x > 0) {
                    error_buffer[idx + width - 1] += (quant_error * 3) / 16;
                }
                error_buffer[idx + width] += (quant_error * 5) / 16;
                if (x + 1 < width) {
                    error_buffer[idx + width + 1] += (quant_error * 1) / 16;
                }
            }
        }
    }
}

double HighBitDepthSupport::calculatePSNR(
    const std::vector<uint16_t>& reference,
    const std::vector<uint16_t>& processed) const {
    
    if (reference.size() != processed.size() || reference.empty()) {
        return 0.0;
    }
    
    double mse = 0.0;
    for (size_t i = 0; i < reference.size(); ++i) {
        double diff = static_cast<double>(reference[i]) - static_cast<double>(processed[i]);
        mse += diff * diff;
    }
    mse /= static_cast<double>(reference.size());
    
    if (mse == 0.0) {
        return 100.0;  // Perfect match
    }
    
    double max_pixel_value = 65535.0;  // 16-bit max
    return 20.0 * std::log10(max_pixel_value / std::sqrt(mse));
}

double HighBitDepthSupport::calculateSSIM(
    const std::vector<uint16_t>& reference,
    const std::vector<uint16_t>& processed,
    [[maybe_unused]] uint32_t width,
    [[maybe_unused]] uint32_t height) const {
    
    // Simplified SSIM calculation (normally would use sliding window)
    if (reference.size() != processed.size() || reference.empty()) {
        return 0.0;
    }
    
    // Calculate means
    double mean_ref = std::accumulate(reference.begin(), reference.end(), 0.0) / static_cast<double>(reference.size());
    double mean_proc = std::accumulate(processed.begin(), processed.end(), 0.0) / static_cast<double>(processed.size());
    
    // Calculate variances and covariance
    double var_ref = 0.0, var_proc = 0.0, covar = 0.0;
    
    for (size_t i = 0; i < reference.size(); ++i) {
        double diff_ref = reference[i] - mean_ref;
        double diff_proc = processed[i] - mean_proc;
        
        var_ref += diff_ref * diff_ref;
        var_proc += diff_proc * diff_proc;
        covar += diff_ref * diff_proc;
    }
    
    var_ref /= static_cast<double>(reference.size());
    var_proc /= static_cast<double>(reference.size());
    covar /= static_cast<double>(reference.size());
    
    // SSIM constants (scaled for 16-bit)
    double c1 = 416.25;     // (0.01 * 65535 / 10)^2 
    double c2 = 3748.125;   // (0.03 * 65535 / 10)^2
    
    // SSIM formula
    double numerator = (2.0 * mean_ref * mean_proc + c1) * (2.0 * covar + c2);
    double denominator = (mean_ref * mean_ref + mean_proc * mean_proc + c1) * (var_ref + var_proc + c2);
    
    if (denominator == 0.0) {
        return 1.0;  // Perfect match if both are constant
    }
    
    double ssim = numerator / denominator;
    
    // Clamp to valid range [0, 1]
    return std::max(0.0, std::min(1.0, ssim));
}

uint32_t HighBitDepthSupport::countOutOfRangePixels(const HighBitDepthFrame& frame) const {
    uint32_t count = 0;
    
    if (frame.planes.empty()) {
        return count;
    }
    
    BitDepthInfo info = frame.bit_depth_info;
    uint32_t max_legal = info.max_value;
    uint32_t min_legal = 0;
    
    if (frame.is_limited_range) {
        min_legal = static_cast<uint32_t>(16.0 * info.max_value / 255.0);
        max_legal = static_cast<uint32_t>(235.0 * info.max_value / 255.0);
    }
    
    // Check all planes
    for (const auto& plane : frame.planes) {
        if (info.bits_per_component <= 8) {
            for (uint8_t pixel : plane) {
                if (pixel < min_legal || pixel > max_legal) {
                    count++;
                }
            }
        } else {
            const uint16_t* pixels = reinterpret_cast<const uint16_t*>(plane.data());
            size_t pixel_count = plane.size() / 2;
            
            for (size_t i = 0; i < pixel_count; ++i) {
                if (pixels[i] < min_legal || pixels[i] > max_legal) {
                    count++;
                }
            }
        }
    }
    
    return count;
}

// Additional helper methods for conversion and processing
bool HighBitDepthSupport::convertWithDithering(
    const HighBitDepthFrame& source,
    HighBitDepthFrame& destination,
    [[maybe_unused]] DitheringMethod dithering) const {
    
    // Simple conversion with dithering - full implementation would handle all format combinations
    if (source.planes.empty() || destination.planes.empty()) {
        return false;
    }
    
    // For now, just copy the data (simplified)
    for (size_t i = 0; i < std::min(source.planes.size(), destination.planes.size()); ++i) {
        if (destination.planes[i].size() >= source.planes[i].size()) {
            std::copy(source.planes[i].begin(), source.planes[i].end(), destination.planes[i].begin());
        }
    }
    
    return true;
}

bool HighBitDepthSupport::convertWithUpscaling(
    const HighBitDepthFrame& source,
    HighBitDepthFrame& destination) const {
    
    // Simple upscaling conversion
    if (source.planes.empty() || destination.planes.empty()) {
        return false;
    }
    
    BitDepthInfo source_info = source.bit_depth_info;
    BitDepthInfo dest_info = destination.bit_depth_info;
    
    uint8_t bit_shift = dest_info.bits_per_component - source_info.bits_per_component;
    
    for (size_t plane_idx = 0; plane_idx < std::min(source.planes.size(), destination.planes.size()); ++plane_idx) {
        const auto& src_plane = source.planes[plane_idx];
        auto& dst_plane = destination.planes[plane_idx];
        
        if (source_info.bits_per_component <= 8 && dest_info.bits_per_component > 8) {
            // 8-bit to 16-bit
            uint16_t* dst_data = reinterpret_cast<uint16_t*>(dst_plane.data());
            for (size_t i = 0; i < src_plane.size() && i * 2 < dst_plane.size(); ++i) {
                dst_data[i] = static_cast<uint16_t>(static_cast<uint16_t>(src_plane[i]) << bit_shift);
            }
        } else {
            // Same bit depth or other combinations
            size_t copy_size = std::min(src_plane.size(), dst_plane.size());
            std::copy(src_plane.begin(), src_plane.begin() + static_cast<std::vector<uint8_t>::difference_type>(copy_size), dst_plane.begin());
        }
    }
    
    return true;
}

bool HighBitDepthSupport::convertSameBitDepth(
    const HighBitDepthFrame& source,
    HighBitDepthFrame& destination) const {
    
    // Direct copy for same bit depth
    if (source.planes.empty() || destination.planes.empty()) {
        return false;
    }
    
    for (size_t i = 0; i < std::min(source.planes.size(), destination.planes.size()); ++i) {
        size_t copy_size = std::min(source.planes[i].size(), destination.planes[i].size());
        std::copy(source.planes[i].begin(), source.planes[i].begin() + static_cast<std::vector<uint8_t>::difference_type>(copy_size), destination.planes[i].begin());
    }
    
    return true;
}

void HighBitDepthSupport::extractLumaData(
    const HighBitDepthFrame& frame,
    std::vector<uint16_t>& luma_data) const {
    
    if (frame.planes.empty()) {
        return;
    }
    
    const auto& luma_plane = frame.planes[0];  // Y plane
    BitDepthInfo info = frame.bit_depth_info;
    
    if (info.bits_per_component <= 8) {
        // Convert 8-bit to 16-bit for comparison
        luma_data.resize(luma_plane.size());
        for (size_t i = 0; i < luma_plane.size(); ++i) {
            luma_data[i] = static_cast<uint16_t>(static_cast<uint16_t>(luma_plane[i]) << 8);
        }
    } else {
        // Already 16-bit
        const uint16_t* data = reinterpret_cast<const uint16_t*>(luma_plane.data());
        size_t sample_count = luma_plane.size() / 2;
        luma_data.assign(data, data + sample_count);
    }
}

uint32_t HighBitDepthSupport::countClippedPixels(const HighBitDepthFrame& frame) const {
    if (frame.planes.empty()) {
        return 0;
    }
    
    BitDepthInfo info = frame.bit_depth_info;
    uint32_t count = 0;
    
    // Check luma plane for clipping (0 or max values)
    const auto& luma_plane = frame.planes[0];
    
    if (info.bits_per_component <= 8) {
        for (uint8_t pixel : luma_plane) {
            if (pixel == 0 || pixel == 255) {
                count++;
            }
        }
    } else {
        const uint16_t* pixels = reinterpret_cast<const uint16_t*>(luma_plane.data());
        size_t pixel_count = luma_plane.size() / 2;
        
        for (size_t i = 0; i < pixel_count; ++i) {
            if (pixels[i] == 0 || pixels[i] == info.max_value) {
                count++;
            }
        }
    }
    
    return count;
}

void HighBitDepthSupport::applyOrderedDithering(
    const std::vector<uint16_t>& source,
    std::vector<uint8_t>& destination,
    uint32_t width,
    uint32_t height) const {
    
    // Simple 4x4 Bayer matrix
    static const uint8_t bayer_matrix[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5}
    };
    
    destination.resize(source.size());
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = y * width + x;
            if (idx >= source.size()) break;
            
            uint16_t pixel = source[idx];
            uint8_t threshold = bayer_matrix[y & 3][x & 3] * 16;  // Scale to 0-240
            
            uint8_t quantized = static_cast<uint8_t>(pixel >> 8);
            uint8_t error = static_cast<uint8_t>(pixel & 0xFF);
            
            if (error > threshold) {
                quantized = static_cast<uint8_t>(std::min(255, static_cast<int>(quantized) + 1));
            }
            
            destination[idx] = quantized;
        }
    }
}

void HighBitDepthSupport::applyTriangularDithering(
    const std::vector<uint16_t>& source,
    std::vector<uint8_t>& destination) const {
    
    destination.resize(source.size());
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-0.5, 0.5);
    
    for (size_t i = 0; i < source.size(); ++i) {
        double pixel_value = static_cast<double>(source[i]) / 65535.0 * 255.0;
        double dithered_value = pixel_value + dis(gen);
        destination[i] = static_cast<uint8_t>(std::clamp(dithered_value, 0.0, 255.0));
    }
}

// Utility function implementations
namespace high_bitdepth_utils {

uint16_t scaleBitDepth(uint16_t value, uint8_t from_bits, uint8_t to_bits) {
    if (from_bits == to_bits) {
        return value;
    }
    
    if (from_bits > to_bits) {
        // Reducing bit depth
        return value >> (from_bits - to_bits);
    } else {
        // Increasing bit depth
        return value << (to_bits - from_bits);
    }
}

bool isLegalRange(uint16_t value, uint8_t bit_depth, bool limited_range) {
    uint32_t max_value = (1U << bit_depth) - 1;
    
    if (!limited_range) {
        return value <= max_value;
    }
    
    uint32_t min_legal = static_cast<uint32_t>(16.0 * max_value / 255.0);
    uint32_t max_legal = static_cast<uint32_t>(235.0 * max_value / 255.0);
    
    return value >= min_legal && value <= max_legal;
}

HighBitDepthFormat detectFromCodecName(const std::string& codec_name) {
    // Convert codec name to lowercase for comparison
    std::string lower_codec = codec_name;
    std::transform(lower_codec.begin(), lower_codec.end(), lower_codec.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    if (lower_codec.find("prores") != std::string::npos) {
        if (lower_codec.find("4444") != std::string::npos) {
            return HighBitDepthFormat::YUVA444P12LE;  // ProRes 4444
        }
        return HighBitDepthFormat::YUV422P10LE;  // Standard ProRes
    }
    
    if (lower_codec.find("dnxhd") != std::string::npos ||
        lower_codec.find("dnxhr") != std::string::npos) {
        return HighBitDepthFormat::YUV422P10LE;
    }
    
    if (lower_codec.find("v210") != std::string::npos) {
        return HighBitDepthFormat::V210;
    }
    
    if (lower_codec.find("hevc") != std::string::npos && 
        lower_codec.find("10") != std::string::npos) {
        return HighBitDepthFormat::YUV420P10LE;
    }
    
    return HighBitDepthFormat::UNKNOWN;
}

uint8_t calculateOptimalPrecision(
    const std::vector<HighBitDepthFormat>& input_formats,
    const std::vector<std::string>& operations) {
    
    uint8_t max_input_depth = 8;
    
    // Find maximum input bit depth
    HighBitDepthSupport support;
    for (HighBitDepthFormat format : input_formats) {
        BitDepthInfo info = support.getBitDepthInfo(format);
        max_input_depth = std::max(max_input_depth, info.bits_per_component);
    }
    
    // Add headroom based on operations
    uint8_t headroom = 0;
    for (const std::string& op : operations) {
        if (op.find("grade") != std::string::npos || 
            op.find("composite") != std::string::npos) {
            headroom = std::max(headroom, static_cast<uint8_t>(4));
        } else if (op.find("resize") != std::string::npos) {
            headroom = std::max(headroom, static_cast<uint8_t>(2));
        }
    }
    
    uint8_t optimal = max_input_depth + headroom;
    return std::min(optimal, static_cast<uint8_t>(16));
}

ProcessingRecommendation getProcessingRecommendation(
    HighBitDepthFormat input_format,
    uint32_t width,
    uint32_t height,
    size_t available_memory) {
    
    ProcessingRecommendation rec;
    
    HighBitDepthSupport support;
    BitDepthInfo info = support.getBitDepthInfo(input_format);
    
    // Calculate memory requirement for 16-bit processing
    size_t memory_16bit = width * height * 3 * 2;  // 3 components, 2 bytes each
    
    if (available_memory > memory_16bit * 4) {
        // Plenty of memory - use highest quality
        rec.internal_bit_depth = 16;
        rec.use_streaming = false;
        rec.recommended_buffer_size = memory_16bit * 2;
        rec.dithering_method = DitheringMethod::BLUE_NOISE;
    } else if (available_memory > memory_16bit * 2) {
        // Moderate memory - balance quality and performance
        rec.internal_bit_depth = std::max(static_cast<uint8_t>(12), info.bits_per_component);
        rec.use_streaming = false;
        rec.recommended_buffer_size = memory_16bit;
        rec.dithering_method = DitheringMethod::ERROR_DIFFUSION;
    } else {
        // Limited memory - optimize for memory usage
        rec.internal_bit_depth = info.bits_per_component;
        rec.use_streaming = true;
        rec.recommended_buffer_size = memory_16bit / 4;
        rec.dithering_method = DitheringMethod::ORDERED;
    }
    
    return rec;
}

} // namespace high_bitdepth_utils

} // namespace media_io
} // namespace ve
