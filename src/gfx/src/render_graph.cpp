// Week 7: Render Graph Implementation  
// Compiler, optimization, and caching system

#include "gfx/render_graph.hpp"
#include "gfx/effect_nodes.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <unordered_set>
#include <chrono>

namespace ve::gfx {

// RenderGraphCompiler Implementation
RenderGraphCompiler::RenderGraphCompiler() {
    ve::log::debug("Created RenderGraphCompiler");
}

RenderGraphCompiler::~RenderGraphCompiler() {
    clear();
}

bool RenderGraphCompiler::compile(const std::vector<EffectNode*>& nodes, CompiledGraph& output) {
    clear();
    
    if (nodes.empty()) {
        ve::log::warning("Empty node list in render graph compilation");
        return false;
    }
    
    // Validate node connections
    if (!validate_graph(nodes)) {
        ve::log::error("Graph validation failed");
        return false;
    }
    
    // Build execution order based on dependencies
    std::vector<EffectNode*> execution_order;
    if (!topological_sort(nodes, execution_order)) {
        ve::log::error("Cyclic dependency detected in render graph");
        return false;
    }
    
    // Group nodes into passes for efficient GPU execution
    std::vector<RenderPass> passes;
    create_render_passes(execution_order, passes);
    
    // Store compiled result
    output.execution_order = std::move(execution_order);
    output.render_passes = std::move(passes);
    output.is_valid = true;
    
    // Cache compiled graph
    uint64_t graph_hash = compute_graph_hash(nodes);
    compiled_graphs_[graph_hash] = output;
    
    ve::log::info("Compiled render graph with", output.execution_order.size(), "nodes in", 
                  output.render_passes.size(), "passes");
    return true;
}

bool RenderGraphCompiler::execute(const CompiledGraph& graph, RenderContext& ctx) {
    if (!graph.is_valid || graph.execution_order.empty()) {
        ve::log::error("Invalid compiled graph");
        return false;
    }
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset render context for execution
    ctx.effects_rendered = 0;
    ctx.texture_allocations = 0;
    
    // Execute each render pass
    for (const auto& pass : graph.render_passes) {
        if (!execute_render_pass(pass, ctx)) {
            ve::log::error("Render pass execution failed");
            return false;
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    ve::log::debug("Executed render graph:", ctx.effects_rendered, "effects,", 
                   ctx.texture_allocations, "textures,", duration.count(), "μs");
    return true;
}

void RenderGraphCompiler::clear() {
    compiled_graphs_.clear();
}

bool RenderGraphCompiler::validate_graph(const std::vector<EffectNode*>& nodes) {
    std::unordered_set<EffectNode*> node_set(nodes.begin(), nodes.end());
    
    for (EffectNode* node : nodes) {
        if (!node) {
            ve::log::error("Null node in graph");
            return false;
        }
        
        if (!node->is_enabled()) {
            continue;  // Skip disabled nodes
        }
        
        // Check if node has valid inputs if required
        if (node->requires_input() && node->get_output() == 0) {
            ve::log::error("Node requires input but has no valid texture");
            return false;
        }
    }
    
    return true;
}

bool RenderGraphCompiler::topological_sort(const std::vector<EffectNode*>& nodes, 
                                          std::vector<EffectNode*>& sorted) {
    std::unordered_set<EffectNode*> visited;
    std::unordered_set<EffectNode*> visiting;
    
    for (EffectNode* node : nodes) {
        if (visited.find(node) == visited.end()) {
            if (!dfs_visit(node, nodes, visited, visiting, sorted)) {
                return false;  // Cycle detected
            }
        }
    }
    
    std::reverse(sorted.begin(), sorted.end());
    return true;
}

bool RenderGraphCompiler::dfs_visit(EffectNode* node, const std::vector<EffectNode*>& all_nodes,
                                   std::unordered_set<EffectNode*>& visited,
                                   std::unordered_set<EffectNode*>& visiting,
                                   std::vector<EffectNode*>& sorted) {
    if (visiting.find(node) != visiting.end()) {
        return false;  // Cycle detected
    }
    
    if (visited.find(node) != visited.end()) {
        return true;  // Already processed
    }
    
    visiting.insert(node);
    
    // Visit dependencies (nodes that provide input to this node)
    for (EffectNode* dependency : all_nodes) {
        if (dependency != node && is_dependency(dependency, node)) {
            if (!dfs_visit(dependency, all_nodes, visited, visiting, sorted)) {
                return false;
            }
        }
    }
    
    visiting.erase(node);
    visited.insert(node);
    sorted.push_back(node);
    
    return true;
}

bool RenderGraphCompiler::is_dependency(EffectNode* from, EffectNode* to) {
    // Check if 'from' node's output is used as input to 'to' node
    TextureHandle from_output = from->get_output();
    return from_output != 0 && to->has_input(from_output);
}

void RenderGraphCompiler::create_render_passes(const std::vector<EffectNode*>& execution_order,
                                               std::vector<RenderPass>& passes) {
    if (execution_order.empty()) {
        return;
    }
    
    // Simple strategy: create one pass per node for now
    // TODO: Optimize by grouping compatible nodes into same pass
    for (EffectNode* node : execution_order) {
        if (node->is_enabled()) {
            RenderPass pass;
            pass.nodes.push_back(node);
            pass.can_execute_parallel = false;  // Conservative for now
            passes.push_back(pass);
        }
    }
    
    ve::log::debug("Created", passes.size(), "render passes");
}

bool RenderGraphCompiler::execute_render_pass(const RenderPass& pass, RenderContext& ctx) {
    if (pass.nodes.empty()) {
        return true;
    }
    
    // Execute nodes in the pass
    for (EffectNode* node : pass.nodes) {
        if (node && node->is_enabled()) {
            node->render(ctx);
        }
    }
    
    return true;
}

uint64_t RenderGraphCompiler::compute_graph_hash(const std::vector<EffectNode*>& nodes) {
    uint64_t hash = 0;
    for (EffectNode* node : nodes) {
        if (node) {
            hash ^= node->get_hash() + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
    }
    return hash;
}

// EffectCache Implementation
EffectCache::EffectCache() : max_cache_size_(100) {
    ve::log::debug("Created EffectCache with max size:", max_cache_size_);
}

bool EffectCache::get_cached_result(uint64_t effect_hash, TextureHandle& output) {
    auto it = cache_.find(effect_hash);
    if (it != cache_.end()) {
        output = it->second.texture;
        it->second.last_used = std::chrono::steady_clock::now();
        cache_hits_++;
        return true;
    }
    
    cache_misses_++;
    return false;
}

void EffectCache::cache_result(uint64_t effect_hash, TextureHandle texture) {
    if (cache_.size() >= max_cache_size_) {
        evict_oldest();
    }
    
    CacheEntry entry;
    entry.texture = texture;
    entry.last_used = std::chrono::steady_clock::now();
    
    cache_[effect_hash] = entry;
}

void EffectCache::invalidate(uint64_t effect_hash) {
    auto it = cache_.find(effect_hash);
    if (it != cache_.end()) {
        // Note: Should also release texture resource here
        cache_.erase(it);
    }
}

void EffectCache::clear() {
    cache_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
}

size_t EffectCache::get_size() const {
    return cache_.size();
}

float EffectCache::get_hit_rate() const {
    size_t total = cache_hits_ + cache_misses_;
    return total > 0 ? static_cast<float>(cache_hits_) / total : 0.0f;
}

void EffectCache::evict_oldest() {
    if (cache_.empty()) {
        return;
    }
    
    auto oldest = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.last_used < oldest->second.last_used) {
            oldest = it;
        }
    }
    
    cache_.erase(oldest);
}

// RenderGraphOptimizer Implementation
RenderGraphOptimizer::RenderGraphOptimizer() {
    ve::log::debug("Created RenderGraphOptimizer");
}

void RenderGraphOptimizer::optimize(CompiledGraph& graph) {
    if (!graph.is_valid || graph.execution_order.empty()) {
        return;
    }
    
    ve::log::debug("Optimizing render graph with", graph.execution_order.size(), "nodes");
    
    // Apply optimization passes
    remove_redundant_nodes(graph);
    merge_compatible_passes(graph);
    reorder_for_cache_efficiency(graph);
    
    ve::log::debug("Optimization complete");
}

void RenderGraphOptimizer::remove_redundant_nodes(CompiledGraph& graph) {
    // Remove disabled nodes
    auto it = std::remove_if(graph.execution_order.begin(), graph.execution_order.end(),
                            [](EffectNode* node) { return !node || !node->is_enabled(); });
    graph.execution_order.erase(it, graph.execution_order.end());
    
    // Remove nodes with no effect (identity transforms, zero-strength effects, etc.)
    // TODO: Implement based on specific node types
}

void RenderGraphOptimizer::merge_compatible_passes(CompiledGraph& graph) {
    // TODO: Identify passes that can be merged for better GPU utilization
    // For now, keep simple one-node-per-pass structure
}

void RenderGraphOptimizer::reorder_for_cache_efficiency(CompiledGraph& graph) {
    // TODO: Reorder nodes to maximize texture cache hits
    // Consider grouping nodes that operate on similar texture regions
}

// RenderGraph Implementation
RenderGraph::RenderGraph() {
    ve::log::debug("Created RenderGraph");
}

RenderGraph::~RenderGraph() {
    clear();
}

void RenderGraph::add_color_correction(const ColorCorrectionNode::Params& params) {
    auto node = std::make_unique<ColorCorrectionNode>();
    node->set_params(params);
    nodes_.push_back(std::move(node));
}

void RenderGraph::add_blur(const BlurNode::Params& params) {
    auto node = std::make_unique<BlurNode>();
    node->set_params(params);
    nodes_.push_back(std::move(node));
}

void RenderGraph::add_transform(const TransformNode::Params& params) {
    auto node = std::make_unique<TransformNode>();
    node->set_params(params);
    nodes_.push_back(std::move(node));
}

void RenderGraph::add_lut(TextureHandle lut_texture, float strength) {
    auto node = std::make_unique<LUTNode>();
    LUTNode::Params params;
    params.strength = strength;
    node->set_params(params);
    node->set_input(1, lut_texture);  // Slot 1 for LUT texture
    nodes_.push_back(std::move(node));
}

void RenderGraph::add_mix(MixNode::BlendMode mode, float opacity) {
    auto node = std::make_unique<MixNode>();
    MixNode::Params params;
    params.blend_mode = mode;
    params.opacity = opacity;
    node->set_params(params);
    nodes_.push_back(std::move(node));
}

bool RenderGraph::compile() {
    // Convert unique_ptr vector to raw pointer vector for compiler
    std::vector<EffectNode*> raw_nodes;
    for (const auto& node : nodes_) {
        raw_nodes.push_back(node.get());
    }
    
    bool success = compiler_.compile(raw_nodes, compiled_graph_);
    if (success) {
        optimizer_.optimize(compiled_graph_);
    }
    
    return success;
}

bool RenderGraph::execute(RenderContext& ctx) {
    if (!compiled_graph_.is_valid) {
        ve::log::error("Cannot execute uncompiled render graph");
        return false;
    }
    
    return compiler_.execute(compiled_graph_, ctx);
}

void RenderGraph::clear() {
    nodes_.clear();
    compiled_graph_ = CompiledGraph{};
    cache_.clear();
}

size_t RenderGraph::get_node_count() const {
    return nodes_.size();
}

bool RenderGraph::is_compiled() const {
    return compiled_graph_.is_valid;
}

EffectCache& RenderGraph::get_cache() {
    return cache_;
}

} // namespace ve::gfx
