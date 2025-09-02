#pragma once

#include "core/frame.hpp"
#include "legacy_formats.hpp"
#include <memory>
#include <vector>

namespace ve::media_io {

/**
 * Interlaced video processing for legacy broadcast formats
 * Handles field separation, deinterlacing, and field order management
 */

enum class DeinterlacingMethod {
    NONE,           // Keep interlaced
    WEAVE,          // Simple field weaving
    BOB,            // Field doubling
    LINEAR_BLEND,   // Linear interpolation
    YADIF,          // Yet Another Deinterlacing Filter
    BWDIF,          // Bob Weaver Deinterlacing Filter
    ESTDIF,         // Edge Sensitive Temporal Deinterlacing Filter
    MOTION_ADAPTIVE // Motion-adaptive deinterlacing
};

enum class FieldDetectionMethod {
    AUTO_DETECT,      // Automatic field order detection
    PATTERN_ANALYSIS, // Analyze field patterns
    TIMECODE_BASED,   // Use timecode information
    PULLDOWN_DETECT   // Detect 3:2 pulldown patterns
};

struct InterlacedInfo {
    FieldOrder field_order = FieldOrder::PROGRESSIVE;
    bool has_pulldown = false;
    bool mixed_content = false;  // Progressive and interlaced content mixed
    double field_dominance_confidence = 0.0;
    uint32_t fields_analyzed = 0;
    
    // Pulldown detection
    bool is_telecined = false;
    std::vector<bool> pulldown_pattern;  // 3:2 pulldown pattern
    uint32_t pattern_confidence = 0;
};

struct DeinterlacingParams {
    DeinterlacingMethod method = DeinterlacingMethod::YADIF;
    FieldDetectionMethod detection = FieldDetectionMethod::AUTO_DETECT;
    bool preserve_timecode = true;
    bool output_progressive = true;
    double motion_threshold = 0.3;
    bool use_temporal_analysis = true;
    
    // Advanced parameters
    bool adaptive_mode = true;
    uint32_t field_history_length = 5;  // Number of fields to analyze
    double edge_enhancement = 0.0;      // Edge enhancement factor
};

class InterlacedProcessor {
public:
    InterlacedProcessor();
    ~InterlacedProcessor();
    
    // Configure processing
    void setParams(const DeinterlacingParams& params);
    void setInputFormat(const LegacyFormatInfo& format_info);
    
    // Field detection and analysis
    bool detectFieldOrder(const Frame& frame);
    InterlacedInfo analyzeInterlacing(const std::vector<Frame>& frames);
    bool detectPulldown(const std::vector<Frame>& frames);
    
    // Processing operations
    std::vector<Frame> deinterlace(const Frame& interlaced_frame);
    Frame separateFields(const Frame& interlaced_frame, bool top_field_first);
    Frame weaveFields(const Frame& field1, const Frame& field2);
    
    // Progressive/interlaced conversion
    Frame convertToProgressive(const Frame& interlaced_frame);
    Frame convertToInterlaced(const Frame& progressive_frame, FieldOrder target_order);
    
    // Pulldown handling
    std::vector<Frame> removePulldown(const std::vector<Frame>& telecined_frames);
    std::vector<Frame> addPulldown(const std::vector<Frame>& progressive_frames);
    
    // Quality assessment
    double calculateInterlacingStrength(const Frame& frame);
    double calculateMotionBetweenFields(const Frame& frame);
    std::vector<uint8_t> generateColorBars(uint32_t width, uint32_t height, bool interlaced = true);
    
    // Information access
    InterlacedInfo getAnalysisResults() const { return interlaced_info_; }
    FieldOrder getDetectedFieldOrder() const { return interlaced_info_.field_order; }
    bool hasValidDetection() const { return interlaced_info_.field_dominance_confidence > 0.7; }

private:
    DeinterlacingParams params_;
    LegacyFormatInfo input_format_;
    InterlacedInfo interlaced_info_;
    
    // Internal processing methods
    Frame deinterlaceYadif(const Frame& frame);
    Frame deinterlaceBwdif(const Frame& frame);
    Frame deinterlaceLinear(const Frame& frame);
    Frame deinterlaceMotionAdaptive(const Frame& frame);
    
    // Field analysis helpers
    double analyzeFieldDominance(const Frame& frame);
    std::vector<double> calculateFieldDifferences(const Frame& frame);
    bool detectMotionBetweenFields(const Frame& frame, double threshold);
    
    // Pulldown detection helpers
    bool analyzePulldownPattern(const std::vector<Frame>& frames);
    std::vector<bool> extractPulldownSequence(const std::vector<Frame>& frames);
    double calculatePulldownConfidence(const std::vector<bool>& pattern);
    
    // Color bar generation
    struct ColorBarSpec {
        uint8_t r, g, b;
        std::string name;
    };
    std::vector<ColorBarSpec> getStandardColorBars() const;
    void generateSMPTEBars(uint8_t* buffer, uint32_t width, uint32_t height, bool interlaced);
    void generateEBUBars(uint8_t* buffer, uint32_t width, uint32_t height, bool interlaced);
};

/**
 * Legacy broadcast test pattern generator
 * Generates standard test patterns for calibration and testing
 */
class TestPatternGenerator {
public:
    enum class PatternType {
        COLOR_BARS_SMPTE,    // SMPTE color bars
        COLOR_BARS_EBU,      // EBU color bars  
        COLOR_BARS_ARIB,     // ARIB color bars (Japan)
        GRAY_SCALE,          // Grayscale gradient
        ZONE_PLATE,          // Zone plate test pattern
        MULTIBURST,          // Multi-frequency burst
        CROSSHATCH,          // Crosshatch pattern
        PLUGE,               // Picture Line-Up Generation Equipment
        MOVING_ZONE          // Moving zone plate for motion testing
    };
    
    static Frame generatePattern(PatternType type, uint32_t width, uint32_t height,
                               bool interlaced = false, FieldOrder field_order = FieldOrder::TOP_FIELD_FIRST);
    
    static Frame generateSMPTEBars(uint32_t width, uint32_t height, bool interlaced = false);
    static Frame generateEBUBars(uint32_t width, uint32_t height, bool interlaced = false);
    static Frame generateMovingPattern(PatternType base_pattern, uint32_t width, uint32_t height,
                                     double motion_speed = 1.0, uint32_t frame_number = 0);
    
    // Specific legacy format patterns
    static Frame generateNTSCTestPattern(bool color = true);
    static Frame generatePALTestPattern(bool color = true);
    static Frame generateSecamTestPattern();
    
    // Technical patterns for equipment testing
    static Frame generateLinearity(uint32_t width, uint32_t height);
    static Frame generateConvergence(uint32_t width, uint32_t height);
    static Frame generateGeometry(uint32_t width, uint32_t height);
    
private:
    struct ColorBarDefinition {
        std::vector<ColorBarSpec> bars;
        std::string standard_name;
        bool has_pluge = false;
    };
    
    static ColorBarDefinition getSMPTEColorBars();
    static ColorBarDefinition getEBUColorBars(); 
    static ColorBarDefinition getARIBColorBars();
    
    static void fillColorBar(uint8_t* buffer, uint32_t width, uint32_t height,
                           uint32_t start_x, uint32_t end_x, uint8_t r, uint8_t g, uint8_t b);
    static void addInterlacedNoise(uint8_t* buffer, uint32_t width, uint32_t height, 
                                 FieldOrder field_order, double noise_level = 0.02);
};

/**
 * Field order correction utilities
 * Handles field order detection and correction for mixed content
 */
class FieldOrderCorrector {
public:
    struct CorrectionResult {
        FieldOrder detected_order = FieldOrder::UNKNOWN;
        FieldOrder corrected_order = FieldOrder::UNKNOWN;
        double confidence = 0.0;
        bool correction_applied = false;
        std::string correction_method;
    };
    
    static CorrectionResult correctFieldOrder(Frame& frame, const LegacyFormatInfo& format_info);
    static bool isFieldOrderCorrect(const Frame& frame, FieldOrder expected_order);
    static Frame swapFields(const Frame& frame);
    
    // Batch processing for sequences
    static std::vector<CorrectionResult> correctSequence(std::vector<Frame>& frames,
                                                        const LegacyFormatInfo& format_info);
    static bool detectMixedFieldOrder(const std::vector<Frame>& frames);
    
private:
    static double calculateFieldOrderConfidence(const Frame& frame, FieldOrder test_order);
    static FieldOrder detectFromMotion(const Frame& frame);
    static FieldOrder detectFromEdges(const Frame& frame);
};

/**
 * Pulldown pattern analyzer for film-to-video conversion
 * Handles 3:2 pulldown, 2:2 pulldown, and other telecine patterns
 */
class PulldownAnalyzer {
public:
    enum class PulldownType {
        NONE,
        PULLDOWN_3_2,    // 3:2 pulldown (film to NTSC)
        PULLDOWN_2_2,    // 2:2 pulldown (film to PAL)
        PULLDOWN_2_3_3_2, // Advanced pulldown pattern
        PULLDOWN_CUSTOM   // Custom pattern detected
    };
    
    struct PulldownInfo {
        PulldownType type = PulldownType::NONE;
        std::vector<bool> pattern;
        double confidence = 0.0;
        uint32_t pattern_length = 0;
        uint32_t repeat_count = 0;
        double original_frame_rate = 0.0;
        bool is_consistent = false;
    };
    
    static PulldownInfo analyzePulldown(const std::vector<Frame>& frames);
    static std::vector<Frame> removePulldown(const std::vector<Frame>& frames, const PulldownInfo& info);
    static bool isPulldownFrame(const Frame& frame, const Frame& next_frame);
    
    // Pattern detection
    static std::vector<bool> detectPulldownPattern(const std::vector<Frame>& frames);
    static PulldownType classifyPattern(const std::vector<bool>& pattern);
    static double calculatePatternConfidence(const std::vector<bool>& pattern, PulldownType type);
    
private:
    static bool isRepeatedField(const Frame& frame1, const Frame& frame2);
    static double calculateFrameSimilarity(const Frame& frame1, const Frame& frame2);
    static std::vector<double> calculateFieldSimilarities(const std::vector<Frame>& frames);
};

} // namespace ve::media_io
