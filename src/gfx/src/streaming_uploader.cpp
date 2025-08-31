// Week 8: Streaming Texture Upload Implementation
// Background texture streaming system

#include "gfx/streaming_uploader.hpp"
#include "gfx/vk_device.hpp"
#include "core/log.hpp"
#include <algorithm>
#include <chrono>

namespace ve::gfx {

// UploadJob static factory methods
RenderJob RenderJob::create_single_effect(int effect_type, const void* params, size_t param_size,
                                         TextureHandle input, TextureHandle output) {
    RenderJob job;
    job.type = Type::SINGLE_EFFECT;
    job.input_texture = input;
    job.output_texture = output;
    job.data.single_effect.effect_type = effect_type;
    job.data.single_effect.parameters = malloc(param_size);
    memcpy(job.data.single_effect.parameters, params, param_size);
    job.data.single_effect.param_size = param_size;
    job.cleanup_func = [job]() {
        free(job.data.single_effect.parameters);
    };
    return job;
}

RenderJob RenderJob::create_effect_chain(const std::vector<int>& effects, 
                                        const std::vector<void*>& params,
                                        const std::vector<size_t>& param_sizes,
                                        TextureHandle input, TextureHandle output) {
    RenderJob job;
    job.type = Type::EFFECT_CHAIN;
    job.input_texture = input;
    job.output_texture = output;
    
    size_t count = effects.size();
    job.data.effect_chain.effect_count = count;
    job.data.effect_chain.effect_types = new int[count];
    job.data.effect_chain.parameters = new void*[count];
    job.data.effect_chain.param_sizes = new size_t[count];
    
    for (size_t i = 0; i < count; ++i) {
        job.data.effect_chain.effect_types[i] = effects[i];
        job.data.effect_chain.param_sizes[i] = param_sizes[i];
        job.data.effect_chain.parameters[i] = malloc(param_sizes[i]);
        memcpy(job.data.effect_chain.parameters[i], params[i], param_sizes[i]);
    }
    
    job.cleanup_func = [job]() {
        for (size_t i = 0; i < job.data.effect_chain.effect_count; ++i) {
            free(job.data.effect_chain.parameters[i]);
        }
        delete[] job.data.effect_chain.effect_types;
        delete[] job.data.effect_chain.parameters;
        delete[] job.data.effect_chain.param_sizes;
    };
    
    return job;
}

// StreamingTextureUploader::JobComparator Implementation
bool StreamingTextureUploader::JobComparator::operator()(const std::unique_ptr<UploadJob>& a, 
                                                         const std::unique_ptr<UploadJob>& b) const {
    // Higher priority jobs come first (reverse comparison for priority queue)
    if (a->priority != b->priority) {
        return static_cast<int>(a->priority) < static_cast<int>(b->priority);
    }
    
    // Among same priority, deadline jobs come first
    if (a->has_deadline && !b->has_deadline) return false;
    if (!a->has_deadline && b->has_deadline) return true;
    
    // Among deadline jobs, earlier deadlines come first
    if (a->has_deadline && b->has_deadline) {
        return a->deadline > b->deadline;
    }
    
    // Otherwise, FIFO (earlier submission comes first)
    return a->submission_time > b->submission_time;
}

// StreamingTextureUploader Implementation
StreamingTextureUploader::StreamingTextureUploader(GraphicsDevice* device, const Config& config)
    : device_(device), config_(config) {
    
    ve::log::info("Creating StreamingTextureUploader with", config_.worker_thread_count, "worker threads");
    
    // Start worker threads
    for (size_t i = 0; i < config_.worker_thread_count; ++i) {
        worker_threads_.emplace_back(&StreamingTextureUploader::worker_thread_main, this);
    }
    
    ve::log::debug("StreamingTextureUploader initialized");
}

StreamingTextureUploader::~StreamingTextureUploader() {
    ve::log::debug("Shutting down StreamingTextureUploader");
    
    // Signal shutdown
    shutdown_requested_.store(true);
    queue_condition_.notify_all();
    
    // Wait for worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    // Clean up remaining jobs
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!pending_jobs_.empty()) {
        auto job = std::move(const_cast<std::unique_ptr<UploadJob>&>(pending_jobs_.top()));
        pending_jobs_.pop();
        if (job->completion_callback) {
            job->completion_callback(false);  // Signal failure
        }
    }
    
    ve::log::info("StreamingTextureUploader shutdown complete");
}

std::future<bool> StreamingTextureUploader::queue_upload(UploadJob job) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    
    // Set submission time
    job.submission_time = std::chrono::steady_clock::now();
    
    // Wrap the original callback with promise resolution
    auto original_callback = job.completion_callback;
    job.completion_callback = [promise, original_callback](bool success) {
        if (original_callback) {
            original_callback(success);
        }
        promise->set_value(success);
    };
    
    // Queue the job
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (pending_jobs_.size() >= config_.max_queue_size) {
            ve::log::warning("Upload queue full, dropping job");
            promise->set_value(false);
            return future;
        }
        
        pending_jobs_.push(std::make_unique<UploadJob>(std::move(job)));
        stats_.total_uploads_submitted++;
        stats_.current_queue_size = pending_jobs_.size();
        stats_.peak_queue_size = std::max(stats_.peak_queue_size, stats_.current_queue_size);
    }
    
    queue_condition_.notify_one();
    return future;
}

std::future<bool> StreamingTextureUploader::queue_upload(TextureHandle target, const uint8_t* data, size_t data_size,
                                                         uint32_t width, uint32_t height, uint32_t bytes_per_pixel,
                                                         UploadJob::Priority priority) {
    UploadJob job;
    job.target = target;
    job.data = std::make_unique<uint8_t[]>(data_size);
    memcpy(job.data.get(), data, data_size);
    job.data_size = data_size;
    job.width = width;
    job.height = height;
    job.bytes_per_pixel = bytes_per_pixel;
    job.priority = priority;
    
    return queue_upload(std::move(job));
}

size_t StreamingTextureUploader::cancel_uploads(TextureHandle target) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // Create new queue without the cancelled jobs
    decltype(pending_jobs_) new_queue;
    size_t cancelled_count = 0;
    
    while (!pending_jobs_.empty()) {
        auto job = std::move(const_cast<std::unique_ptr<UploadJob>&>(pending_jobs_.top()));
        pending_jobs_.pop();
        
        if (job->target == target) {
            cancelled_count++;
            if (job->completion_callback) {
                job->completion_callback(false);
            }
        } else {
            new_queue.push(std::move(job));
        }
    }
    
    pending_jobs_ = std::move(new_queue);
    stats_.current_queue_size = pending_jobs_.size();
    
    ve::log::debug("Cancelled", cancelled_count, "uploads for texture", target);
    return cancelled_count;
}

size_t StreamingTextureUploader::cancel_all_uploads() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    size_t cancelled_count = pending_jobs_.size();
    
    while (!pending_jobs_.empty()) {
        auto job = std::move(const_cast<std::unique_ptr<UploadJob>&>(pending_jobs_.top()));
        pending_jobs_.pop();
        if (job->completion_callback) {
            job->completion_callback(false);
        }
    }
    
    stats_.current_queue_size = 0;
    ve::log::info("Cancelled all", cancelled_count, "pending uploads");
    return cancelled_count;
}

bool StreamingTextureUploader::wait_for_completion(uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    if (timeout_ms == 0) {
        completion_condition_.wait(lock, [this]() { return pending_jobs_.empty(); });
        return true;
    } else {
        return completion_condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                            [this]() { return pending_jobs_.empty(); });
    }
}

bool StreamingTextureUploader::is_busy() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !pending_jobs_.empty();
}

UploadStats StreamingTextureUploader::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void StreamingTextureUploader::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.reset();
}

void StreamingTextureUploader::pause() {
    paused_.store(true);
    ve::log::info("StreamingTextureUploader paused");
}

void StreamingTextureUploader::resume() {
    paused_.store(false);
    queue_condition_.notify_all();
    ve::log::info("StreamingTextureUploader resumed");
}

void StreamingTextureUploader::worker_thread_main() {
    ve::log::debug("Upload worker thread started");
    
    while (!shutdown_requested_.load()) {
        std::unique_ptr<UploadJob> job = get_next_job();
        
        if (!job) {
            continue;  // Timeout or shutdown
        }
        
        if (paused_.load()) {
            // Put job back in queue and wait
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                pending_jobs_.push(std::move(job));
            }
            
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_condition_.wait(lock, [this]() { return !paused_.load() || shutdown_requested_.load(); });
            continue;
        }
        
        // Process the job
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = process_upload_job(std::move(job));
        auto end_time = std::chrono::high_resolution_clock::now();
        
        float duration_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
        update_statistics(*job, success, duration_ms);
    }
    
    ve::log::debug("Upload worker thread finished");
}

bool StreamingTextureUploader::process_upload_job(std::unique_ptr<UploadJob> job) {
    if (!device_) {
        ve::log::error("No graphics device available for upload");
        if (job->completion_callback) {
            job->completion_callback(false);
        }
        return false;
    }
    
    // Check deadline
    if (job->has_deadline) {
        auto now = std::chrono::steady_clock::now();
        if (now > job->deadline) {
            ve::log::warning("Upload job missed deadline, skipping");
            if (job->completion_callback) {
                job->completion_callback(false);
            }
            return false;
        }
    }
    
    // Perform the upload
    bool success = false;
    
    try {
        if (job->use_region) {
            // Partial texture update
            success = device_->update_texture_region(
                job->target, 
                job->data.get(), 
                job->region.x, job->region.y,
                job->region.width, job->region.height,
                job->bytes_per_pixel
            );
        } else {
            // Full texture update
            success = device_->update_texture(
                job->target,
                job->data.get(),
                job->width,
                job->height,
                job->bytes_per_pixel
            );
        }
        
        if (success) {
            ve::log::debug("Successfully uploaded", job->data_size, "bytes to texture", job->target);
        } else {
            ve::log::warning("Failed to upload texture", job->target);
        }
    } catch (const std::exception& e) {
        ve::log::error("Exception during texture upload:", e.what());
        success = false;
    }
    
    // Call completion callback
    if (job->completion_callback) {
        job->completion_callback(success);
    }
    
    return success;
}

std::unique_ptr<UploadJob> StreamingTextureUploader::get_next_job() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // Wait for jobs or shutdown
    if (!queue_condition_.wait_for(lock, std::chrono::milliseconds(100),
                                  [this]() { return !pending_jobs_.empty() || shutdown_requested_.load(); })) {
        return nullptr;  // Timeout
    }
    
    if (shutdown_requested_.load() || pending_jobs_.empty()) {
        return nullptr;
    }
    
    auto job = std::move(const_cast<std::unique_ptr<UploadJob>&>(pending_jobs_.top()));
    pending_jobs_.pop();
    stats_.current_queue_size = pending_jobs_.size();
    
    return job;
}

void StreamingTextureUploader::update_statistics(const UploadJob& job, bool success, float duration_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (success) {
        stats_.total_uploads_completed++;
        stats_.total_bytes_uploaded += job.data_size;
        
        // Update timing statistics
        if (stats_.total_uploads_completed == 1) {
            stats_.average_upload_time_ms = duration_ms;
        } else {
            stats_.average_upload_time_ms = (stats_.average_upload_time_ms * (stats_.total_uploads_completed - 1) + duration_ms) / stats_.total_uploads_completed;
        }
        stats_.peak_upload_time_ms = std::max(stats_.peak_upload_time_ms, duration_ms);
        
        // Update bandwidth
        update_bandwidth_stats(job.data_size, duration_ms);
        
        // Queue wait time
        auto queue_wait_ms = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - job.submission_time).count();
        
        if (stats_.total_uploads_completed == 1) {
            stats_.average_queue_wait_time_ms = queue_wait_ms;
        } else {
            stats_.average_queue_wait_time_ms = (stats_.average_queue_wait_time_ms * (stats_.total_uploads_completed - 1) + queue_wait_ms) / stats_.total_uploads_completed;
        }
    } else {
        stats_.total_uploads_failed++;
    }
}

void StreamingTextureUploader::update_bandwidth_stats(size_t bytes_uploaded, float duration_ms) {
    if (duration_ms <= 0.0f) return;
    
    float mbps = (bytes_uploaded / (1024.0f * 1024.0f)) / (duration_ms / 1000.0f);
    
    auto now = std::chrono::steady_clock::now();
    bandwidth_samples_.push({bytes_uploaded, now});
    
    // Remove old samples (keep last 5 seconds)
    while (!bandwidth_samples_.empty()) {
        auto sample_age = std::chrono::duration_cast<std::chrono::seconds>(now - bandwidth_samples_.front().second).count();
        if (sample_age > 5) {
            bandwidth_samples_.pop();
        } else {
            break;
        }
    }
    
    // Calculate current bandwidth from recent samples
    if (!bandwidth_samples_.empty()) {
        size_t total_bytes = 0;
        for (auto tmp = bandwidth_samples_; !tmp.empty(); tmp.pop()) {
            total_bytes += tmp.front().first;
        }
        
        auto time_span = std::chrono::duration<float>(now - bandwidth_samples_.front().second).count();
        if (time_span > 0.0f) {
            stats_.current_upload_bandwidth_mbps = (total_bytes / (1024.0f * 1024.0f)) / time_span;
        }
    }
    
    stats_.peak_upload_bandwidth_mbps = std::max(stats_.peak_upload_bandwidth_mbps, mbps);
}

} // namespace ve::gfx
