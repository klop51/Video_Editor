#pragma once
#include "../../core/include/core/time.hpp"
#include "../../media_io/include/media_io/media_probe.hpp"
#include "../../timeline/include/timeline/timeline.hpp"
#include <QMainWindow>
#include <QDockWidget>
#include <QTimer>
#include <QQueue>
#include <memory>

QT_BEGIN_NAMESPACE
class QMenuBar;
class QToolBar;
class QStatusBar;
class QAction;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QThread;
class QSlider;
class QToolButton;
class QProgressBar;
QT_END_NAMESPACE

namespace ve::timeline { class Timeline; }
namespace ve::playback { class PlaybackController; }
namespace ve::media { struct ProbeResult; }
namespace ve::commands { 
    class Command; 
    class CommandHistory; 
}

namespace ve::ui {

class TimelinePanel;
class ViewerPanel;
class MediaProcessingWorker;
class TimelineProcessingWorker;

// Forward declarations for worker result structs
struct MediaInfo {
    QString filePath;
    ve::media::ProbeResult probeResult;
    bool success;
    QString errorMessage;
};

struct TimelineInfo {
    QString filePath;
    ve::media::ProbeResult probeResult;
    bool has_video;
    bool has_audio;
    double duration_seconds;
    bool success;
    QString errorMessage;
    // Context from the UI (drop location etc.)
    ve::TimePoint start_time{}; // Where to place the clip on the timeline
    int track_index = 0;        // Target track index (will auto-expand if needed)
    
    // NEW: Prepared data to minimize UI thread work
    std::shared_ptr<ve::timeline::MediaSource> prepared_source;
    ve::timeline::Segment prepared_segment;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
    void set_timeline(ve::timeline::Timeline* timeline);
    void set_playback_controller(ve::playback::PlaybackController* controller);
    
protected:
    void closeEvent(QCloseEvent* event) override;
    
private slots:
    // File menu
    void new_project();
    void open_project();
    void save_project();
    void save_project_as();
    void import_media();
    void export_timeline();
    void quit_application();
    
    // Edit menu
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void delete_selection();
    
    // Playback menu
    void play_pause();
    void stop();
    void step_forward();
    void step_backward();
    void go_to_start();
    void go_to_end();
    
    // Audio controls
    void toggle_audio_mute();
    void set_master_volume(int volume_percent);
    void update_audio_levels();
    
    // Position update
    void update_playback_position();
    
    // View menu
    void zoom_in();
    void zoom_out();
    void zoom_fit();
    void toggle_timeline();
    void toggle_media_browser();
    void toggle_properties();
    void toggle_fps_overlay();
    void toggle_preview_quality();
    
    // Help menu
    void about();
    void about_qt();
    
    // Playback status updates
    void on_playback_time_changed(ve::TimePoint time);
    void on_playback_state_changed();
    
    // Media browser interactions
    void on_media_item_double_clicked(QTreeWidgetItem* item, int column);
    void on_media_browser_context_menu(const QPoint& pos);
    void add_media_to_timeline(const QString& filePath);
    void add_selected_media_to_timeline();
    // Overload allowing explicit placement context
    void add_media_to_timeline(const QString& filePath, ve::TimePoint start_time, int track_index);
    
    // Timeline interactions
    void on_timeline_clip_added(const QString& filePath, ve::TimePoint start_time, int track_index);
    
    // Worker thread handlers
    void on_media_processed(const MediaInfo& info);
    void on_media_processing_error(const QString& error);
    void on_media_progress(int percentage, const QString& status);
    void on_timeline_processed(const TimelineInfo& info);
    void on_timeline_processing_error(const QString& error);
    void on_timeline_progress(int percentage, const QString& status);

    // Application-level project state notifications
    void on_project_state_changed();      // project loaded/closed/saved
    void on_project_dirty();              // project gained unsaved changes
    
private:
    void create_menus();
    void create_toolbars();
    void create_status_bar();
    void create_dock_widgets();
    void setup_layout();
    void connect_signals();
    void update_window_title();
    void update_actions();
    
    // Media import helpers
    void import_single_file(const QString& filePath);
    void add_media_to_browser(const QString& filePath, const ve::media::ProbeResult& probe);
    void add_media_browser_placeholder();
    void remove_media_browser_placeholder();
    
    // Command system helpers
    bool execute_command(std::unique_ptr<ve::commands::Command> command);
    
    // Development/testing helpers
    void create_test_timeline_content();
    
    // Worker thread management
    void setup_media_worker();
    void cleanup_media_worker();
    void setup_timeline_worker();
    void cleanup_timeline_worker();
    
    // Central widget and panels
    ViewerPanel* viewer_panel_;
    
    // Dock widgets and their panels
    QDockWidget* timeline_dock_;
    TimelinePanel* timeline_panel_;
    
    QDockWidget* media_browser_dock_;
    QTreeWidget* media_browser_;
    
    QDockWidget* properties_dock_;
    QLabel* property_panel_;
    
    // Menu actions
    QAction* new_action_;
    QAction* open_action_;
    QAction* save_action_;
    QAction* save_as_action_;
    QAction* import_action_;
    QAction* export_action_;
    QAction* quit_action_;
    
    QAction* undo_action_;
    QAction* redo_action_;
    QAction* cut_action_;
    QAction* copy_action_;
    QAction* paste_action_;
    QAction* delete_action_;
    QAction* add_to_timeline_action_{}; // Edit menu timeline insertion action
    
    QAction* play_pause_action_;
    QAction* stop_action_;
    QAction* step_forward_action_;
    QAction* step_backward_action_;
    QAction* go_to_start_action_;
    QAction* go_to_end_action_;
    QAction* toggle_fps_overlay_action_{};
    QAction* toggle_preview_fit_action_{};
    
    // Audio controls
    QAction* mute_audio_action_;
    QSlider* volume_slider_;
    QToolButton* mute_button_;
    QProgressBar* audio_level_meter_;
    QLabel* volume_label_;
    
    // Status bar
    QLabel* status_label_;
    QLabel* time_label_;
    QLabel* fps_label_;
    QLabel* audio_status_label_;
    
    // Audio level update timer
    QTimer* audio_level_timer_;
    
    // Data
    ve::timeline::Timeline* timeline_;
    ve::playback::PlaybackController* playback_controller_;
    std::unique_ptr<ve::commands::CommandHistory> command_history_;
    QTimer* position_update_timer_;
    
    QString current_project_path_;
    bool project_modified_;
    // NOTE: project_modified_ is intentionally updated directly in certain UI actions
    // (undo/redo/command execution) AND indirectly via Application's project_modified
    // signal (triggered by Timeline::mark_modified callback). This duplication keeps
    // the UI instantly responsive while still exercising the central dirty pipeline
    // for any other observers. See undo/redo/execute_command for paired
    // timeline_->mark_modified() calls ensuring Application is notified.
    
    // Worker threads for background processing
    MediaProcessingWorker* media_worker_;
    QThread* media_worker_thread_;
    TimelineProcessingWorker* timeline_worker_;
    QThread* timeline_worker_thread_;
    
    // UI responsiveness improvements - chunked processing
    QQueue<TimelineInfo> timeline_update_queue_;
    QTimer* timeline_update_pump_;
    void flushTimelineBatch();
};

} // namespace ve::ui
