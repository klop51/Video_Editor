#pragma once
#include "timeline/timeline.hpp"
#include "core/time.hpp"
#include <QWidget>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QMenu> // Ensure QMenu complete type available for context menu implementation
#include <memory>
#include <functional>

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
    
    // Editing operations
    void cut_selected_segments();
    void copy_selected_segments();
    void paste_segments();
    void delete_selected_segments();
    void split_segment_at_playhead();
    
signals:
    void time_changed(ve::TimePoint time);
    void selection_changed();
    void track_height_changed();
    void clip_added(const QString& filePath, ve::TimePoint start_time, int track_index);
    
    // Editing signals
    void segments_cut();
    void segments_deleted();
    void segment_split(ve::timeline::SegmentId segment_id, ve::TimePoint split_time);
    
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
    
private:
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
    void draw_video_thumbnail(QPainter& painter, const QRect& rect, const ve::timeline::Segment& segment);
    void draw_segment_handles(QPainter& painter, const QRect& rect);
    
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
};

} // namespace ve::ui
