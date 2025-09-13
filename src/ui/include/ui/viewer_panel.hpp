#pragma once
#include "../../decode/include/decode/frame.hpp"
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QPixmap>
#include <deque>
#include <memory>
// Forward declarations to reduce header coupling; implementation includes full definitions.
namespace ve { namespace gfx { class GraphicsDevice; struct GraphicsDeviceInfo; } }
namespace ve { namespace render { class RenderGraph; } }

QT_BEGIN_NAMESPACE
class QDragEnterEvent;
class QDropEvent;
QT_END_NAMESPACE

namespace ve::playback { class PlaybackController; }

namespace ve::ui {

class ViewerPanel : public QWidget {
    Q_OBJECT
    
public:
    explicit ViewerPanel(QWidget* parent = nullptr);
    ~ViewerPanel(); // defined in .cpp after including full RenderGraph type
    
    void set_playback_controller(ve::playback::PlaybackController* controller);
    void set_fps_overlay_visible(bool on);
    void set_preview_scale_to_widget(bool on) { preview_scale_to_widget_ = on; }
    // Experimental GPU pipeline control (no-op if initialization fails)
    bool enable_gpu_pipeline(); // returns true if GPU path initialized and enabled
    void disable_gpu_pipeline();
    bool gpu_pipeline_enabled() const { return gpu_enabled_; }
    
    // Media loading
    bool load_media(const QString& filePath);
    
public slots:
    void display_frame(const ve::decode::VideoFrame& frame);
    void update_time_display(int64_t time_us);
    void update_playback_controls();
    
signals:
    void play_pause_requested();
    void stop_requested();
    void seek_requested(int64_t time_us);
    
protected:
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    
private slots:
    void on_play_pause_clicked();
    void on_stop_clicked();
    void on_position_slider_changed(int value);
    void on_step_forward_clicked();
    void on_step_backward_clicked();
    
private:
    void setup_ui();
    void update_frame_display();
    QPixmap convert_frame_to_pixmap(const ve::decode::VideoFrame& frame);
    
    // UI components
    class GLVideoWidget* gl_widget_ = nullptr; // created on-demand
    QWidget* video_display_ = nullptr; // active display widget
    QLabel* video_display_label_ = nullptr; // QLabel used for CPU path
    QLabel* time_label_;
    QLabel* fps_overlay_;
    QPushButton* play_pause_button_;
    QPushButton* stop_button_;
    QPushButton* step_backward_button_;
    QPushButton* step_forward_button_;
    QSlider* position_slider_;
    
    // Data
    ve::playback::PlaybackController* playback_controller_;
    ve::decode::VideoFrame current_frame_;
    bool has_frame_;
    
    // Display scaling
    QSize display_size_;
    Qt::AspectRatioMode aspect_ratio_mode_;

    // Overlay FPS updater (throttled)
    qint64 fps_last_ms_ = 0;
    int fps_frames_accum_ = 0;

    // Simple rendering coalescing to keep UI responsive
    bool render_in_progress_ = false;
    bool pending_frame_valid_ = false;
    ve::decode::VideoFrame pending_frame_{};

    // Preview scaling and small RGBA pixmap cache
    bool preview_scale_to_widget_ = true;
    struct PixCacheEntry { int64_t pts; int w; int h; QPixmap pix; };
    std::deque<PixCacheEntry> pix_cache_;
    int pix_cache_capacity_ = 8;

    // GPU pipeline state (experimental)
    bool gpu_enabled_ = false;
    bool gpu_initialized_ = false;
    std::shared_ptr<ve::gfx::GraphicsDevice> gfx_device_;
    std::unique_ptr<ve::render::RenderGraph> render_graph_;
};

} // namespace ve::ui
