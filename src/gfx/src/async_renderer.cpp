// Week 8: Asynchronous Renderer Implementation
// Background GPU processing for non-blocking effects

#include "gfx/async_renderer.hpp"
#include "gfx/vk_device.hpp"
#include "gfx/render_graph.hpp"
#include "core/log.hpp"
#include <algorithm>

namespace ve::gfx {

// AsyncRenderer::JobComparator Implementation
bool AsyncRenderer::JobComparator::operator()(const std::unique_ptr<RenderJob>& a, 
                                              const std::unique_ptr<RenderJob>& b) const {
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

// AsyncRenderer Implementation
AsyncRenderer::AsyncRenderer(GraphicsDevice* device, const Config& config)
    : device_(device), config_(config) {
    
    ve::log::info("Creating AsyncRenderer with", config_.worker_thread_count, "worker threads");
    
    // Start worker threads
    for (size_t i = 0; i < config_.worker_thread_count; ++i) {
        worker_threads_.emplace_back(&AsyncRenderer::worker_thread_main, this);
    }
    
    ve::log::debug("AsyncRenderer initialized");
}

AsyncRenderer::~AsyncRenderer() {
    ve::log::debug("Shutting down AsyncRenderer");
    
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
        auto job = std::move(const_cast<std::unique_ptr<RenderJob>&>(pending_jobs_.top()));
        pending_jobs_.pop();
        
        if (job->completion_callback) {
            job->completion_callback(false, 0);  // Signal failure
        }
        
        if (job->cleanup_func) {
            job->cleanup_func();
        }
    }
    
    ve::log::info("AsyncRenderer shutdown complete");
}

std::future<TextureHandle> AsyncRenderer::render_async(RenderJob job) {
    auto promise = std::make_shared<std::promise<TextureHandle>>();
    auto future = promise->get_future();
    
    // Assign job ID
    job.job_id = next_job_id_.fetch_add(1);
    job.submission_time = std::chrono::steady_clock::now();
    
    // Wrap the original callback with promise resolution
    auto original_callback = job.completion_callback;
    job.completion_callback = [promise, original_callback](bool success, TextureHandle result) {
        if (original_callback) {
            original_callback(success, result);
        }
        promise->set_value(success ? result : 0);
    };
    
    // Queue the job
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (pending_jobs_.size() >= config_.max_queue_size) {
            ve::log::warning("Render queue full, dropping job");
            promise->set_value(0);
            return future;
        }
        
        pending_jobs_.push(std::make_unique<RenderJob>(std::move(job)));
        
        // Update statistics
        stats_.total_jobs_submitted++;
        stats_.current_queue_size = pending_jobs_.size();
        stats_.peak_queue_size = std::max(stats_.peak_queue_size, stats_.current_queue_size);
        
        // Update job type counters
        switch (pending_jobs_.top()->type) {
            case RenderJob::Type::SINGLE_EFFECT:
                stats_.single_effect_jobs++;
                break;
            case RenderJob::Type::EFFECT_CHAIN:
                stats_.effect_chain_jobs++;
                break;
            case RenderJob::Type::RENDER_GRAPH:
                stats_.render_graph_jobs++;
                break;
            case RenderJob::Type::CUSTOM_RENDER:
                stats_.custom_render_jobs++;
                break;
        }
    }
    
    queue_condition_.notify_one();
    return future;
}

std::future<TextureHandle> AsyncRenderer::apply_effect_async(int effect_type, const void* parameters, size_t param_size,
                                                             TextureHandle input_texture, RenderJob::Priority priority) {
    RenderJob job = RenderJob::create_single_effect(effect_type, parameters, param_size, input_texture, 0);
    job.priority = priority;
    return render_async(std::move(job));
}

std::future<TextureHandle> AsyncRenderer::apply_effect_chain_async(const std::vector<int>& effects,
                                                                   const std::vector<void*>& parameters,
                                                                   const std::vector<size_t>& param_sizes,
                                                                   TextureHandle input_texture,
                                                                   RenderJob::Priority priority) {
    RenderJob job = RenderJob::create_effect_chain(effects, parameters, param_sizes, input_texture, 0);
    job.priority = priority;
    return render_async(std::move(job));
}

std::future<TextureHandle> AsyncRenderer::render_graph_async(RenderGraph* graph, RenderContext* context,
                                                            TextureHandle input_texture,
                                                            RenderJob::Priority priority) {
    RenderJob job = RenderJob::create_render_graph(graph, context, input_texture, 0);
    job.priority = priority;
    return render_async(std::move(job));
}

size_t AsyncRenderer::cancel_jobs_for_texture(TextureHandle texture_handle) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // Create new queue without the cancelled jobs
    decltype(pending_jobs_) new_queue;
    size_t cancelled_count = 0;
    
    while (!pending_jobs_.empty()) {
        auto job = std::move(const_cast<std::unique_ptr<RenderJob>&>(pending_jobs_.top()));
        pending_jobs_.pop();
        
        if (job->input_texture == texture_handle || job->output_texture == texture_handle) {
            cancelled_count++;
            stats_.total_jobs_cancelled++;
            
            if (job->completion_callback) {
                job->completion_callback(false, 0);
            }
            if (job->cleanup_func) {
                job->cleanup_func();
            }
        } else {
            new_queue.push(std::move(job));
        }
    }
    
    pending_jobs_ = std::move(new_queue);
    stats_.current_queue_size = pending_jobs_.size();
    
    ve::log::debug("Cancelled", cancelled_count, "jobs for texture", texture_handle);
    return cancelled_count;
}

size_t AsyncRenderer::cancel_all_jobs() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    size_t cancelled_count = pending_jobs_.size();
    
    while (!pending_jobs_.empty()) {
        auto job = std::move(const_cast<std::unique_ptr<RenderJob>&>(pending_jobs_.top()));
        pending_jobs_.pop();
        
        stats_.total_jobs_cancelled++;
        
        if (job->completion_callback) {
            job->completion_callback(false, 0);
        }
        if (job->cleanup_func) {
            job->cleanup_func();
        }
    }
    
    stats_.current_queue_size = 0;
    ve::log::info("Cancelled all", cancelled_count, "pending jobs");
    return cancelled_count;
}

bool AsyncRenderer::wait_for_completion(uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    if (timeout_ms == 0) {
        completion_condition_.wait(lock, [this]() { 
            return pending_jobs_.empty() && active_job_count_.load() == 0; 
        });
        return true;
    } else {
        return completion_condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                            [this]() { 
                                                return pending_jobs_.empty() && active_job_count_.load() == 0; 
                                            });
    }
}

bool AsyncRenderer::is_busy() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !pending_jobs_.empty() || active_job_count_.load() > 0;
}

AsyncRenderStats AsyncRenderer::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto stats_copy = stats_;
    stats_copy.active_job_count = active_job_count_.load();
    
    return stats_copy;
}

void AsyncRenderer::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.reset();
}

void AsyncRenderer::pause() {
    paused_.store(true);
    ve::log::info("AsyncRenderer paused");
}

void AsyncRenderer::resume() {
    paused_.store(false);
    queue_condition_.notify_all();
    ve::log::info("AsyncRenderer resumed");
}

void AsyncRenderer::set_deadline(std::chrono::steady_clock::time_point deadline) {
    current_deadline_ = deadline;
    has_deadline_ = true;
}

void AsyncRenderer::worker_thread_main() {
    ve::log::debug("Render worker thread started");
    
    while (!shutdown_requested_.load()) {
        std::unique_ptr<RenderJob> job = get_next_job();
        
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
        
        // Check if we've exceeded concurrent job limit
        if (active_job_count_.load() >= config_.max_concurrent_jobs) {
            // Put job back and wait
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                pending_jobs_.push(std::move(job));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        
        // Process the job
        active_job_count_.fetch_add(1);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        bool success = process_render_job(std::move(job));
        auto end_time = std::chrono::high_resolution_clock::now();
        
        float duration_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();
        update_statistics(*job, success, duration_ms);
        
        active_job_count_.fetch_sub(1);
        
        // Notify completion
        completion_condition_.notify_all();
    }
    
    ve::log::debug("Render worker thread finished");
}

bool AsyncRenderer::process_render_job(std::unique_ptr<RenderJob> job) {
    if (!device_) {
        ve::log::error("No graphics device available for rendering");
        if (job->completion_callback) {
            job->completion_callback(false, 0);
        }
        return false;
    }
    
    // Check deadline
    if (job->has_deadline) {
        auto now = std::chrono::steady_clock::now();
        if (now > job->deadline) {
            ve::log::warning("Render job", job->job_id, "missed deadline, skipping");
            if (job->completion_callback) {
                job->completion_callback(false, 0);
            }
            return false;
        }
    }
    
    // Check global deadline
    if (has_deadline_) {
        auto now = std::chrono::steady_clock::now();
        if (now > current_deadline_) {
            ve::log::warning("Render job", job->job_id, "missed global deadline, skipping");
            if (job->completion_callback) {
                job->completion_callback(false, 0);
            }
            return false;
        }
    }
    
    // Execute the job based on type
    bool success = false;
    
    try {
        switch (job->type) {
            case RenderJob::Type::SINGLE_EFFECT:
                success = execute_single_effect(*job);
                break;
            case RenderJob::Type::EFFECT_CHAIN:
                success = execute_effect_chain(*job);
                break;
            case RenderJob::Type::RENDER_GRAPH:
                success = execute_render_graph(*job);
                break;
            case RenderJob::Type::CUSTOM_RENDER:
                success = execute_custom_render(*job);
                break;
        }
        
        if (success) {
            ve::log::debug("Successfully completed render job", job->job_id);
        } else {
            ve::log::warning("Failed to complete render job", job->job_id);
        }
    } catch (const std::exception& e) {
        ve::log::error("Exception during render job", job->job_id, ":", e.what());
        success = false;
    }
    
    // Call completion callback
    if (job->completion_callback) {
        job->completion_callback(success, job->output_texture);
    }
    
    // Cleanup
    if (job->cleanup_func) {
        job->cleanup_func();
    }
    
    return success;
}

bool AsyncRenderer::execute_single_effect(const RenderJob& job) {
    const auto& effect_data = job.data.single_effect;
    
    // Allocate output texture if needed
    TextureHandle output = job.output_texture;
    if (output == 0) {
        // Note: In real implementation, would allocate texture based on input dimensions
        ve::log::debug("Would allocate output texture for single effect");
        output = 1;  // Placeholder
    }
    
    // Apply effect based on type
    bool success = false;
    switch (effect_data.effect_type) {
        case 1: // Color correction
            ve::log::debug("Applying color correction effect");
            // success = device_->apply_color_correction(...);
            success = true;  // Placeholder
            break;
        case 2: // Blur
            ve::log::debug("Applying blur effect");
            // success = device_->apply_gaussian_blur(...);
            success = true;  // Placeholder
            break;
        case 3: // Sharpen
            ve::log::debug("Applying sharpen effect");
            // success = device_->apply_sharpen(...);
            success = true;  // Placeholder
            break;
        default:
            ve::log::warning("Unknown effect type:", effect_data.effect_type);
            break;
    }
    
    return success;
}

bool AsyncRenderer::execute_effect_chain(const RenderJob& job) {
    const auto& chain_data = job.data.effect_chain;
    
    TextureHandle current_input = job.input_texture;
    TextureHandle current_output = 0;
    
    // Process each effect in the chain
    for (size_t i = 0; i < chain_data.effect_count; ++i) {
        int effect_type = chain_data.effect_types[i];
        void* params = chain_data.parameters[i];
        size_t param_size = chain_data.param_sizes[i];
        
        // Use final output texture for last effect, intermediate for others
        if (i == chain_data.effect_count - 1) {
            current_output = job.output_texture;
        } else {
            // Allocate intermediate texture
            // current_output = device_->allocate_texture(...);
            current_output = 1;  // Placeholder
        }
        
        // Create single effect job
        RenderJob single_job = RenderJob::create_single_effect(effect_type, params, param_size,
                                                              current_input, current_output);
        
        bool success = execute_single_effect(single_job);
        if (!success) {
            ve::log::error("Effect chain failed at step", i, "effect type", effect_type);
            return false;
        }
        
        // Output becomes input for next effect
        current_input = current_output;
        
        ve::log::debug("Effect chain step", i, "completed");
    }
    
    ve::log::debug("Effect chain completed successfully");
    return true;
}

bool AsyncRenderer::execute_render_graph(const RenderJob& job) {
    const auto& graph_data = job.data.render_graph;
    
    if (!graph_data.graph || !graph_data.context) {
        ve::log::error("Invalid render graph or context");
        return false;
    }
    
    // Set input texture in context
    // graph_data.context->set_input_texture(job.input_texture);
    
    // Execute the render graph
    bool success = graph_data.graph->execute(*graph_data.context);
    
    if (success) {
        ve::log::debug("Render graph executed successfully");
        // job.output_texture = graph_data.context->get_output_texture();
    } else {
        ve::log::warning("Render graph execution failed");
    }
    
    return success;
}

bool AsyncRenderer::execute_custom_render(const RenderJob& job) {
    const auto& custom_data = job.data.custom_render;
    
    if (!custom_data.render_func) {
        ve::log::error("No custom render function provided");
        return false;
    }
    
    // Execute custom render function
    bool success = (*custom_data.render_func)(device_, job.input_texture, job.output_texture);
    
    if (success) {
        ve::log::debug("Custom render function executed successfully");
    } else {
        ve::log::warning("Custom render function failed");
    }
    
    return success;
}

std::unique_ptr<RenderJob> AsyncRenderer::get_next_job() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    
    // Wait for jobs or shutdown
    if (!queue_condition_.wait_for(lock, std::chrono::milliseconds(100),
                                  [this]() { return !pending_jobs_.empty() || shutdown_requested_.load(); })) {
        return nullptr;  // Timeout
    }
    
    if (shutdown_requested_.load() || pending_jobs_.empty()) {
        return nullptr;
    }
    
    auto job = std::move(const_cast<std::unique_ptr<RenderJob>&>(pending_jobs_.top()));
    pending_jobs_.pop();
    stats_.current_queue_size = pending_jobs_.size();
    
    return job;
}

void AsyncRenderer::update_statistics(const RenderJob& job, bool success, float duration_ms) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    if (success) {
        stats_.total_jobs_completed++;
        
        // Update timing statistics
        if (stats_.total_jobs_completed == 1) {
            stats_.average_job_time_ms = duration_ms;
        } else {
            stats_.average_job_time_ms = (stats_.average_job_time_ms * (stats_.total_jobs_completed - 1) + duration_ms) / stats_.total_jobs_completed;
        }
        stats_.peak_job_time_ms = std::max(stats_.peak_job_time_ms, duration_ms);
        
        // Queue wait time
        auto queue_wait_ms = std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - job.submission_time).count();
        
        if (stats_.total_jobs_completed == 1) {
            stats_.average_queue_wait_time_ms = queue_wait_ms;
        } else {
            stats_.average_queue_wait_time_ms = (stats_.average_queue_wait_time_ms * (stats_.total_jobs_completed - 1) + queue_wait_ms) / stats_.total_jobs_completed;
        }
    } else {
        stats_.total_jobs_failed++;
    }
    
    // Update throughput
    update_throughput_stats();
}

void AsyncRenderer::update_throughput_stats() {
    auto now = std::chrono::steady_clock::now();
    completion_times_.push(now);
    
    // Remove old completion times (keep last 5 seconds)
    while (!completion_times_.empty()) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - completion_times_.front()).count();
        if (age > 5) {
            completion_times_.pop();
        } else {
            break;
        }
    }
    
    // Calculate jobs per second
    if (!completion_times_.empty()) {
        auto time_span = std::chrono::duration<float>(now - completion_times_.front()).count();
        if (time_span > 0.0f) {
            stats_.jobs_per_second = completion_times_.size() / time_span;
            stats_.peak_jobs_per_second = std::max(stats_.peak_jobs_per_second, stats_.jobs_per_second);
        }
    }
}

} // namespace ve::gfx
