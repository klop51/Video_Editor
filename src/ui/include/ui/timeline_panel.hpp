#pragma once
#include "../../timeline/include/timeline/timeline.hpp"
#include "../../core/include/core/time.hpp"
#include "../../audio/include/audio/waveform_cache.h"
#include "../../audio/include/audio/waveform_generator.h"
#include <QWidget>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QMenu> // Ensure QMenu complete type available for context menu implementation
#include <memory>
#include <functional>
#include <limits>
#include <unordered_map>
#include <chrono>

// Forward declarations
namespace ve::commands { class Command; }
namespace ve::playback { class PlaybackController; }

namespace ve::ui {

class TimelinePanel : public QWidget {
    Q_OBJECT
    
public:
    explicit TimelinePanel(QWidget* parent = nullptr);
    
    void set_timeline(ve::timeline::Timeline* timeline);
    void set_zoom(double zoom_factor);
    void set_current_time(ve::TimePoint time);
    
    // Video playback integration
    void set_playback_controller(ve::playback::PlaybackController* controller);
    
    // Command system integration
    using CommandExecutor = std::function<bool(std::unique_ptr<ve::commands::Command>)>;
    void set_command_executor(CommandExecutor executor);
    
    // View parameters
    double zoom_factor() const { return zoom_factor_; }
    ve::TimePoint current_time() const { return current_time_; }
    
public slots:
    void refresh();
    void zoom_in();
    void zoom_out();
    void zoom_fit();
    
    // Phase 1: Override update for throttled painting
    void update();
    
    // Editing operations
    void cut_selected_segments();
    void copy_selected_segments();
    void paste_segments();
    void delete_selected_segments();
    void split_segment_at_playhead();
    
    // Undo/Redo operations
    void request_undo();
    void request_redo();
    
signals:
    void time_changed(ve::TimePoint time);
    void selection_changed();
    void track_height_changed();
    void clip_added(const QString& filePath, ve::TimePoint start_time, int track_index);
    
    // Editing signals
    void segments_cut();
    void segments_deleted();
    void segments_added(); // For paste operations
    void segment_split(ve::timeline::SegmentId segment_id, ve::TimePoint split_time);
    
    // Command system signals
    void undo_requested();
    void redo_requested();
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    
    // Hover events for proper mouse highlighting
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    
private:
    // Clipboard for copy/paste operations
    struct ClipboardSegment {
        ve::timeline::Segment segment;
        size_t original_track_index;
        ve::TimePoint relative_start_time; // Relative to clipboard reference point
    };
    std::vector<ClipboardSegment> clipboard_segments_;
    
    // Drawing methods
    void draw_background(QPainter& painter);
    void draw_timecode_ruler(QPainter& painter);
    void draw_tracks(QPainter& painter);
    void draw_track(QPainter& painter, const ve::timeline::Track& track, int track_y);
    void draw_segments(QPainter& painter, const ve::timeline::Track& track, int track_y);
    void draw_playhead(QPainter& painter);
    void draw_selection(QPainter& painter);
    void draw_drag_preview(QPainter& painter);
    
    // Enhanced drawing methods
    void draw_audio_waveform(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment);
    void draw_cached_waveform(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment);
    
    // Enhanced waveform integration helper functions
    void draw_placeholder_waveform(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment);
    void draw_video_thumbnail(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment);
    void draw_segment_handles(QPainter& painter, const QRect& rect);
    
    // Advanced Timeline Features (Phase 2)
    // Drag-and-drop with snap-to-grid
    ve::TimePoint snap_to_grid(ve::TimePoint time) const;
    ve::TimePoint snap_to_segments(ve::TimePoint time, ve::timeline::SegmentId exclude_id = 0) const;
    void draw_snap_guides(QPainter& painter) const;
    bool is_snap_enabled() const { return snap_enabled_; }
    void set_snap_enabled(bool enabled) { snap_enabled_ = enabled; }
    
    // Segment trimming handles
    bool is_on_segment_edge(const QPoint& pos, const ve::timeline::Segment& segment, bool& is_left_edge) const;
    void draw_segment_resize_handles(QPainter& painter, const ve::timeline::Segment& segment, int track_y) const;
    
    // Multi-segment selection
    void select_segments_in_range(ve::TimePoint start, ve::TimePoint end, int track_index = -1);
    void toggle_segment_selection(ve::timeline::SegmentId segment_id);
    void clear_selection() { selected_segments_.clear(); emit selection_changed(); }
    bool is_segment_selected(ve::timeline::SegmentId segment_id) const;
    
    // Ripple editing
    void set_ripple_mode(bool enabled) { ripple_mode_ = enabled; }
    bool is_ripple_mode() const { return ripple_mode_; }
    void ripple_edit_segments(ve::TimePoint edit_point, ve::TimeDuration delta);
    
    // Helper functions
    ve::TimeDuration abs_time_difference(ve::TimePoint a, ve::TimePoint b) const;
    ve::TimeDuration pixel_to_time_delta(double pixels) const;
    
    // Segment rendering cache helpers
    QPixmap* get_cached_segment(uint32_t segment_id, const QRect& rect) const;
    void cache_segment(uint32_t segment_id, const QRect& rect, const QPixmap& pixmap) const;
    void clear_segment_cache() const;
    
    // Phase 1 performance optimizations
    void init_paint_objects() const;                    // Initialize pre-allocated objects
    void set_heavy_operation_mode(bool enabled);       // Enable/disable heavy operation mode
    void request_throttled_update();                   // Request throttled paint update
    void draw_minimal_timeline(QPainter& painter);     // Ultra-fast minimal timeline
    bool should_skip_expensive_features() const;       // Check if we should skip expensive rendering
    
    // Phase 2 performance optimizations
    void invalidate_region(const QRect& rect, bool needs_full_redraw = false);  // Mark region as dirty
    void invalidate_track(size_t track_index);         // Mark entire track as dirty
    void invalidate_segment(uint32_t segment_id);      // Mark specific segment as dirty
    
    // Level-of-detail rendering enum (needed for batching)
    enum class DetailLevel { MINIMAL, BASIC, NORMAL, DETAILED };
    
    // Phase 3 performance optimizations - Segment batching
    struct SegmentBatch {
        QColor color;                              // Shared color for batch
        DetailLevel detail_level;                  // Shared detail level
        bool is_selected;                         // Selection state
        std::vector<const ve::timeline::Segment*> segments; // Segments in this batch
        std::vector<QRect> rects;                 // Corresponding rectangles
    };
    void draw_segments_batched(QPainter& painter, const ve::timeline::Track& track, int track_y);
    void create_segment_batches(const std::vector<const ve::timeline::Segment*>& visible_segments, 
                               const ve::timeline::Track& track, int track_y,
                               std::vector<SegmentBatch>& batches);
    void draw_segment_batch(QPainter& painter, const SegmentBatch& batch);
    
    // Enhanced viewport culling optimizations
    struct ViewportInfo {
        int left_x, right_x;           // Viewport boundaries in pixels
        ve::TimePoint start_time, end_time;  // Viewport boundaries in time
        int top_y, bottom_y;           // Vertical viewport boundaries
        double time_to_pixel_ratio;    // Cached conversion ratio
    };
    ViewportInfo calculate_viewport_info() const;
    bool is_track_visible(int track_y, const ViewportInfo& viewport) const;
    std::vector<const ve::timeline::Segment*> cull_segments_optimized(
        const std::vector<ve::timeline::Segment>& segments, const ViewportInfo& viewport) const;
    void clear_dirty_regions();                       // Clear all dirty regions
    bool is_region_dirty(const QRect& rect) const;   // Check if region needs repaint
    
    // Default empty timeline drawing (requires ViewportInfo definition)
    void draw_default_empty_tracks(QPainter& painter, const ViewportInfo& viewport);
    void draw_empty_track(QPainter& painter, const std::string& track_name, bool is_video, int track_y);
    
    // Level-of-detail calculation method
    DetailLevel calculate_detail_level(int segment_width, double zoom_factor) const;
    
    // Coordinate conversion
    ve::TimePoint pixel_to_time(int x) const;
    int time_to_pixel(ve::TimePoint time) const;
    int track_at_y(int y) const;
    int track_y_position(size_t track_index) const;
    
    // Interaction helpers
    void start_drag(const QPoint&);
    void update_drag(const QPoint& pos);
    void end_drag(const QPoint&);
    void finish_segment_edit(const QPoint&);
    void handle_click(const QPoint& pos);
    void handle_context_menu(const QPoint& pos);
    void update_cursor(const QPoint& pos);
    void cancel_drag_operations(); // Cancel any stuck drag states
    
    // Segment interaction
    ve::timeline::Segment* find_segment_at_pos(const QPoint& pos);
    bool is_on_segment_edge(const QPoint& pos, const ve::timeline::Segment& segment, bool& is_left_edge);
    
    // Navigation helpers
    void seek_relative(double seconds);
    void jump_to_previous_clip();
    void jump_to_next_clip();
    void jump_to_end();
    
    // Visual parameters
    static constexpr int TRACK_HEIGHT = 60;
    static constexpr int TRACK_SPACING = 2;
    static constexpr int TIMECODE_HEIGHT = 30;
    static constexpr int PLAYHEAD_WIDTH = 2;
    static constexpr int MIN_PIXELS_PER_SECOND = 10;
    static constexpr int MAX_PIXELS_PER_SECOND = 1000;
    
    // Data
    ve::timeline::Timeline* timeline_;
    ve::TimePoint current_time_;
    double zoom_factor_;
    
    // Video playback integration
    ve::playback::PlaybackController* playback_controller_;
    
    // Command system integration
    CommandExecutor command_executor_;
    
    // View state
    int scroll_x_;
    bool dragging_;
    QPoint drag_start_;
    QPoint last_mouse_pos_;
    
    // Enhanced drag state for editing operations
    ve::timeline::SegmentId dragged_segment_id_;
    bool dragging_segment_;
    bool resizing_segment_;
    bool is_left_resize_;
    ve::TimePoint original_segment_start_;
    ve::TimeDuration original_segment_duration_;
    
    // Drag preview state (for visual feedback without modifying data)
    ve::TimePoint preview_start_time_;
    ve::TimeDuration preview_duration_;
    bool show_drag_preview_;
    
    // Selection
    std::vector<ve::timeline::SegmentId> selected_segments_;
    bool selecting_range_;
    ve::TimePoint selection_start_;
    ve::TimePoint selection_end_;
    
    // Advanced Timeline Features (Phase 2)
    // Snap-to-grid and snap-to-segments
    bool snap_enabled_;
    ve::TimeDuration grid_size_;
    mutable std::vector<ve::TimePoint> snap_points_;  // Cached snap points for current frame
    
    // Ripple editing
    bool ripple_mode_;
    
    // Multi-selection with rubber band
    bool rubber_band_selecting_;
    QRect rubber_band_rect_;
    QPoint rubber_band_start_;
    
    // Enhanced drag preview with multiple segments
    std::vector<ve::timeline::SegmentId> preview_segments_;
    std::vector<ve::TimePoint> preview_positions_;
    
    // Colors
    QColor track_color_video_;
    QColor track_color_audio_;
    QColor segment_color_;
    QColor segment_selected_color_;
    QColor playhead_color_;
    QColor background_color_;
    QColor grid_color_;
    QColor snap_guide_color_;
    QColor rubber_band_color_;
    
    // Debug tools
    QTimer* heartbeat_timer_;
    
    // Waveform caching and generation (placeholder for future implementation)
    // std::unique_ptr<ve::audio::WaveformCache> waveform_cache_;
    // std::unique_ptr<ve::audio::WaveformGenerator> waveform_generator_;
    
    // Update optimization
    QTimer* update_timer_;  // For batching timeline UI updates
    QTimer* throttle_timer_; // Enhanced throttling for heavy operations
    bool pending_heavy_update_; // Flag to prevent excessive heavy redraws
    int segments_being_added_; // Track when multiple segments are being added
    
    // Enhanced throttling for Phase 1 optimizations
    static constexpr int HEAVY_OPERATION_FPS = 15;  // 66ms between paints during heavy ops
    static constexpr int NORMAL_FPS = 60;           // 16ms between paints normally
    bool heavy_operation_mode_;                     // Current operation mode
    QTimer* paint_throttle_timer_;                  // Timer for throttled painting
    bool pending_paint_request_;                    // Flag for pending paint
    
    // Simple segment rendering cache
    struct SegmentCacheEntry {
        uint32_t segment_id;
        QRect rect;
        QPixmap cached_pixmap;
        int zoom_level; // Cache per zoom level
    };
    mutable std::vector<SegmentCacheEntry> segment_cache_;
    mutable int cache_zoom_level_; // Current zoom level for cache validation
    
    // Pre-allocated paint objects (Phase 1 optimization)
    mutable QColor cached_video_color_;
    mutable QColor cached_audio_color_;
    mutable QColor cached_selected_color_;
    mutable QColor cached_text_color_;
    mutable QPen cached_border_pen_;
    mutable QPen cached_grid_pen_;
    mutable QBrush cached_segment_brush_;
    mutable QFont cached_name_font_;
    mutable QFont cached_small_font_;
    mutable QFontMetrics cached_font_metrics_;
    mutable bool paint_objects_initialized_;
    
    // Phase 4: Advanced paint state caching system
    struct PaintStateCache {
        // Current QPainter state tracking
        QColor current_pen_color;
        QColor current_brush_color;
        qreal current_pen_width;
        Qt::PenStyle current_pen_style;
        QFont current_font;
        bool has_valid_state;
        
        // State change counters for optimization analysis
        mutable int pen_changes;
        mutable int brush_changes;
        mutable int font_changes;
        mutable int total_state_changes;
        
        PaintStateCache() : current_pen_width(1.0), current_pen_style(Qt::SolidLine), 
                           has_valid_state(false), pen_changes(0), brush_changes(0), 
                           font_changes(0), total_state_changes(0) {}
                           
        void reset() {
            has_valid_state = false;
            pen_changes = brush_changes = font_changes = total_state_changes = 0;
        }
    };
    mutable PaintStateCache paint_state_cache_;
    
    // Smart state management methods
    void apply_pen_if_needed(QPainter& painter, const QColor& color, qreal width = 1.0, 
                            Qt::PenStyle style = Qt::SolidLine) const;
    void apply_brush_if_needed(QPainter& painter, const QColor& color) const;
    void apply_font_if_needed(QPainter& painter, const QFont& font) const;
    void reset_paint_state_cache() const;
    
    // Phase 3: Advanced caching and progressive rendering
    struct CachedTrackData {
        std::vector<const ve::timeline::Segment*> visible_segments;
        QRect bounds;
        uint64_t version;
        double zoom_level;
        int scroll_x;
        std::chrono::steady_clock::time_point last_update;
        
        bool is_valid(uint64_t timeline_version, double current_zoom, int current_scroll) const {
            return version == timeline_version && 
                   std::abs(zoom_level - current_zoom) < 0.001 &&
                   scroll_x == current_scroll;
        }
    };
    
    struct TimelineDataCache {
        std::vector<CachedTrackData> cached_tracks;
        uint64_t timeline_version = 0;
        bool is_updating = false;
        std::chrono::steady_clock::time_point last_full_update;
        
        void invalidate() {
            timeline_version = 0;
            cached_tracks.clear();
        }
    };
    
    enum class RenderPass {
        BACKGROUND,
        TIMECODE,
        TRACK_STRUCTURE,
        SEGMENTS_BASIC,
        SEGMENTS_DETAILED,
        WAVEFORMS,
        OVERLAYS
    };
    
    struct ProgressiveRenderer {
        RenderPass current_pass = RenderPass::BACKGROUND;
        bool is_active = false;
        QRect render_region;
        std::chrono::steady_clock::time_point pass_start_time;
        std::vector<RenderPass> remaining_passes;
        
        void start_progressive_render(const QRect& region);
        bool advance_to_next_pass();
        bool is_render_complete() const;
        void reset();
    };
    
    // Phase 3 cache instances
    mutable TimelineDataCache timeline_data_cache_;
    mutable QPixmap background_cache_;
    mutable QPixmap timecode_cache_;
    mutable bool background_cache_valid_;
    mutable bool timecode_cache_valid_;
    mutable double cached_background_zoom_;
    mutable int cached_background_scroll_;
    mutable std::unordered_map<uint32_t, QPixmap> segment_pixmap_cache_;
    mutable ProgressiveRenderer progressive_renderer_;
    
    // Phase 3 functions
    void update_timeline_data_cache() const;
    const CachedTrackData* get_cached_track_data(size_t track_index) const;
    void invalidate_background_cache();
    void invalidate_timecode_cache();
    void invalidate_segment_cache(uint32_t segment_id);
    bool render_next_progressive_pass(QPainter& painter);
    
    // Phase 2: Dirty region tracking for optimized repainting
    struct DirtyRegion {
        QRect rect;
        bool needs_full_redraw = false;
        bool needs_text_update = false;
        bool needs_waveform_update = false;
        std::chrono::steady_clock::time_point created_time;
        
        DirtyRegion(const QRect& r, bool full = false) 
            : rect(r), needs_full_redraw(full), created_time(std::chrono::steady_clock::now()) {}
    };
    mutable std::vector<DirtyRegion> dirty_regions_;
    mutable bool has_dirty_regions_;
    mutable QRect total_dirty_rect_;  // Bounding box of all dirty regions

    // Cached hit-test state to cut down repeated scans
    mutable ve::timeline::SegmentId cached_hit_segment_id_;
    mutable size_t cached_hit_segment_index_;
    mutable size_t cached_hit_track_index_;
    mutable uint64_t cached_hit_timeline_version_;
    
    // ======= Phase 4: Memory & Threading Optimizations =======
    
    // Phase 4.1: Paint Object Memory Pool
    struct PaintObjectPool {
        // Pre-allocated objects to eliminate allocations during paint
        std::vector<QColor> color_pool_;
        std::vector<QPen> pen_pool_;
        std::vector<QBrush> brush_pool_;
        std::vector<QFont> font_pool_;
        std::vector<QRect> rect_pool_;
        
        // Pool indices for current usage
        mutable size_t color_index_ = 0;
        mutable size_t pen_index_ = 0;
        mutable size_t brush_index_ = 0;
        mutable size_t font_index_ = 0;
        mutable size_t rect_index_ = 0;
        
        // Pool statistics
        mutable size_t max_colors_used_ = 0;
        mutable size_t max_pens_used_ = 0;
        mutable size_t max_brushes_used_ = 0;
        mutable int total_allocations_saved_ = 0;
        
        void initialize_pools();
        QColor* get_color(int r, int g, int b, int a = 255);
        QPen* get_pen(const QColor& color, qreal width = 1.0, Qt::PenStyle style = Qt::SolidLine);
        QBrush* get_brush(const QColor& color);
        QFont* get_font(const QString& family, int size, int weight = QFont::Normal);
        QRect* get_rect(int x, int y, int width, int height);
        void reset_pools();
        void print_pool_statistics() const;
    };
    mutable PaintObjectPool paint_object_pool_;
    
    // Phase 4.2: Advanced Performance Analytics
    struct PerformanceAnalytics {
        // Cache hit rate tracking
        mutable int background_cache_hits_ = 0;
        mutable int background_cache_misses_ = 0;
        mutable int timecode_cache_hits_ = 0;
        mutable int timecode_cache_misses_ = 0;
        mutable int segment_cache_hits_ = 0;
        mutable int segment_cache_misses_ = 0;
        mutable int timeline_data_cache_hits_ = 0;
        mutable int timeline_data_cache_misses_ = 0;
        
        // Paint performance tracking
        mutable std::chrono::microseconds total_paint_time_{0};
        mutable std::chrono::microseconds background_paint_time_{0};
        mutable std::chrono::microseconds timecode_paint_time_{0};
        mutable std::chrono::microseconds segments_paint_time_{0};
        mutable int paint_event_count_ = 0;
        
        // Memory optimization tracking
        mutable size_t peak_memory_usage_ = 0;
        mutable size_t current_cache_memory_ = 0;
        mutable int memory_allocations_saved_ = 0;
        
        // Progressive rendering analytics
        mutable int progressive_renders_started_ = 0;
        mutable int progressive_renders_completed_ = 0;
        mutable std::chrono::microseconds avg_pass_time_{0};
        
        void reset_statistics();
        void record_cache_hit(const std::string& cache_type);
        void record_cache_miss(const std::string& cache_type);
        void record_paint_time(const std::string& phase, std::chrono::microseconds time);
        void record_memory_saved(size_t bytes);
        void print_analytics() const;
        double get_overall_cache_hit_rate() const;
    };
    mutable PerformanceAnalytics performance_analytics_;
    
    // Phase 4.3: Memory Container Optimizations
    struct MemoryOptimizations {
        // Pre-allocated containers for timeline processing
        mutable std::vector<const ve::timeline::Segment*> visible_segments_buffer_;
        mutable std::vector<QRect> segment_rects_buffer_;
        mutable std::vector<QString> segment_names_buffer_;
        mutable std::vector<QColor> segment_colors_buffer_;
        
        // Segment batching containers
        struct SegmentBatch {
            QColor color;
            std::vector<QRect> rects;
            std::vector<QString> names;
        };
        mutable std::vector<SegmentBatch> segment_batches_;
        
        // String pool for segment names to avoid QString allocations
        mutable std::unordered_map<std::string, QString> string_pool_;
        mutable size_t string_pool_hits_ = 0;
        mutable size_t string_pool_misses_ = 0;
        
        void reserve_containers(size_t segment_count);
        void clear_containers();
        const QString& get_cached_string(const std::string& str);
        void batch_segments_by_color(const std::vector<const ve::timeline::Segment*>& segments);
        void print_memory_stats() const;
    };
    mutable MemoryOptimizations memory_optimizations_;
    
    // Phase 4.4: Advanced Paint State Management
    struct AdvancedPaintStateCache {
        // Current QPainter state tracking with deep comparison
        struct PaintState {
            QColor pen_color;
            QColor brush_color;
            qreal pen_width;
            Qt::PenStyle pen_style;
            QFont font;
            QTransform transform;
            QPainter::CompositionMode composition_mode;
            bool antialiasing_enabled;
            
            bool operator==(const PaintState& other) const {
                return pen_color == other.pen_color &&
                       brush_color == other.brush_color &&
                       std::abs(pen_width - other.pen_width) < 0.001 &&
                       pen_style == other.pen_style &&
                       font == other.font &&
                       transform == other.transform &&
                       composition_mode == other.composition_mode &&
                       antialiasing_enabled == other.antialiasing_enabled;
            }
        };
        
        mutable PaintState current_state_;
        mutable PaintState cached_state_;
        mutable bool state_is_cached_ = false;
        
        // State change optimization counters
        mutable int pen_changes_avoided_ = 0;
        mutable int brush_changes_avoided_ = 0;
        mutable int font_changes_avoided_ = 0;
        mutable int transform_changes_avoided_ = 0;
        mutable int total_state_changes_avoided_ = 0;
        
        void cache_current_state(QPainter& painter);
        bool apply_pen_optimized(QPainter& painter, const QColor& color, qreal width = 1.0, Qt::PenStyle style = Qt::SolidLine);
        bool apply_brush_optimized(QPainter& painter, const QColor& color);
        bool apply_font_optimized(QPainter& painter, const QFont& font);
        bool apply_transform_optimized(QPainter& painter, const QTransform& transform);
        void reset_state_cache();
        void print_state_optimization_stats() const;
    };
    mutable AdvancedPaintStateCache advanced_paint_state_;
    
    // Phase 4 Functions
    void initialize_phase4_optimizations();
    void update_memory_containers_for_paint() const;
    void batch_similar_segments(const std::vector<const ve::timeline::Segment*>& segments) const;
    void draw_segment_batch_optimized(QPainter& painter, const MemoryOptimizations::SegmentBatch& batch, int track_y) const;
    void record_performance_metrics(const std::string& operation, std::chrono::microseconds duration) const;
    void cleanup_phase4_resources() const;
};

} // namespace ve::ui
