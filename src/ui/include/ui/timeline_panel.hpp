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

// Forward declarations
namespace ve::commands { class Command; }

namespace ve::ui {

class TimelinePanel : public QWidget {
    Q_OBJECT
    
public:
    explicit TimelinePanel(QWidget* parent = nullptr);
    
    void set_timeline(ve::timeline::Timeline* timeline);
    void set_zoom(double zoom_factor);
    void set_current_time(ve::TimePoint time);
    
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
    void draw_video_thumbnail(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment);
    void draw_segment_handles(QPainter& painter, const QRect& rect);
    
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
    
    // Colors
    QColor track_color_video_;
    QColor track_color_audio_;
    QColor segment_color_;
    QColor segment_selected_color_;
    QColor playhead_color_;
    QColor background_color_;
    QColor grid_color_;
    
    // Debug tools
    QTimer* heartbeat_timer_;
    
    // Waveform caching and generation
    std::unique_ptr<ve::audio::WaveformCache> waveform_cache_;
    std::unique_ptr<ve::audio::WaveformGenerator> waveform_generator_;
    
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
};

} // namespace ve::ui
