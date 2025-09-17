/**
 * @file mixing_graph.hpp
 * @brief Node-Based Audio Mixing Architecture for Professional Video Editing
 * 
 * Phase 2 Week 4: Advanced Mixing Graph Implementation
 * 
 * Provides professional-grade audio mixing capabilities with:
 * - Dynamic node-based audio processing graph
 * - Real-time reconfiguration without dropouts
 * - SIMD-optimized processing loops
 * - Multi-track mixing with effects chains
 * 
 * Architecture:
 * AudioNode (base) → [InputNode|EffectNode|MixerNode|OutputNode]
 * Graph manages node connections and processing order
 */

#pragma once

#include "audio/audio_frame.hpp"
#include "core/time.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <atomic>
#include <functional>
#include <mutex>
#include <immintrin.h> // For SIMD intrinsics

namespace ve::audio {

// Forward declarations
class AudioNode;
class MixingGraph;

/**
 * @brief Unique identifier for audio nodes in the mixing graph
 */
using NodeID = uint32_t;
constexpr NodeID INVALID_NODE_ID = 0;

/**
 * @brief Audio processing node types
 */
enum class NodeType {
    Input,      ///< Audio input (decoder, microphone, etc.)
    Effect,     ///< Audio effect processor (EQ, compressor, etc.)
    Mixer,      ///< Audio mixer (combines multiple inputs)
    Output,     ///< Audio output (speakers, file export, etc.)
    Bus         ///< Audio bus (routing and grouping)
};

/**
 * @brief Node processing priority levels
 */
enum class ProcessingPriority {
    Realtime,   ///< Highest priority for real-time playback
    Interactive,///< High priority for user interactions
    Background, ///< Lower priority for background processing
    Offline     ///< Lowest priority for export/rendering
};

/**
 * @brief Audio processing parameters for nodes
 */
struct AudioProcessingParams {
    uint32_t sample_rate = 48000;      ///< Target sample rate
    uint16_t channels = 2;             ///< Number of channels
    uint32_t buffer_size = 1024;       ///< Buffer size in samples
    SampleFormat format = SampleFormat::Float32; ///< Sample format
    
    // SIMD optimization settings
    bool enable_simd = true;           ///< Enable SIMD optimizations
    bool enable_avx = true;            ///< Enable AVX instructions
    uint32_t simd_alignment = 32;      ///< Memory alignment for SIMD
};

/**
 * @brief Node connection information
 */
struct NodeConnection {
    NodeID source_node = INVALID_NODE_ID;  ///< Source node ID
    NodeID target_node = INVALID_NODE_ID;  ///< Target node ID
    uint16_t source_output = 0;             ///< Source output index
    uint16_t target_input = 0;              ///< Target input index
    float gain = 1.0f;                      ///< Connection gain
    bool enabled = true;                    ///< Connection enabled
};

/**
 * @brief Performance statistics for nodes
 */
struct NodePerformanceStats {
    std::atomic<uint64_t> total_samples_processed{0};
    std::atomic<uint64_t> total_processing_time_ns{0};
    std::atomic<uint32_t> dropout_count{0};
    std::atomic<uint32_t> buffer_underruns{0};
    
    // Real-time metrics
    float average_cpu_usage = 0.0f;
    float peak_cpu_usage = 0.0f;
    float current_latency_ms = 0.0f;
};

/**
 * @brief Base class for all audio processing nodes
 */
class AudioNode {
public:
    AudioNode(NodeID id, NodeType type, const std::string& name);
    virtual ~AudioNode() = default;

    // Node identification
    NodeID get_id() const { return id_; }
    NodeType get_type() const { return type_; }
    const std::string& get_name() const { return name_; }
    
    // Processing configuration
    virtual bool configure(const AudioProcessingParams& params);
    virtual bool is_configured() const { return configured_; }
    
    // Audio processing (pure virtual - must be implemented)
    virtual bool process(
        const std::vector<std::shared_ptr<AudioFrame>>& inputs,
        std::vector<std::shared_ptr<AudioFrame>>& outputs,
        const ve::TimePoint& timestamp
    ) = 0;
    
    // Node connections
    virtual uint16_t get_input_count() const = 0;
    virtual uint16_t get_output_count() const = 0;
    virtual bool can_connect_input(uint16_t input_index) const;
    virtual bool can_connect_output(uint16_t output_index) const;
    
    // Performance monitoring
    const NodePerformanceStats& get_performance_stats() const { return stats_; }
    void reset_performance_stats();
    
    // Processing priority
    void set_processing_priority(ProcessingPriority priority) { priority_ = priority; }
    ProcessingPriority get_processing_priority() const { return priority_; }
    
    // Enable/disable node
    void set_enabled(bool enabled) { enabled_ = enabled; }
    bool is_enabled() const { return enabled_; }

protected:
    // SIMD-optimized audio processing utilities
    void process_audio_simd_float32(
        const float* input, 
        float* output, 
        uint32_t sample_count,
        float gain = 1.0f
    );
    
    void mix_audio_simd_float32(
        const float* input1,
        const float* input2,
        float* output,
        uint32_t sample_count,
        float gain1 = 1.0f,
        float gain2 = 1.0f
    );
    
    // Performance tracking
    void update_performance_stats(uint64_t processing_time_ns, uint32_t sample_count);
    
    // Configuration
    AudioProcessingParams params_;
    bool configured_ = false;

protected:
    NodeID id_;
    NodeType type_;
    std::string name_;
    ProcessingPriority priority_ = ProcessingPriority::Realtime;
    std::atomic<bool> enabled_{true};
    NodePerformanceStats stats_;
};

/**
 * @brief Input node for audio sources (decoders, live input, etc.)
 */
class InputNode : public AudioNode {
public:
    InputNode(NodeID id, const std::string& name, uint16_t output_channels = 2);
    
    // AudioNode interface
    bool process(
        const std::vector<std::shared_ptr<AudioFrame>>& inputs,
        std::vector<std::shared_ptr<AudioFrame>>& outputs,
        const ve::TimePoint& timestamp
    ) override;
    
    uint16_t get_input_count() const override { return 0; }
    uint16_t get_output_count() const override { return output_channels_; }
    
    // Input-specific functionality
    void set_audio_source(std::function<std::shared_ptr<AudioFrame>(const ve::TimePoint&)> source);
    void set_gain(float gain) { gain_ = gain; }
    float get_gain() const { return gain_; }

private:
    uint16_t output_channels_;
    std::function<std::shared_ptr<AudioFrame>(const ve::TimePoint&)> audio_source_;
    float gain_ = 1.0f;
};

/**
 * @brief Mixer node for combining multiple audio inputs
 */
class MixerNode : public AudioNode {
public:
    MixerNode(NodeID id, const std::string& name, uint16_t input_count = 8, uint16_t output_channels = 2);
    
    // AudioNode interface
    bool process(
        const std::vector<std::shared_ptr<AudioFrame>>& inputs,
        std::vector<std::shared_ptr<AudioFrame>>& outputs,
        const ve::TimePoint& timestamp
    ) override;
    
    uint16_t get_input_count() const override { return input_count_; }
    uint16_t get_output_count() const override { return output_channels_; }
    
    // Configuration
    bool configure(const AudioProcessingParams& params) override;
    
    // Mixer-specific functionality
    void set_input_gain(uint16_t input_index, float gain);
    float get_input_gain(uint16_t input_index) const;
    void set_master_gain(float gain) { master_gain_ = gain; }
    float get_master_gain() const { return master_gain_; }
    
    // Panning control
    void set_input_pan(uint16_t input_index, float pan); // -1.0 (left) to +1.0 (right)
    float get_input_pan(uint16_t input_index) const;

private:
    uint16_t input_count_;
    uint16_t output_channels_;
    std::vector<float> input_gains_;
    std::vector<float> input_pans_;
    float master_gain_ = 1.0f;
    
    // SIMD-aligned mixing buffers
    alignas(32) std::vector<float> mix_buffer_;
};

/**
 * @brief Output node for audio destinations
 */
class OutputNode : public AudioNode {
public:
    OutputNode(NodeID id, const std::string& name, uint16_t input_channels = 2);
    
    // AudioNode interface
    bool process(
        const std::vector<std::shared_ptr<AudioFrame>>& inputs,
        std::vector<std::shared_ptr<AudioFrame>>& outputs,
        const ve::TimePoint& timestamp
    ) override;
    
    uint16_t get_input_count() const override { return input_channels_; }
    uint16_t get_output_count() const override { return 0; }
    
    // Output-specific functionality
    void set_audio_output(std::function<void(std::shared_ptr<AudioFrame>, const ve::TimePoint&)> output);
    void set_master_volume(float volume) { master_volume_ = volume; }
    float get_master_volume() const { return master_volume_; }

private:
    uint16_t input_channels_;
    std::function<void(std::shared_ptr<AudioFrame>, const ve::TimePoint&)> audio_output_;
    float master_volume_ = 1.0f;
};

/**
 * @brief Main mixing graph that manages nodes and connections
 */
class MixingGraph {
public:
    MixingGraph();
    ~MixingGraph();

    // Node management
    NodeID add_node(std::unique_ptr<AudioNode> node);
    bool remove_node(NodeID node_id);
    AudioNode* get_node(NodeID node_id) const;
    std::vector<NodeID> get_all_nodes() const;
    
    // Node connection management
    bool connect_nodes(NodeID source_id, uint16_t source_output, 
                      NodeID target_id, uint16_t target_input,
                      float gain = 1.0f);
    bool disconnect_nodes(NodeID source_id, uint16_t source_output,
                         NodeID target_id, uint16_t target_input);
    bool is_connected(NodeID source_id, uint16_t source_output,
                     NodeID target_id, uint16_t target_input) const;
    
    // Graph configuration
    bool configure_graph(const AudioProcessingParams& params);
    bool is_configured() const { return configured_; }
    
    // Real-time processing
    bool process_graph(const ve::TimePoint& timestamp);
    
    // Performance monitoring
    struct GraphPerformanceStats {
        uint32_t total_nodes = 0;
        uint32_t active_nodes = 0;
        uint32_t total_connections = 0;
        float total_cpu_usage = 0.0f;
        float peak_cpu_usage = 0.0f;
        uint32_t total_dropouts = 0;
        float average_latency_ms = 0.0f;
    };
    
    GraphPerformanceStats get_performance_stats() const;
    void reset_performance_stats();
    
    // Graph topology analysis
    bool has_cycles() const;
    std::vector<NodeID> get_processing_order() const;
    
    // Dynamic reconfiguration (lock-free when possible)
    bool reconfigure_without_dropouts(std::function<void()> reconfiguration_func);

private:
    // Node storage
    std::unordered_map<NodeID, std::unique_ptr<AudioNode>> nodes_;
    std::vector<NodeConnection> connections_;
    std::vector<NodeID> processing_order_;
    
    // Configuration
    AudioProcessingParams params_;
    bool configured_ = false;
    
    // Node ID generation
    std::atomic<NodeID> next_node_id_{1};
    
    // Performance tracking
    mutable GraphPerformanceStats graph_stats_;
    
    // Threading and synchronization
    mutable std::mutex graph_mutex_;
    std::atomic<bool> processing_active_{false};
    
    // Graph analysis helpers
    void rebuild_processing_order();
    bool check_for_cycles_recursive(NodeID node_id, std::unordered_set<NodeID>& visited, 
                                   std::unordered_set<NodeID>& in_stack) const;
    std::vector<NodeID> get_node_dependencies(NodeID node_id) const;
};

/**
 * @brief Factory for creating common node types
 */
class NodeFactory {
public:
    // Standard node creation
    static std::unique_ptr<InputNode> create_input_node(const std::string& name, uint16_t channels = 2);
    static std::unique_ptr<MixerNode> create_mixer_node(const std::string& name, uint16_t inputs = 8, uint16_t channels = 2);
    static std::unique_ptr<OutputNode> create_output_node(const std::string& name, uint16_t channels = 2);
    
    // Preset configurations
    static std::unique_ptr<MixingGraph> create_basic_stereo_mixer(uint16_t track_count = 8);
    static std::unique_ptr<MixingGraph> create_professional_mixer(uint16_t track_count = 16);
};

} // namespace ve::audio