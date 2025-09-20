/**
 * @file audio_track_widget.hpp
 * @brief Professional Audio Track Widget for Timeline Integration
 * 
 * Week 8 Qt Timeline UI Integration - Specialized widget for displaying
 * audio tracks in the timeline with integrated waveform visualization,
 * professional editing controls, and seamless Week 7 waveform system integration.
 */

#pragma once

#include "waveform_widget.hpp"
#include "../../timeline/include/timeline/timeline.hpp"
#include "../../core/include/core/time.hpp"
#include <QtWidgets/QWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSlider>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QMenu>
#include <QtGui/QPainter>
#include <QtCore/QTimer>
#include <memory>
#include <vector>

// Forward declarations
namespace ve::audio {
    class WaveformGenerator;
    class WaveformCache;
}

namespace ve::commands {
    class Command;
}

namespace ve::ui {

/**
 * @brief Audio track control panel configuration
 */
struct AudioTrackControls {
    bool show_mute_button{true};
    bool show_solo_button{true};
    bool show_volume_slider{true};
    bool show_pan_control{true};
    bool show_record_arm{false};
    bool show_track_meters{true};
    
    int control_width{120};                      // Width of control panel
    int button_size{24};                         // Size of control buttons
    int slider_height{100};                      // Height of volume slider
};

/**
 * @brief Audio clip visual representation
 */
struct AudioClipVisual {
    QRect bounds;                                // Clip boundaries in widget coordinates
    ve::timeline::SegmentId segment_id;          // Associated timeline segment
    ve::TimePoint start_time;                    // Clip start time
    ve::TimePoint duration;                      // Clip duration
    std::string audio_file_path;                 // Source audio file
    
    // Visual state
    bool is_selected{false};                     // Currently selected
    bool is_muted{false};                        // Muted state
    float volume{1.0f};                          // Volume level (0.0 - 2.0)
    float pan{0.0f};                             // Pan position (-1.0 to 1.0)
    
    // Waveform integration
    std::shared_ptr<QWaveformWidget> waveform_widget; // Embedded waveform display
    bool waveform_visible{true};                 // Show/hide waveform
    WaveformStyle waveform_style;                // Waveform appearance
};

/**
 * @brief Audio track header with professional controls
 */
class AudioTrackHeader : public QWidget {
    Q_OBJECT

public:
    explicit AudioTrackHeader(QWidget* parent = nullptr);
    ~AudioTrackHeader() override = default;
    
    // Track configuration
    void set_track_name(const QString& name);
    void set_track_color(const QColor& color);
    void set_controls_config(const AudioTrackControls& config);
    
    // Audio parameters
    void set_volume(float volume);           // 0.0 - 2.0 (200% max)
    void set_pan(float pan);                 // -1.0 (left) to 1.0 (right)
    void set_muted(bool muted);
    void set_solo(bool solo);
    void set_record_armed(bool armed);
    
    // Visual state
    void set_selected(bool selected);
    void set_track_height(int height);
    
    // Getters
    float volume() const { return volume_; }
    float pan() const { return pan_; }
    bool is_muted() const { return muted_; }
    bool is_solo() const { return solo_; }
    bool is_record_armed() const { return record_armed_; }

signals:
    void volume_changed(float volume);
    void pan_changed(float pan);
    void mute_toggled(bool muted);
    void solo_toggled(bool solo);
    void record_arm_toggled(bool armed);
    void track_name_changed(const QString& name);
    void header_context_menu(const QPoint& pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void on_volume_slider_changed(int value);
    void on_pan_slider_changed(int value);
    void on_mute_clicked();
    void on_solo_clicked();
    void on_record_clicked();

private:
    void setup_ui();
    void update_control_layout();
    void draw_track_meters(QPainter& painter, const QRect& rect);
    
    // Configuration
    AudioTrackControls controls_config_;
    
    // Track properties
    QString track_name_{"Audio Track"};
    QColor track_color_{100, 150, 255};
    float volume_{1.0f};
    float pan_{0.0f};
    bool muted_{false};
    bool solo_{false};
    bool record_armed_{false};
    bool selected_{false};
    
    // UI components
    QVBoxLayout* main_layout_;
    QLabel* name_label_;
    QSlider* volume_slider_;
    QSlider* pan_slider_;
    QPushButton* mute_button_;
    QPushButton* solo_button_;
    QPushButton* record_button_;
    
    // Visual state
    int track_height_{80};
    QRect meter_rect_;                           // Audio level meters area
    std::vector<float> meter_levels_;            // Current audio levels for meters
};

/**
 * @brief Main audio track widget combining header and timeline content
 */
class AudioTrackWidget : public QWidget {
    Q_OBJECT

public:
    explicit AudioTrackWidget(QWidget* parent = nullptr);
    ~AudioTrackWidget() override;
    
    // Disable copy semantics
    AudioTrackWidget(const AudioTrackWidget&) = delete;
    AudioTrackWidget& operator=(const AudioTrackWidget&) = delete;
    
    /**
     * @brief Track configuration
     */
    void set_track(const ve::timeline::Track* track);
    void set_track_index(size_t index);
    void set_track_name(const QString& name);
    void set_track_height(int height);
    
    /**
     * @brief Timeline integration
     */
    void set_timeline_zoom(double zoom_factor);
    void set_timeline_scroll(int scroll_x);
    void set_current_time(const ve::TimePoint& time);
    void set_selection_range(const ve::TimePoint& start, const ve::TimePoint& end);
    
    /**
     * @brief Week 7 waveform system integration
     */
    void set_waveform_generator(std::shared_ptr<ve::audio::WaveformGenerator> generator);
    void set_waveform_cache(std::shared_ptr<ve::audio::WaveformCache> cache);
    
    /**
     * @brief Audio clip management
     */
    void add_audio_clip(const ve::timeline::Segment& segment);
    void remove_audio_clip(ve::timeline::SegmentId segment_id);
    void update_audio_clip(const ve::timeline::Segment& segment);
    void clear_audio_clips();
    
    /**
     * @brief Selection and editing
     */
    void set_selected_clips(const std::vector<ve::timeline::SegmentId>& selected);
    std::vector<ve::timeline::SegmentId> get_selected_clips() const;
    void select_all_clips();
    void deselect_all_clips();
    
    /**
     * @brief Audio processing controls
     */
    void set_track_volume(float volume);
    void set_track_pan(float pan);
    void set_track_muted(bool muted);
    void set_track_solo(bool solo);
    
    /**
     * @brief Visual customization
     */
    void set_waveform_style(const WaveformStyle& style);
    void set_track_color(const QColor& color);
    void set_controls_visible(bool visible);
    
    /**
     * @brief Context menu integration
     */
    void set_context_menu_actions(const std::vector<QAction*>& actions);
    
    // Getters
    size_t track_index() const { return track_index_; }
    const ve::timeline::Track* track() const { return track_; }
    int track_height() const { return track_height_; }
    
signals:
    /**
     * @brief Track interaction signals
     */
    void clip_selected(ve::timeline::SegmentId segment_id, bool multi_select);
    void clip_deselected(ve::timeline::SegmentId segment_id);
    void clip_moved(ve::timeline::SegmentId segment_id, ve::TimePoint new_start);
    void clip_resized(ve::timeline::SegmentId segment_id, ve::TimePoint new_start, ve::TimePoint new_duration);
    void clip_split(ve::timeline::SegmentId segment_id, ve::TimePoint split_time);
    void clip_deleted(ve::timeline::SegmentId segment_id);
    
    /**
     * @brief Audio parameter signals
     */
    void track_volume_changed(size_t track_index, float volume);
    void track_pan_changed(size_t track_index, float pan);
    void track_mute_changed(size_t track_index, bool muted);
    void track_solo_changed(size_t track_index, bool solo);
    
    /**
     * @brief Timeline interaction signals
     */
    void playhead_moved(ve::TimePoint position);
    void timeline_zoom_requested(float factor, ve::TimePoint center);
    void timeline_scroll_requested(int delta_x);
    
    /**
     * @brief Context menu signals
     */
    void context_menu_requested(const QPoint& global_pos, ve::timeline::SegmentId segment_id);
    void track_context_menu_requested(const QPoint& global_pos);

protected:
    /**
     * @brief Qt event handling
     */
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    /**
     * @brief Internal update slots
     */
    void update_waveform_displays();
    void handle_track_parameter_change();
    void refresh_clip_visuals();
    
    /**
     * @brief Header control integration
     */
    void on_header_volume_changed(float volume);
    void on_header_pan_changed(float pan);
    void on_header_mute_toggled(bool muted);
    void on_header_solo_toggled(bool solo);

private:
    /**
     * @brief Rendering system
     */
    void render_track_background(QPainter& painter, const QRect& rect);
    void render_audio_clips(QPainter& painter, const QRect& rect);
    void render_clip(QPainter& painter, const AudioClipVisual& clip);
    void render_clip_waveform(QPainter& painter, const AudioClipVisual& clip);
    void render_clip_borders(QPainter& painter, const AudioClipVisual& clip);
    void render_clip_selection(QPainter& painter, const AudioClipVisual& clip);
    void render_clip_handles(QPainter& painter, const AudioClipVisual& clip);
    void render_playhead(QPainter& painter, const QRect& rect);
    void render_selection_overlay(QPainter& painter, const QRect& rect);
    void render_drop_indicator(QPainter& painter, const QRect& rect);
    
    /**
     * @brief Coordinate conversion
     */
    ve::TimePoint pixel_to_time(int pixel_x) const;
    int time_to_pixel(const ve::TimePoint& time) const;
    QRect time_range_to_rect(const ve::TimePoint& start, const ve::TimePoint& duration) const;
    QRect clip_to_widget_rect(const AudioClipVisual& clip) const;
    
    /**
     * @brief Clip management
     */
    AudioClipVisual* find_clip_at_position(const QPoint& pos);
    AudioClipVisual* find_clip_by_id(ve::timeline::SegmentId segment_id);
    void update_clip_bounds();
    void create_clip_waveform_widget(AudioClipVisual& clip);
    void destroy_clip_waveform_widget(AudioClipVisual& clip);
    
    /**
     * @brief Interaction handling
     */
    enum class InteractionMode {
        NONE,
        SELECTING,
        DRAGGING_CLIP,
        RESIZING_CLIP_LEFT,
        RESIZING_CLIP_RIGHT,
        TIMELINE_SCRUBBING,
        RANGE_SELECTION
    };
    
    void handle_clip_click(const QPoint& pos, Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void handle_timeline_click(const QPoint& pos, Qt::MouseButton button);
    void start_clip_drag(AudioClipVisual* clip, const QPoint& start_pos);
    void start_clip_resize(AudioClipVisual* clip, bool left_edge, const QPoint& start_pos);
    void update_interaction(const QPoint& current_pos);
    void finish_interaction();
    void cancel_interaction();
    
    /**
     * @brief Selection management
     */
    void select_clip(ve::timeline::SegmentId segment_id, bool multi_select);
    void deselect_clip(ve::timeline::SegmentId segment_id);
    void update_selection_visual();
    bool is_clip_selected(ve::timeline::SegmentId segment_id) const;
    
    /**
     * @brief Data members
     */
    
    // Track data
    const ve::timeline::Track* track_{nullptr};
    size_t track_index_{0};
    int track_height_{80};
    QString track_name_;
    QColor track_color_{100, 150, 255};
    
    // Timeline state
    double zoom_factor_{1.0};
    int scroll_x_{0};
    ve::TimePoint current_time_{0, 1};
    ve::TimePoint selection_start_{0, 1};
    ve::TimePoint selection_end_{0, 1};
    
    // Week 7 waveform integration
    std::shared_ptr<ve::audio::WaveformGenerator> waveform_generator_;
    std::shared_ptr<ve::audio::WaveformCache> waveform_cache_;
    WaveformStyle default_waveform_style_;
    
    // Audio clips
    std::vector<AudioClipVisual> audio_clips_;
    std::vector<ve::timeline::SegmentId> selected_clips_;
    
    // UI components
    std::unique_ptr<AudioTrackHeader> header_widget_;
    QHBoxLayout* main_layout_;
    
    // Interaction state
    InteractionMode interaction_mode_{InteractionMode::NONE};
    QPoint interaction_start_pos_;
    QPoint interaction_current_pos_;
    AudioClipVisual* interaction_target_clip_{nullptr};
    bool interaction_is_left_edge_{false};
    
    // Drag and drop state
    ve::TimePoint drop_position_{0, 1};
    bool show_drop_indicator_{false};
    
    // Visual configuration
    AudioTrackControls controls_config_;
    bool controls_visible_{true};
    std::vector<QAction*> context_menu_actions_;
    
    // Performance optimization
    std::unique_ptr<QTimer> update_timer_;
    bool needs_visual_update_{false};
    mutable QRect cached_content_rect_;
    
    // Constants
    static constexpr int MIN_CLIP_WIDTH = 10;
    static constexpr int CLIP_HANDLE_WIDTH = 8;
    static constexpr int CLIP_BORDER_WIDTH = 2;
    static constexpr int SELECTION_BORDER_WIDTH = 3;
};

/**
 * @brief Utility functions for audio track widget operations
 */
namespace audio_track_utils {
    /**
     * @brief Clip visual helpers
     */
    QColor calculate_clip_color(const ve::timeline::Segment& segment, bool selected, bool muted);
    QRect calculate_waveform_rect(const QRect& clip_rect, const AudioTrackControls& controls);
    
    /**
     * @brief Time conversion utilities
     */
    ve::TimePoint snap_to_grid(const ve::TimePoint& time, const ve::TimePoint& grid_size);
    ve::TimePoint calculate_grid_size(double zoom_factor);
    
    /**
     * @brief Professional audio editing helpers
     */
    QString format_audio_time(const ve::TimePoint& time);
    QString format_audio_level(float level_db);
    QColor level_to_meter_color(float level_db);
    
    /**
     * @brief Context menu builders
     */
    QMenu* create_clip_context_menu(const ve::timeline::Segment& segment, QWidget* parent = nullptr);
    QMenu* create_track_context_menu(size_t track_index, QWidget* parent = nullptr);
    
    /**
     * @brief Keyboard shortcut handlers
     */
    std::map<QKeySequence, std::function<void()>> get_default_shortcuts();
}

} // namespace ve::ui