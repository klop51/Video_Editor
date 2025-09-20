/**
 * @file minimal_audio_track_widget.hpp
 * @brief Enhanced Audio Track Widget for Professional Qt Timeline Integration
 * 
 * Professional track widget featuring multi-track support, clip boundaries,
 * fade visualization, context menu integration, and advanced timeline functionality.
 */

#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMenu>
#include <QtCore/QTimer>
#include <memory>
#include <vector>

// Forward declarations
namespace ve::ui {
    class MinimalWaveformWidget;
}

namespace ve::audio {
    class WaveformGenerator;
}

namespace ve::ui {

/**
 * @brief Professional audio track widget for timeline display
 * 
 * Enhanced implementation featuring:
 * - Multi-track support with visual separation
 * - Audio clip boundaries and fade visualization
 * - Context menu integration for audio operations
 * - Professional track selection and highlighting
 * - Timeline zoom and navigation integration
 */
class MinimalAudioTrackWidget : public QWidget {
    Q_OBJECT

public:
    explicit MinimalAudioTrackWidget(QWidget* parent = nullptr);
    ~MinimalAudioTrackWidget() override;

    // Track configuration
    void set_track_name(const QString& name);
    void set_track_color(const QColor& color);
    void set_waveform_generator(std::shared_ptr<ve::audio::WaveformGenerator> generator);
    void set_audio_duration(double duration_seconds);
    void set_track_height(int height);
    
    // Audio clip management
    struct AudioClip {
        QString name;
        double start_time = 0.0;
        double duration = 0.0;
        double fade_in = 0.0;
        double fade_out = 0.0;
        QColor color = QColor(100, 150, 255);
        bool is_selected = false;
        bool is_muted = false;
    };
    
    void add_audio_clip(const AudioClip& clip);
    void remove_audio_clip(int clip_index);
    void clear_audio_clips();
    void select_clip(int clip_index);
    void set_clip_fade(int clip_index, double fade_in, double fade_out);
    
    // Timeline integration
    void set_timeline_position(double position_seconds);
    void set_visible_time_range(double start_seconds, double duration_seconds);
    void set_timeline_zoom(double zoom_factor);
    
    // Track state
    void set_track_muted(bool muted);
    void set_track_solo(bool solo);
    void set_track_selected(bool selected);
    bool is_track_selected() const;

signals:
    void track_selected();
    void track_muted_changed(bool muted);
    void track_solo_changed(bool solo);
    void playhead_position_changed(double position_seconds);
    void clip_selected(int clip_index);
    void clip_context_menu_requested(int clip_index, const QPoint& position);
    void track_context_menu_requested(const QPoint& position);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // Components
    MinimalWaveformWidget* waveform_widget_;
    QHBoxLayout* layout_;
    QMenu* context_menu_;
    QTimer* update_timer_;
    
    // Track state
    QString track_name_;
    QColor track_color_ = QColor(100, 150, 255);
    double audio_duration_seconds_ = 0.0;
    double timeline_position_seconds_ = 0.0;
    double visible_start_seconds_ = 0.0;
    double visible_duration_seconds_ = 10.0;
    double zoom_factor_ = 1.0;
    
    bool is_selected_ = false;
    bool is_muted_ = false;
    bool is_solo_ = false;
    int track_height_ = 90;
    
    // Audio clips
    std::vector<AudioClip> audio_clips_;
    int selected_clip_index_ = -1;
    int hovered_clip_index_ = -1;
    
    // Mouse interaction state
    enum class InteractionMode {
        None,
        DraggingPlayhead,
        SelectingClip,
        DraggingClip,
        ResizingClip
    };
    InteractionMode interaction_mode_ = InteractionMode::None;
    QPoint last_mouse_pos_;
    double mouse_drag_start_time_ = 0.0;
    int drag_clip_index_ = -1;
    
    // Visual configuration
    QColor background_color_ = QColor(35, 35, 35);
    QColor selected_color_ = QColor(100, 150, 255);
    QColor muted_color_ = QColor(100, 100, 100);
    QColor playhead_color_ = QColor(255, 100, 100);
    
    // Helper methods
    void setup_ui();
    void setup_context_menu();
    void update_waveform_display();
    
    // Audio clip methods
    int find_clip_at_position(double time_seconds) const;
    QRect get_clip_rect(const AudioClip& clip) const;
    void draw_audio_clip(QPainter& painter, const AudioClip& clip, int clip_index);
    void draw_clip_fade(QPainter& painter, const QRect& clip_rect, double fade_in, double fade_out);
    
    // Coordinate conversion
    double pixel_to_time(int pixel_x) const;
    int time_to_pixel(double time_seconds) const;
    QRect get_waveform_rect() const;
    QRect get_track_header_rect() const;
    
    // Context menu actions
    void show_track_context_menu(const QPoint& position);
    void show_clip_context_menu(int clip_index, const QPoint& position);

private slots:
    void on_mute_action_triggered();
    void on_solo_action_triggered();
    void on_delete_clip_action_triggered();
    void on_split_clip_action_triggered();
    void update_display();
};

} // namespace ve::ui