// Week 8: Streaming Texture Upload System
// Efficient background texture streaming for large video files

#pragma once

#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <memory>

namespace ve::gfx {

// Forward declarations
class GraphicsDevice;
using TextureHandle = uint32_t;

/**
 * @brief Upload job for streaming texture data to GPU
 */
struct UploadJob {
    TextureHandle target;                           // Target texture to upload to
    std::unique_ptr<uint8_t[]> data;               // Source data (ownership transferred)
    size_t data_size;                              // Size of data in bytes
    uint32_t width;                                // Texture dimensions
    uint32_t height;
    uint32_t bytes_per_pixel;                      // Format information
    std::function<void(bool success)> completion_callback;  // Called when upload completes
    
    // Optional upload region
    struct UploadRegion {
        uint32_t x, y;                             // Offset
        uint32_t width, height;                    // Region size
    } region;
    bool use_region = false;                       // Whether to use partial upload
    
    // Priority system
    enum class Priority : int {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        CRITICAL = 3
    } priority = Priority::NORMAL;
    
    // Timing information
    std::chrono::steady_clock::time_point submission_time;
    std::chrono::steady_clock::time_point deadline;  // Optional deadline for upload
    bool has_deadline = false;
};

/**
 * @brief Statistics for upload performance monitoring
 */
struct UploadStats {
    size_t total_uploads_submitted = 0;
    size_t total_uploads_completed = 0;
    size_t total_uploads_failed = 0;
    size_t total_bytes_uploaded = 0;
    
    float average_upload_time_ms = 0.0f;
    float peak_upload_time_ms = 0.0f;
    float average_queue_wait_time_ms = 0.0f;
    
    size_t current_queue_size = 0;
    size_t peak_queue_size = 0;
    
    // Bandwidth monitoring
    float current_upload_bandwidth_mbps = 0.0f;
    float peak_upload_bandwidth_mbps = 0.0f;
    
    void reset() {
        *this = UploadStats{};
    }
};

/**
 * @brief Background texture streaming system for efficient GPU uploads
 * 
 * Handles large texture uploads asynchronously to prevent blocking the main thread.
 * Supports priority queuing, deadline scheduling, and comprehensive performance monitoring.
 */
class StreamingTextureUploader {
public:
    /**
     * @brief Configuration options for the uploader
     */
    struct Config {
        size_t max_queue_size = 100;               // Maximum pending uploads
        size_t worker_thread_count = 1;            // Number of upload threads
        size_t max_concurrent_uploads = 4;         // Uploads in progress simultaneously
        size_t upload_chunk_size = 64 * 1024 * 1024;  // 64MB chunks for large uploads
        bool enable_compression = false;            // Compress uploads when beneficial
        bool enable_priority_scheduling = true;    // Use priority queue
        bool enable_deadline_scheduling = true;    // Respect upload deadlines
    };

    /**
     * @brief Create streaming uploader
     * @param device Graphics device for GPU uploads
     * @param config Configuration options
     */
    explicit StreamingTextureUploader(GraphicsDevice* device, const Config& config = Config{});
    
    /**
     * @brief Destructor - ensures clean shutdown
     */
    ~StreamingTextureUploader();
    
    // Non-copyable, non-movable
    StreamingTextureUploader(const StreamingTextureUploader&) = delete;
    StreamingTextureUploader& operator=(const StreamingTextureUploader&) = delete;
    StreamingTextureUploader(StreamingTextureUploader&&) = delete;
    StreamingTextureUploader& operator=(StreamingTextureUploader&&) = delete;
    
    /**
     * @brief Queue texture upload job
     * @param job Upload job with data and completion callback
     * @return Future that resolves when upload completes
     */
    std::future<bool> queue_upload(UploadJob job);
    
    /**
     * @brief Queue simple texture upload with data copy
     * @param target Target texture handle
     * @param data Source data (will be copied)
     * @param width Texture width
     * @param height Texture height
     * @param bytes_per_pixel Format information
     * @param priority Upload priority
     * @return Future that resolves when upload completes
     */
    std::future<bool> queue_upload(TextureHandle target, const uint8_t* data, size_t data_size,
                                  uint32_t width, uint32_t height, uint32_t bytes_per_pixel,
                                  UploadJob::Priority priority = UploadJob::Priority::NORMAL);
    
    /**
     * @brief Cancel pending upload if not yet started
     * @param target Texture handle to cancel uploads for
     * @return Number of cancelled uploads
     */
    size_t cancel_uploads(TextureHandle target);
    
    /**
     * @brief Cancel all pending uploads
     * @return Number of cancelled uploads
     */
    size_t cancel_all_uploads();
    
    /**
     * @brief Wait for all pending uploads to complete
     * @param timeout_ms Maximum time to wait in milliseconds (0 = no timeout)
     * @return True if all uploads completed, false if timeout
     */
    bool wait_for_completion(uint32_t timeout_ms = 0);
    
    /**
     * @brief Check if uploader is currently processing uploads
     */
    bool is_busy() const;
    
    /**
     * @brief Get current upload performance statistics
     */
    UploadStats get_stats() const;
    
    /**
     * @brief Reset performance statistics
     */
    void reset_stats();
    
    /**
     * @brief Update configuration (some settings require restart)
     * @param new_config New configuration
     * @return True if configuration was applied successfully
     */
    bool update_config(const Config& new_config);
    
    /**
     * @brief Get current configuration
     */
    const Config& get_config() const { return config_; }
    
    /**
     * @brief Pause upload processing (uploads remain queued)
     */
    void pause();
    
    /**
     * @brief Resume upload processing
     */
    void resume();
    
    /**
     * @brief Check if uploader is paused
     */
    bool is_paused() const { return paused_.load(); }

private:
    /**
     * @brief Priority queue comparator for upload jobs
     */
    struct JobComparator {
        bool operator()(const std::unique_ptr<UploadJob>& a, const std::unique_ptr<UploadJob>& b) const;
    };
    
    using JobQueue = std::priority_queue<std::unique_ptr<UploadJob>, 
                                        std::vector<std::unique_ptr<UploadJob>>, 
                                        JobComparator>;
    
    // Core functionality
    void worker_thread_main();
    bool process_upload_job(std::unique_ptr<UploadJob> job);
    void update_statistics(const UploadJob& job, bool success, float duration_ms);
    bool should_compress_upload(const UploadJob& job) const;
    std::unique_ptr<uint8_t[]> compress_data(const uint8_t* data, size_t data_size, size_t& compressed_size);
    
    // Queue management
    std::unique_ptr<UploadJob> get_next_job();
    bool has_pending_jobs() const;
    void cleanup_completed_jobs();
    
    // Bandwidth monitoring
    void update_bandwidth_stats(size_t bytes_uploaded, float duration_ms);
    
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
    
    // Job queue and tracking
    JobQueue pending_jobs_;
    std::vector<std::future<bool>> in_flight_jobs_;
    mutable std::mutex stats_mutex_;
    
    // Statistics
    UploadStats stats_;
    std::queue<std::pair<size_t, std::chrono::steady_clock::time_point>> bandwidth_samples_;
    
    // Job ID generation
    std::atomic<uint64_t> next_job_id_{1};
};

} // namespace ve::gfx
