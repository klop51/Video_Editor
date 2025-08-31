// Week 8: Asynchronous GPU Rendering System
// Background processing for non-blocking video effects

#pragma once

#include <future>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <chrono>

namespace ve::gfx {

// Forward declarations
class GraphicsDevice;
class RenderGraph;
struct RenderContext;
using TextureHandle = uint32_t;

/**
 * @brief Asynchronous render job for background processing
 */
struct RenderJob {
    enum class Type {
        SINGLE_EFFECT,      // Apply single effect to texture
        EFFECT_CHAIN,       // Apply multiple effects in sequence
        RENDER_GRAPH,       // Execute complete render graph
        CUSTOM_RENDER       // Custom render function
    };
    
    Type type;
    uint64_t job_id;                              // Unique job identifier
    TextureHandle input_texture;                  // Source texture
    TextureHandle output_texture;                 // Target texture (0 = allocate)
    
    // Priority and timing
    enum class Priority : int {
        LOW = 0,
        NORMAL = 1, 
        HIGH = 2,
        CRITICAL = 3
    } priority = Priority::NORMAL;
    
    std::chrono::steady_clock::time_point submission_time;
    std::chrono::steady_clock::time_point deadline;  // Optional deadline
    bool has_deadline = false;
    
    // Job-specific data
    union JobData {
        struct SingleEffect {
            int effect_type;                      // Effect type enum
            void* parameters;                     // Effect parameters
            size_t param_size;                    // Size of parameters
        } single_effect;
        
        struct EffectChain {
            int* effect_types;                    // Array of effect types
            void** parameters;                    // Array of effect parameters  
            size_t* param_sizes;                  // Size of each parameter set
            size_t effect_count;                  // Number of effects
        } effect_chain;
        
        struct RenderGraphJob {
            RenderGraph* graph;                   // Render graph to execute
            RenderContext* context;               // Render context
        } render_graph;
        
        struct CustomRender {
            std::function<bool(GraphicsDevice*, TextureHandle, TextureHandle)>* render_func;
        } custom_render;
    } data;
    
    // Completion callback
    std::function<void(bool success, TextureHandle result)> completion_callback;
    
    // Job cleanup function
    std::function<void()> cleanup_func;
    
    // Constructor helpers
    static RenderJob create_single_effect(int effect_type, const void* params, size_t param_size,
                                         TextureHandle input, TextureHandle output = 0);
    static RenderJob create_effect_chain(const std::vector<int>& effects, 
                                        const std::vector<void*>& params,
                                        const std::vector<size_t>& param_sizes,
                                        TextureHandle input, TextureHandle output = 0);
    static RenderJob create_render_graph(RenderGraph* graph, RenderContext* context,
                                        TextureHandle input, TextureHandle output = 0);
    static RenderJob create_custom_render(std::function<bool(GraphicsDevice*, TextureHandle, TextureHandle)> func,
                                         TextureHandle input, TextureHandle output = 0);
};

/**
 * @brief Performance statistics for async rendering
 */
struct AsyncRenderStats {
    size_t total_jobs_submitted = 0;
    size_t total_jobs_completed = 0;
    size_t total_jobs_failed = 0;
    size_t total_jobs_cancelled = 0;
    
    float average_job_time_ms = 0.0f;
    float peak_job_time_ms = 0.0f;
    float average_queue_wait_time_ms = 0.0f;
    
    size_t current_queue_size = 0;
    size_t peak_queue_size = 0;
    size_t active_job_count = 0;
    
    // Throughput metrics
    float jobs_per_second = 0.0f;
    float peak_jobs_per_second = 0.0f;
    
    // Job type breakdown
    size_t single_effect_jobs = 0;
    size_t effect_chain_jobs = 0;
    size_t render_graph_jobs = 0;
    size_t custom_render_jobs = 0;
    
    void reset() {
        *this = AsyncRenderStats{};
    }
};

/**
 * @brief Asynchronous rendering system for non-blocking GPU operations
 * 
 * Allows video effects and rendering operations to be queued and processed
 * in the background, preventing UI blocking during intensive GPU work.
 */
class AsyncRenderer {
public:
    /**
     * @brief Configuration for async rendering behavior
     */
    struct Config {
        size_t worker_thread_count = 2;           // Number of render threads
        size_t max_queue_size = 50;               // Maximum pending jobs
        size_t max_concurrent_jobs = 4;           // Jobs processing simultaneously
        bool enable_priority_scheduling = true;   // Use priority queue
        bool enable_deadline_scheduling = true;   // Respect job deadlines
        bool enable_job_batching = true;          // Batch compatible jobs
        uint32_t job_timeout_ms = 30000;          // 30 second job timeout
    };
    
    /**
     * @brief Create async renderer
     * @param device Graphics device for rendering
     * @param config Configuration options
     */
    explicit AsyncRenderer(GraphicsDevice* device, const Config& config = Config{});
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~AsyncRenderer();
    
    // Non-copyable, non-movable
    AsyncRenderer(const AsyncRenderer&) = delete;
    AsyncRenderer& operator=(const AsyncRenderer&) = delete;
    AsyncRenderer(AsyncRenderer&&) = delete;
    AsyncRenderer& operator=(AsyncRenderer&&) = delete;
    
    /**
     * @brief Submit render job for async processing
     * @param job Render job to execute
     * @return Future that resolves when rendering completes
     */
    std::future<TextureHandle> render_async(RenderJob job);
    
    /**
     * @brief Submit single effect for async processing
     * @param effect_type Type of effect to apply
     * @param parameters Effect parameters
     * @param param_size Size of parameters
     * @param input_texture Source texture
     * @param priority Job priority
     * @return Future that resolves to output texture
     */
    std::future<TextureHandle> apply_effect_async(int effect_type, const void* parameters, size_t param_size,
                                                  TextureHandle input_texture, 
                                                  RenderJob::Priority priority = RenderJob::Priority::NORMAL);
    
    /**
     * @brief Submit effect chain for async processing
     * @param effects List of effects to apply in sequence
     * @param parameters List of effect parameters
     * @param param_sizes Size of each parameter set
     * @param input_texture Source texture
     * @param priority Job priority
     * @return Future that resolves to output texture
     */
    std::future<TextureHandle> apply_effect_chain_async(const std::vector<int>& effects,
                                                        const std::vector<void*>& parameters,
                                                        const std::vector<size_t>& param_sizes,
                                                        TextureHandle input_texture,
                                                        RenderJob::Priority priority = RenderJob::Priority::NORMAL);
    
    /**
     * @brief Submit render graph for async processing
     * @param graph Render graph to execute
     * @param context Render context
     * @param input_texture Source texture
     * @param priority Job priority
     * @return Future that resolves to output texture
     */
    std::future<TextureHandle> render_graph_async(RenderGraph* graph, RenderContext* context,
                                                  TextureHandle input_texture,
                                                  RenderJob::Priority priority = RenderJob::Priority::NORMAL);
    
    /**
     * @brief Cancel pending jobs for specific texture
     * @param texture_handle Texture handle to cancel jobs for
     * @return Number of cancelled jobs
     */
    size_t cancel_jobs_for_texture(TextureHandle texture_handle);
    
    /**
     * @brief Cancel all pending jobs
     * @return Number of cancelled jobs
     */
    size_t cancel_all_jobs();
    
    /**
     * @brief Wait for all jobs to complete
     * @param timeout_ms Maximum time to wait (0 = no timeout)
     * @return True if all jobs completed, false if timeout
     */
    bool wait_for_completion(uint32_t timeout_ms = 0);
    
    /**
     * @brief Check if renderer is currently processing jobs
     */
    bool is_busy() const;
    
    /**
     * @brief Get current performance statistics
     */
    AsyncRenderStats get_stats() const;
    
    /**
     * @brief Reset performance statistics
     */
    void reset_stats();
    
    /**
     * @brief Update configuration (some settings require restart)
     * @param new_config New configuration
     * @return True if configuration was applied
     */
    bool update_config(const Config& new_config);
    
    /**
     * @brief Get current configuration
     */
    const Config& get_config() const { return config_; }
    
    /**
     * @brief Pause job processing (jobs remain queued)
     */
    void pause();
    
    /**
     * @brief Resume job processing
     */
    void resume();
    
    /**
     * @brief Check if renderer is paused
     */
    bool is_paused() const { return paused_.load(); }
    
    /**
     * @brief Set deadline for time-sensitive operations
     * @param deadline Time by which operation should complete
     */
    void set_deadline(std::chrono::steady_clock::time_point deadline);

private:
    /**
     * @brief Priority queue comparator for render jobs
     */
    struct JobComparator {
        bool operator()(const std::unique_ptr<RenderJob>& a, const std::unique_ptr<RenderJob>& b) const;
    };
    
    using JobQueue = std::priority_queue<std::unique_ptr<RenderJob>,
                                        std::vector<std::unique_ptr<RenderJob>>,
                                        JobComparator>;
    
    // Core functionality
    void worker_thread_main();
    bool process_render_job(std::unique_ptr<RenderJob> job);
    bool execute_single_effect(const RenderJob& job);
    bool execute_effect_chain(const RenderJob& job);
    bool execute_render_graph(const RenderJob& job);
    bool execute_custom_render(const RenderJob& job);
    
    // Queue management
    std::unique_ptr<RenderJob> get_next_job();
    bool has_pending_jobs() const;
    void cleanup_completed_jobs();
    bool should_batch_job(const RenderJob& job) const;
    std::vector<std::unique_ptr<RenderJob>> get_batchable_jobs(const RenderJob& primary_job);
    
    // Statistics and monitoring
    void update_statistics(const RenderJob& job, bool success, float duration_ms);
    void update_throughput_stats();
    
    // Configuration and state
    GraphicsDevice* device_;
    Config config_;
    
    // Threading
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> shutdown_requested_{false};
    std::atomic<bool> paused_{false};
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    std::condition_variable completion_condition_;
    
    // Job management
    JobQueue pending_jobs_;
    std::vector<std::future<bool>> in_flight_jobs_;
    std::atomic<size_t> active_job_count_{0};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    AsyncRenderStats stats_;
    std::queue<std::chrono::steady_clock::time_point> completion_times_;
    
    // Job ID generation
    std::atomic<uint64_t> next_job_id_{1};
    
    // Deadline tracking
    std::chrono::steady_clock::time_point current_deadline_;
    bool has_deadline_ = false;
};

} // namespace ve::gfx
