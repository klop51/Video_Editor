/**
 * @file mixing_graph.cpp
 * @brief Implementation of Node-Based Audio Mixing Architecture
 * 
 * Phase 2 Week 4: Advanced Mixing Graph Implementation
 * 
 * Provides professional-grade audio mixing with SIMD-optimized processing,
 * dynamic graph reconfiguration, and real-time performance monitoring.
 */

#include "audio/mixing_graph.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <set>

namespace ve::audio {

//==============================================================================
// AudioNode Implementation
//==============================================================================

AudioNode::AudioNode(NodeID id, NodeType type, const std::string& name)
    : id_(id), type_(type), name_(name) {
}

bool AudioNode::configure(const AudioProcessingParams& params) {
    params_ = params;
    configured_ = true;
    
    ve::log::info("Configured audio node '" + name_ + "' - " +
                  std::to_string(params.sample_rate) + "Hz, " +
                  std::to_string(params.channels) + " channels, " +
                  std::to_string(params.buffer_size) + " samples");
    
    return true;
}

bool AudioNode::can_connect_input(uint16_t input_index) const {
    return input_index < get_input_count();
}

bool AudioNode::can_connect_output(uint16_t output_index) const {
    return output_index < get_output_count();
}

void AudioNode::reset_performance_stats() {
    stats_.total_samples_processed = 0;
    stats_.total_processing_time_ns = 0;
    stats_.dropout_count = 0;
    stats_.buffer_underruns = 0;
    stats_.average_cpu_usage = 0.0f;
    stats_.peak_cpu_usage = 0.0f;
    stats_.current_latency_ms = 0.0f;
}

void AudioNode::process_audio_simd_float32(
    const float* input, 
    float* output, 
    uint32_t sample_count,
    float gain) {
    
    if (!params_.enable_simd || !params_.enable_avx) {
        // Fallback to scalar processing
        for (uint32_t i = 0; i < sample_count; ++i) {
            output[i] = input[i] * gain;
        }
        return;
    }
    
#ifdef __AVX__
    const __m256 gain_vec = _mm256_set1_ps(gain);
    const uint32_t simd_count = sample_count & ~7; // Process 8 samples at a time
    
    for (uint32_t i = 0; i < simd_count; i += 8) {
        __m256 input_vec = _mm256_load_ps(&input[i]);
        __m256 result = _mm256_mul_ps(input_vec, gain_vec);
        _mm256_store_ps(&output[i], result);
    }
    
    // Handle remaining samples
    for (uint32_t i = simd_count; i < sample_count; ++i) {
        output[i] = input[i] * gain;
    }
#else
    // Fallback when AVX not available
    for (uint32_t i = 0; i < sample_count; ++i) {
        output[i] = input[i] * gain;
    }
#endif
}

void AudioNode::mix_audio_simd_float32(
    const float* input1,
    const float* input2,
    float* output,
    uint32_t sample_count,
    float gain1,
    float gain2) {
    
    if (!params_.enable_simd || !params_.enable_avx) {
        // Fallback to scalar processing
        for (uint32_t i = 0; i < sample_count; ++i) {
            output[i] = (input1[i] * gain1) + (input2[i] * gain2);
        }
        return;
    }
    
#ifdef __AVX__
    const __m256 gain1_vec = _mm256_set1_ps(gain1);
    const __m256 gain2_vec = _mm256_set1_ps(gain2);
    const uint32_t simd_count = sample_count & ~7;
    
    for (uint32_t i = 0; i < simd_count; i += 8) {
        __m256 input1_vec = _mm256_load_ps(&input1[i]);
        __m256 input2_vec = _mm256_load_ps(&input2[i]);
        __m256 scaled1 = _mm256_mul_ps(input1_vec, gain1_vec);
        __m256 scaled2 = _mm256_mul_ps(input2_vec, gain2_vec);
        __m256 result = _mm256_add_ps(scaled1, scaled2);
        _mm256_store_ps(&output[i], result);
    }
    
    // Handle remaining samples
    for (uint32_t i = simd_count; i < sample_count; ++i) {
        output[i] = (input1[i] * gain1) + (input2[i] * gain2);
    }
#else
    // Fallback when AVX not available
    for (uint32_t i = 0; i < sample_count; ++i) {
        output[i] = (input1[i] * gain1) + (input2[i] * gain2);
    }
#endif
}

void AudioNode::update_performance_stats(uint64_t processing_time_ns, uint32_t sample_count) {
    stats_.total_samples_processed += sample_count;
    stats_.total_processing_time_ns += processing_time_ns;
    
    // Calculate CPU usage (processing time vs real-time duration)
    double sample_duration_ns = (static_cast<double>(sample_count) / params_.sample_rate) * 1e9;
    float cpu_usage = static_cast<float>(processing_time_ns / sample_duration_ns) * 100.0f;
    
    stats_.average_cpu_usage = (stats_.average_cpu_usage * 0.9f) + (cpu_usage * 0.1f);
    if (cpu_usage > stats_.peak_cpu_usage) {
        stats_.peak_cpu_usage = cpu_usage;
    }
}

//==============================================================================
// InputNode Implementation
//==============================================================================

InputNode::InputNode(NodeID id, const std::string& name, uint16_t output_channels)
    : AudioNode(id, NodeType::Input, name), output_channels_(output_channels) {
}

bool InputNode::process(
    const std::vector<std::shared_ptr<AudioFrame>>& inputs,
    std::vector<std::shared_ptr<AudioFrame>>& outputs,
    const ve::TimePoint& timestamp) {
    
    if (!is_enabled() || !audio_source_) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Get audio from source
        auto frame = audio_source_(timestamp);
        if (!frame) {
            return false;
        }
        
        // Apply gain if needed
        if (gain_ != 1.0f) {
            // Apply gain to audio data
            float* data = reinterpret_cast<float*>(frame->data());
            uint32_t total_samples = frame->sample_count() * frame->channel_count();
            
            process_audio_simd_float32(data, data, total_samples, gain_);
        }
        
        outputs.resize(1);
        outputs[0] = frame;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        update_performance_stats(duration.count(), frame->sample_count());
        
        return true;
        
    } catch (const std::exception& e) {
        ve::log::error("InputNode '" + get_name() + "' processing failed: " + e.what());
        stats_.dropout_count++;
        return false;
    }
}

void InputNode::set_audio_source(std::function<std::shared_ptr<AudioFrame>(const ve::TimePoint&)> source) {
    audio_source_ = source;
}

//==============================================================================
// MixerNode Implementation
//==============================================================================

MixerNode::MixerNode(NodeID id, const std::string& name, uint16_t input_count, uint16_t output_channels)
    : AudioNode(id, NodeType::Mixer, name), 
      input_count_(input_count), 
      output_channels_(output_channels),
      input_gains_(input_count, 1.0f),
      input_pans_(input_count, 0.0f) {
}

bool MixerNode::configure(const AudioProcessingParams& params) {
    if (!AudioNode::configure(params)) {
        return false;
    }
    
    // Allocate SIMD-aligned mixing buffer
    uint32_t buffer_samples = params.buffer_size * output_channels_;
    mix_buffer_.resize(buffer_samples);
    
    return true;
}

bool MixerNode::process(
    const std::vector<std::shared_ptr<AudioFrame>>& inputs,
    std::vector<std::shared_ptr<AudioFrame>>& outputs,
    const ve::TimePoint& timestamp) {
    
    if (!is_enabled() || !is_configured()) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        // Clear mixing buffer
        std::fill(mix_buffer_.begin(), mix_buffer_.end(), 0.0f);
        
        uint32_t samples_per_channel = 0;
        uint32_t sample_rate = params_.sample_rate;
        
        // Mix all input signals
        for (size_t i = 0; i < inputs.size() && i < input_count_; ++i) {
            if (!inputs[i]) continue;
            
            samples_per_channel = inputs[i]->sample_count();
            sample_rate = inputs[i]->sample_rate();
            
            const float* input_data = reinterpret_cast<const float*>(inputs[i]->data());
            uint16_t input_channels = inputs[i]->channel_count();
            
            float gain = input_gains_[i] * master_gain_;
            float pan = input_pans_[i];
            
            // Mix with panning for stereo output
            if (output_channels_ == 2 && input_channels >= 1) {
                float left_gain = gain * (1.0f - std::max(0.0f, pan));
                float right_gain = gain * (1.0f + std::min(0.0f, pan));
                
                for (uint32_t sample = 0; sample < samples_per_channel; ++sample) {
                    float mono_sample = input_data[sample * input_channels];
                    mix_buffer_[sample * 2] += mono_sample * left_gain;     // Left
                    mix_buffer_[sample * 2 + 1] += mono_sample * right_gain; // Right
                }
            } else {
                // Direct channel mapping
                uint16_t channels_to_copy = std::min(input_channels, output_channels_);
                for (uint32_t sample = 0; sample < samples_per_channel; ++sample) {
                    for (uint16_t ch = 0; ch < channels_to_copy; ++ch) {
                        mix_buffer_[sample * output_channels_ + ch] += 
                            input_data[sample * input_channels + ch] * gain;
                    }
                }
            }
        }
        
        // Create output frame
        if (samples_per_channel > 0) {
            auto output_frame = AudioFrame::create_from_data(
                sample_rate,
                output_channels_,
                samples_per_channel,
                SampleFormat::Float32,
                timestamp,
                mix_buffer_.data(),
                mix_buffer_.size() * sizeof(float)
            );
            
            if (output_frame) {
                outputs.resize(1);
                outputs[0] = output_frame;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        update_performance_stats(duration.count(), samples_per_channel);
        
        return true;
        
    } catch (const std::exception& e) {
        ve::log::error("MixerNode '" + get_name() + "' processing failed: " + e.what());
        stats_.dropout_count++;
        return false;
    }
}

void MixerNode::set_input_gain(uint16_t input_index, float gain) {
    if (input_index < input_gains_.size()) {
        input_gains_[input_index] = std::max(0.0f, gain);
    }
}

float MixerNode::get_input_gain(uint16_t input_index) const {
    return (input_index < input_gains_.size()) ? input_gains_[input_index] : 0.0f;
}

void MixerNode::set_input_pan(uint16_t input_index, float pan) {
    if (input_index < input_pans_.size()) {
        input_pans_[input_index] = std::clamp(pan, -1.0f, 1.0f);
    }
}

float MixerNode::get_input_pan(uint16_t input_index) const {
    return (input_index < input_pans_.size()) ? input_pans_[input_index] : 0.0f;
}

//==============================================================================
// OutputNode Implementation
//==============================================================================

OutputNode::OutputNode(NodeID id, const std::string& name, uint16_t input_channels)
    : AudioNode(id, NodeType::Output, name), input_channels_(input_channels) {
}

bool OutputNode::process(
    const std::vector<std::shared_ptr<AudioFrame>>& inputs,
    std::vector<std::shared_ptr<AudioFrame>>& outputs,
    const ve::TimePoint& timestamp) {
    
    if (!is_enabled() || !audio_output_ || inputs.empty() || !inputs[0]) {
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    try {
        auto frame = inputs[0];
        
        // Apply master volume if needed
        if (master_volume_ != 1.0f) {
            float* data = reinterpret_cast<float*>(frame->data());
            uint32_t total_samples = frame->sample_count() * frame->channel_count();
            
            process_audio_simd_float32(data, data, total_samples, master_volume_);
        }
        
        // Send to output
        audio_output_(frame, timestamp);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        update_performance_stats(duration.count(), frame->sample_count());
        
        return true;
        
    } catch (const std::exception& e) {
        ve::log::error("OutputNode '" + get_name() + "' processing failed: " + e.what());
        stats_.dropout_count++;
        return false;
    }
}

void OutputNode::set_audio_output(std::function<void(std::shared_ptr<AudioFrame>, const ve::TimePoint&)> output) {
    audio_output_ = output;
}

//==============================================================================
// MixingGraph Implementation
//==============================================================================

MixingGraph::MixingGraph() {
}

MixingGraph::~MixingGraph() {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    nodes_.clear();
    connections_.clear();
}

NodeID MixingGraph::add_node(std::unique_ptr<AudioNode> node) {
    if (!node) {
        return INVALID_NODE_ID;
    }
    
    std::lock_guard<std::mutex> lock(graph_mutex_);
    
    NodeID node_id = node->get_id();
    nodes_[node_id] = std::move(node);
    
    if (configured_) {
        nodes_[node_id]->configure(params_);
    }
    
    rebuild_processing_order();
    
    ve::log::info("Added node '" + nodes_[node_id]->get_name() + "' to mixing graph");
    
    return node_id;
}

bool MixingGraph::remove_node(NodeID node_id) {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    
    auto it = nodes_.find(node_id);
    if (it == nodes_.end()) {
        return false;
    }
    
    // Remove all connections involving this node
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
            [node_id](const NodeConnection& conn) {
                return conn.source_node == node_id || conn.target_node == node_id;
            }),
        connections_.end()
    );
    
    ve::log::info("Removed node '" + it->second->get_name() + "' from mixing graph");
    nodes_.erase(it);
    rebuild_processing_order();
    
    return true;
}

AudioNode* MixingGraph::get_node(NodeID node_id) const {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    auto it = nodes_.find(node_id);
    return (it != nodes_.end()) ? it->second.get() : nullptr;
}

std::vector<NodeID> MixingGraph::get_all_nodes() const {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    std::vector<NodeID> node_ids;
    node_ids.reserve(nodes_.size());
    
    for (const auto& [id, node] : nodes_) {
        node_ids.push_back(id);
    }
    
    return node_ids;
}

bool MixingGraph::connect_nodes(NodeID source_id, uint16_t source_output, 
                               NodeID target_id, uint16_t target_input,
                               float gain) {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    
    auto source_it = nodes_.find(source_id);
    auto target_it = nodes_.find(target_id);
    
    if (source_it == nodes_.end() || target_it == nodes_.end()) {
        return false;
    }
    
    AudioNode* source_node = source_it->second.get();
    AudioNode* target_node = target_it->second.get();
    
    if (!source_node->can_connect_output(source_output) ||
        !target_node->can_connect_input(target_input)) {
        return false;
    }
    
    // Check if connection already exists
    for (const auto& conn : connections_) {
        if (conn.source_node == source_id && conn.source_output == source_output &&
            conn.target_node == target_id && conn.target_input == target_input) {
            return false; // Connection already exists
        }
    }
    
    NodeConnection connection;
    connection.source_node = source_id;
    connection.target_node = target_id;
    connection.source_output = source_output;
    connection.target_input = target_input;
    connection.gain = gain;
    connection.enabled = true;
    
    connections_.push_back(connection);
    rebuild_processing_order();
    
    ve::log::info("Connected '" + source_node->get_name() + "' to '" + target_node->get_name() + "'");
    
    return true;
}

bool MixingGraph::configure_graph(const AudioProcessingParams& params) {
    std::lock_guard<std::mutex> lock(graph_mutex_);
    
    params_ = params;
    
    for (auto& [id, node] : nodes_) {
        if (!node->configure(params)) {
            ve::log::error("Failed to configure node '" + node->get_name() + "'");
            return false;
        }
    }
    
    configured_ = true;
    ve::log::info("Mixing graph configured successfully");
    
    return true;
}

bool MixingGraph::process_graph(const ve::TimePoint& timestamp) {
    if (!configured_ || processing_active_.exchange(true)) {
        return false; // Already processing or not configured
    }
    
    try {
        std::lock_guard<std::mutex> lock(graph_mutex_);
        
        // Process nodes in topological order
        for (NodeID node_id : processing_order_) {
            auto it = nodes_.find(node_id);
            if (it == nodes_.end() || !it->second->is_enabled()) {
                continue;
            }
            
            AudioNode* node = it->second.get();
            
            // Collect inputs for this node
            std::vector<std::shared_ptr<AudioFrame>> inputs;
            for (const auto& conn : connections_) {
                if (conn.target_node == node_id && conn.enabled) {
                    // Get output from source node (simplified - would need proper frame routing)
                    inputs.resize(std::max(inputs.size(), static_cast<size_t>(conn.target_input + 1)));
                }
            }
            
            // Process the node
            std::vector<std::shared_ptr<AudioFrame>> outputs;
            if (!node->process(inputs, outputs, timestamp)) {
                ve::log::warn("Node '" + node->get_name() + "' failed to process");
            }
            
            // Store outputs for dependent nodes (simplified implementation)
        }
        
        processing_active_ = false;
        return true;
        
    } catch (const std::exception& e) {
        processing_active_ = false;
        ve::log::error("Graph processing failed: " + std::string(e.what()));
        return false;
    }
}

void MixingGraph::rebuild_processing_order() {
    processing_order_.clear();
    
    // Simple topological sort implementation
    std::set<NodeID> remaining_nodes;
    for (const auto& [id, node] : nodes_) {
        remaining_nodes.insert(id);
    }
    
    while (!remaining_nodes.empty()) {
        NodeID next_node = INVALID_NODE_ID;
        
        // Find a node with no dependencies in remaining set
        for (NodeID node_id : remaining_nodes) {
            bool has_dependency = false;
            for (const auto& conn : connections_) {
                if (conn.target_node == node_id && 
                    remaining_nodes.count(conn.source_node) > 0) {
                    has_dependency = true;
                    break;
                }
            }
            
            if (!has_dependency) {
                next_node = node_id;
                break;
            }
        }
        
        if (next_node == INVALID_NODE_ID) {
            ve::log::error("Cycle detected in mixing graph!");
            break;
        }
        
        processing_order_.push_back(next_node);
        remaining_nodes.erase(next_node);
    }
}

//==============================================================================
// NodeFactory Implementation
//==============================================================================

std::unique_ptr<InputNode> NodeFactory::create_input_node(const std::string& name, uint16_t channels) {
    static std::atomic<NodeID> next_id{1};
    return std::make_unique<InputNode>(next_id++, name, channels);
}

std::unique_ptr<MixerNode> NodeFactory::create_mixer_node(const std::string& name, uint16_t inputs, uint16_t channels) {
    static std::atomic<NodeID> next_id{1000}; // Different range for mixers
    return std::make_unique<MixerNode>(next_id++, name, inputs, channels);
}

std::unique_ptr<OutputNode> NodeFactory::create_output_node(const std::string& name, uint16_t channels) {
    static std::atomic<NodeID> next_id{2000}; // Different range for outputs
    return std::make_unique<OutputNode>(next_id++, name, channels);
}

std::unique_ptr<MixingGraph> NodeFactory::create_basic_stereo_mixer(uint16_t track_count) {
    auto graph = std::make_unique<MixingGraph>();
    
    // Create main mixer
    auto mixer = create_mixer_node("Main Mixer", track_count, 2);
    NodeID mixer_id = graph->add_node(std::move(mixer));
    
    // Create output
    auto output = create_output_node("Master Output", 2);
    NodeID output_id = graph->add_node(std::move(output));
    
    // Connect mixer to output
    graph->connect_nodes(mixer_id, 0, output_id, 0);
    
    return graph;
}

} // namespace ve::audio