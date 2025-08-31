// Week 7: Render Graph Compiler and Optimization
// Compiles effect nodes into optimized render commands

#pragma once

#include "effect_nodes.hpp"
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace ve::gfx {

// Render command for optimized execution
struct RenderCommand {
    enum class Type {
        SET_RENDER_TARGET,
        BIND_SHADER,
        BIND_TEXTURE,
        SET_CONSTANTS,
        DRAW_FULLSCREEN_QUAD,
        CLEAR_TARGET
    };
    
    Type type;
    union {
        struct { TextureHandle target; } set_render_target;
        struct { ShaderHandle shader; } bind_shader;
        struct { int slot; TextureHandle texture; } bind_texture;
        struct { BufferHandle buffer; const void* data; size_t size; } set_constants;
        struct { float color[4]; } clear_target;
    };
};

// Graph optimization pass
class RenderGraphOptimizer {
public:
    struct OptimizationStats {
        int nodes_eliminated = 0;
        int passes_combined = 0;
        int textures_reused = 0;
        float estimated_speedup = 1.0f;
    };
    
    // Optimization passes
    std::vector<EffectNode*> optimize_graph(const std::vector<EffectNode*>& nodes);
    OptimizationStats get_last_optimization_stats() const { return last_stats_; }
    
private:
    OptimizationStats last_stats_;
    
    // Individual optimization passes
    std::vector<EffectNode*> eliminate_disabled_nodes(const std::vector<EffectNode*>& nodes);
    std::vector<EffectNode*> combine_compatible_passes(const std::vector<EffectNode*>& nodes);
    std::vector<EffectNode*> optimize_texture_usage(const std::vector<EffectNode*>& nodes);
    
    // Helper functions
    bool can_combine_nodes(EffectNode* a, EffectNode* b) const;
    bool has_dependencies(EffectNode* node, const std::vector<EffectNode*>& nodes) const;
};

// Effect cache for performance
class EffectCache {
public:
    EffectCache(size_t max_cache_size = 100);
    ~EffectCache();
    
    // Cache management
    bool is_cached(uint64_t hash) const;
    TextureHandle get_cached_result(uint64_t hash);
    void store_result(uint64_t hash, TextureHandle texture);
    
    // Invalidation
    void invalidate_all();
    void invalidate_dependencies(EffectNode* changed_node);
    
    // Statistics
    struct CacheStats {
        int hits = 0;
        int misses = 0;
        int evictions = 0;
        float hit_rate = 0.0f;
        size_t memory_used = 0;
    };
    
    CacheStats get_stats() const { return stats_; }
    void reset_stats() { stats_ = CacheStats{}; }
    
private:
    struct CacheEntry {
        uint64_t hash;
        TextureHandle texture;
        uint64_t last_access_time;
        size_t memory_size;
    };
    
    std::unordered_map<uint64_t, CacheEntry> cache_;
    size_t max_cache_size_;
    size_t current_cache_size_;
    uint64_t access_counter_;
    CacheStats stats_;
    
    void evict_least_recently_used();
    uint64_t get_current_time() const;
};

// Main render graph compiler
class RenderGraphCompiler {
public:
    RenderGraphCompiler(GraphicsDevice* device);
    ~RenderGraphCompiler();
    
    // Graph compilation
    std::vector<RenderCommand> compile(const std::vector<EffectNode*>& nodes);
    void execute(const std::vector<RenderCommand>& commands, RenderContext& ctx);
    
    // Configuration
    void set_optimization_enabled(bool enabled) { optimization_enabled_ = enabled; }
    void set_caching_enabled(bool enabled) { caching_enabled_ = enabled; }
    void set_max_intermediate_textures(int count) { max_intermediate_textures_ = count; }
    
    // Resource management
    void allocate_intermediate_textures(int width, int height);
    void cleanup_intermediate_textures();
    
    // Statistics
    struct CompilerStats {
        int nodes_compiled = 0;
        int commands_generated = 0;
        int optimizations_applied = 0;
        float compilation_time_ms = 0.0f;
        float execution_time_ms = 0.0f;
        EffectCache::CacheStats cache_stats;
        RenderGraphOptimizer::OptimizationStats optimization_stats;
    };
    
    CompilerStats get_stats() const { return stats_; }
    void reset_stats() { stats_ = CompilerStats{}; }
    
private:
    GraphicsDevice* device_;
    std::unique_ptr<RenderGraphOptimizer> optimizer_;
    std::unique_ptr<EffectCache> cache_;
    
    // Configuration
    bool optimization_enabled_ = true;
    bool caching_enabled_ = true;
    int max_intermediate_textures_ = 8;
    
    // Resources
    std::vector<TextureHandle> intermediate_textures_;
    int texture_width_ = 1920;
    int texture_height_ = 1080;
    
    // Statistics
    CompilerStats stats_;
    
    // Internal compilation methods
    std::vector<RenderCommand> compile_node(EffectNode* node, RenderContext& ctx);
    void validate_graph(const std::vector<EffectNode*>& nodes);
    std::vector<EffectNode*> topological_sort(const std::vector<EffectNode*>& nodes);
    TextureHandle allocate_output_texture(EffectNode* node);
    
    // Command generation helpers
    void add_set_render_target_command(std::vector<RenderCommand>& commands, TextureHandle target);
    void add_bind_shader_command(std::vector<RenderCommand>& commands, ShaderHandle shader);
    void add_bind_texture_command(std::vector<RenderCommand>& commands, int slot, TextureHandle texture);
    void add_set_constants_command(std::vector<RenderCommand>& commands, BufferHandle buffer, const void* data, size_t size);
    void add_draw_command(std::vector<RenderCommand>& commands);
    void add_clear_command(std::vector<RenderCommand>& commands, float r, float g, float b, float a);
};

// High-level render graph for easy usage
class RenderGraph {
public:
    RenderGraph(GraphicsDevice* device);
    ~RenderGraph();
    
    // Node management
    ColorCorrectionNode* add_color_correction();
    BlurNode* add_blur();
    TransformNode* add_transform();
    LUTNode* add_lut();
    MixNode* add_mix();
    
    // Graph operations
    void connect(EffectNode* source, EffectNode* destination, int input_slot = 0);
    void disconnect(EffectNode* destination, int input_slot = 0);
    void remove_node(EffectNode* node);
    void clear();
    
    // Execution
    TextureHandle render(TextureHandle input_texture, int width, int height);
    void render_to_target(TextureHandle input_texture, TextureHandle output_texture, int width, int height);
    
    // Graph query
    std::vector<EffectNode*> get_all_nodes() const;
    std::vector<EffectNode*> get_root_nodes() const;  // Nodes with no inputs
    std::vector<EffectNode*> get_leaf_nodes() const;  // Nodes with no outputs
    
    // Performance
    RenderGraphCompiler::CompilerStats get_performance_stats() const;
    void enable_optimization(bool enabled);
    void enable_caching(bool enabled);
    
private:
    GraphicsDevice* device_;
    std::unique_ptr<RenderGraphCompiler> compiler_;
    std::vector<std::unique_ptr<EffectNode>> nodes_;
    std::unordered_map<EffectNode*, std::vector<std::pair<EffectNode*, int>>> connections_;
    
    void validate_connection(EffectNode* source, EffectNode* destination, int input_slot);
    std::vector<EffectNode*> build_execution_order();
};

} // namespace ve::gfx
