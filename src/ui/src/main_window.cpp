#include "ui/main_window.hpp"
#include "ui/timeline_panel.hpp"
#include "ui/viewer_panel.hpp"
#include "timeline/timeline.hpp"
#include "playback/controller.hpp"
#include "commands/command.hpp"
#include "commands/timeline_commands.hpp"
#include "core/log.hpp"
#include "media_io/media_probe.hpp"
#include "decode/decoder.hpp"
#include "app/application.hpp"
#include <limits>

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <algorithm>
#include <QAction>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>

// Convert TimeDuration -> microseconds (safe)
static qint64 td_to_us(const ve::TimeDuration& d) {
    const auto r = d.to_rational();                 // num/den (seconds = num/den)
    long double us = 1'000'000.0L * (long double)r.num / (long double)r.den;
    if (us < 0) us = 0;
    if (us > (long double)std::numeric_limits<qint64>::max()) us = (long double)std::numeric_limits<qint64>::max();
    return (qint64)us;
}

// Make a TimePoint from microseconds
static inline ve::TimePoint us_to_tp(qint64 us) { return ve::TimePoint{ us, 1'000'000 }; }

// Compute frame duration in microseconds from fps (num/den = frames per second)
static qint64 frame_us_from_fps(const ve::TimeRational& fps) {
    if (fps.num <= 0) return 33'333;                // ~30 fps fallback
    long double us = 1'000'000.0L * (long double)fps.den / (long double)fps.num; // 1/fps
    if (us < 1) us = 1;
    return (qint64)us;
}

// Get timeline duration and current pos in microseconds (works even if controller has only rational APIs)
static qint64 timeline_duration_us(const ve::timeline::Timeline* tl) {
    return tl ? td_to_us(tl->duration()) : 0;
}
#include <QProgressDialog>
#include <QDateTime>
#include <QTimer>
#include <QCloseEvent>
#include <QSettings>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMimeData>
#include <QUrl>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QThread>
#include <QFuture>
#include <QElapsedTimer>

// Quick micro-profiler for finding GUI thread hogs
struct ScopeTimer {
    const char* name; 
    QElapsedTimer t;
    ScopeTimer(const char* n) : name(n) { t.start(); }
    ~ScopeTimer() { 
        auto elapsed = t.elapsed();
        if (elapsed > 16) { // Only log if > 16ms (potential frame drop)
            qDebug() << name << "took" << elapsed << "ms (potential UI freeze)";
        }
    }
};
#define SCOPE(name) ScopeTimer _st_##__LINE__(name)

namespace ve::ui {

// Worker class for background media processing
class MediaProcessingWorker : public QObject {
    Q_OBJECT
public:
    MediaProcessingWorker() = default;

public slots:
    void processMedia(const QString& filePath);

signals:
    void mediaProcessed(const ve::ui::MediaInfo& info);
    void progressUpdate(int percentage, const QString& status); // Throttled progress
    void errorOccurred(const QString& error);

private:
    QElapsedTimer progress_timer_;
};

// Worker class for background timeline processing
class TimelineProcessingWorker : public QObject {
    Q_OBJECT
public:
    TimelineProcessingWorker() = default;

public slots:
    void processForTimeline(const QString& filePath);

signals:
    void timelineProcessed(const ve::ui::TimelineInfo& info);
    void progressUpdate(int percentage, const QString& status); // Throttled progress
    void errorOccurred(const QString& error);

private:
    QElapsedTimer progress_timer_; // For throttling progress updates
};

// Implementation
void MediaProcessingWorker::processMedia(const QString& filePath) {
    progress_timer_.start(); // Start progress throttling timer
    
    ve::ui::MediaInfo info;
    info.filePath = filePath;
    info.success = false;
    
    try {
        // Emit throttled progress update (max ~20 Hz)
        if (progress_timer_.elapsed() > 50) {
            emit progressUpdate(10, "Analyzing media file...");
            progress_timer_.restart();
        }
        
        // Probe the media file in background thread - NO UI OPERATIONS HERE
        info.probeResult = ve::media::probe_file(filePath.toStdString());
        info.success = info.probeResult.success;
        
        if (!info.success) {
            info.errorMessage = QString::fromStdString(info.probeResult.error_message);
        } else {
            // Final progress update
            emit progressUpdate(100, "Media analysis complete");
        }
    } catch (const std::exception& e) {
        info.errorMessage = QString("Exception during media processing: %1").arg(e.what());
    } catch (...) {
        info.errorMessage = "Unknown error during media processing";
    }
    
    emit mediaProcessed(info);
}

// Implementation
void TimelineProcessingWorker::processForTimeline(const QString& filePath) {
    progress_timer_.start(); // Start progress throttling timer
    
    ve::ui::TimelineInfo info;
    info.filePath = filePath;
    info.success = false;
    info.has_video = false;
    info.has_audio = false;
    info.duration_seconds = 0.0;
    // Retrieve placement context from this worker's dynamic properties (set before invoke)
    QVariant startProp = this->property("timeline_start_time_us");
    QVariant trackProp = this->property("timeline_track_index");
    if(startProp.isValid()) {
        int64_t us = startProp.toLongLong();
        info.start_time = ve::TimePoint{us, 1000000};
    }
    if(trackProp.isValid()) {
        info.track_index = trackProp.toInt();
    }
    
    try {
        // Emit throttled progress update (max ~20 Hz)
        if (progress_timer_.elapsed() > 50) {
            emit progressUpdate(20, "Processing for timeline...");
            progress_timer_.restart();
        }
        
        // Probe the media file in background thread - NO UI OPERATIONS HERE
        info.probeResult = ve::media::probe_file(filePath.toStdString());
        
        if (!info.probeResult.success) {
            info.errorMessage = QString::fromStdString(info.probeResult.error_message);
            emit timelineProcessed(info);
            return;
        }
        
        // Progress update
        if (progress_timer_.elapsed() > 50) {
            emit progressUpdate(60, "Analyzing streams...");
            progress_timer_.restart();
        }
        
        // Check if media has video or audio streams - lightweight operation
        for (const auto& stream : info.probeResult.streams) {
            if (stream.type == "video") info.has_video = true;
            if (stream.type == "audio") info.has_audio = true;
        }
        
        if (!info.has_video && !info.has_audio) {
            info.errorMessage = "No video or audio streams found in the media file.";
            emit timelineProcessed(info);
            return;
        }
        
        info.duration_seconds = static_cast<double>(info.probeResult.duration_us) / 1000000.0;
        info.success = true;
        
        // Final progress update
        emit progressUpdate(100, "Timeline processing complete");
        
    } catch (const std::exception& e) {
        info.errorMessage = QString("Exception during timeline processing: %1").arg(e.what());
    } catch (...) {
        info.errorMessage = "Unknown error during timeline processing";
    }
    
    emit timelineProcessed(info);
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , viewer_panel_(nullptr)
    , timeline_dock_(nullptr)
    , timeline_panel_(nullptr)
    , media_browser_dock_(nullptr)
    , media_browser_(nullptr)
    , properties_dock_(nullptr)
    , property_panel_(nullptr)
    , timeline_(nullptr)
    , playback_controller_(nullptr)
    , command_history_(std::make_unique<ve::commands::CommandHistory>())
    , position_update_timer_(nullptr)
    , project_modified_(false)
    , media_worker_(nullptr)
    , media_worker_thread_(nullptr)
    , timeline_worker_(nullptr)
    , timeline_worker_thread_(nullptr)
{
    setWindowTitle("Video Editor");
    setMinimumSize(1200, 800);
    
    // Set up media processing worker thread
    setup_media_worker();
    setup_timeline_worker();
    
    create_menus();
    create_toolbars();
    create_status_bar();
    create_dock_widgets();
    setup_layout();
    connect_signals();
    update_actions();
    
    // Setup position update timer for playback
    position_update_timer_ = new QTimer(this);
    connect(position_update_timer_, &QTimer::timeout, this, &MainWindow::update_playback_position);
    position_update_timer_->setInterval(33); // ~30 FPS updates
    
    // Setup chunked timeline processing to prevent UI freezes
    timeline_update_pump_ = new QTimer(this);
    timeline_update_pump_->setSingleShot(true);
    connect(timeline_update_pump_, &QTimer::timeout, this, &MainWindow::flushTimelineBatch);
    
    // Restore window state
    QSettings settings;
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    // Set initial status message to guide the user
    status_label_->setText("Welcome! Create a new project to get started: File → New Project");
    status_label_->setStyleSheet("color: blue; font-weight: bold;");
    
    ve::log::info("Main window created");
}

MainWindow::~MainWindow() {
    // Clean up worker threads
    cleanup_timeline_worker();
    cleanup_media_worker();

    // Detach playback callbacks to prevent late thread invokes during shutdown
    if (playback_controller_) {
        playback_controller_->set_video_callback(nullptr);
        playback_controller_->set_state_callback(nullptr);
    }
    
    // Save window state
    QSettings settings;
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    ve::log::info("Main window destroyed");
}

void MainWindow::setup_media_worker() {
    // Create worker thread for media processing
    media_worker_thread_ = new QThread(this);
    media_worker_ = new MediaProcessingWorker();
    
    // Move worker to the thread
    media_worker_->moveToThread(media_worker_thread_);
    
    // Connect signals with explicit QueuedConnection for thread safety
    connect(media_worker_, &MediaProcessingWorker::mediaProcessed,
            this, &MainWindow::on_media_processed, Qt::QueuedConnection);
    connect(media_worker_, &MediaProcessingWorker::progressUpdate,
            this, &MainWindow::on_media_progress, Qt::QueuedConnection);
    connect(media_worker_, &MediaProcessingWorker::errorOccurred,
            this, &MainWindow::on_media_processing_error, Qt::QueuedConnection);
    
    // Clean up worker when thread finishes
    connect(media_worker_thread_, &QThread::finished,
            media_worker_, &QObject::deleteLater);
    
    // Start the worker thread
    media_worker_thread_->start();
    
    ve::log::info("Media processing worker thread started");
}

void MainWindow::setup_timeline_worker() {
    // Create worker thread for timeline processing
    timeline_worker_thread_ = new QThread(this);
    timeline_worker_ = new TimelineProcessingWorker();
    
    // Move worker to the thread
    timeline_worker_->moveToThread(timeline_worker_thread_);
    
    // Connect signals with explicit QueuedConnection for thread safety
    connect(timeline_worker_, &TimelineProcessingWorker::timelineProcessed,
            this, &MainWindow::on_timeline_processed, Qt::QueuedConnection);
    connect(timeline_worker_, &TimelineProcessingWorker::progressUpdate,
            this, &MainWindow::on_timeline_progress, Qt::QueuedConnection);
    connect(timeline_worker_, &TimelineProcessingWorker::errorOccurred,
            this, &MainWindow::on_timeline_processing_error, Qt::QueuedConnection);
    
    // Clean up worker when thread finishes
    connect(timeline_worker_thread_, &QThread::finished,
            timeline_worker_, &QObject::deleteLater);
    
    // Start the worker thread
    timeline_worker_thread_->start();
    
    ve::log::info("Timeline processing worker thread started");
}

void MainWindow::cleanup_media_worker() {
    if (media_worker_thread_) {
        // Request thread to quit and wait for completion
        media_worker_thread_->quit();
        if (!media_worker_thread_->wait(3000)) {
            // Force terminate if it doesn't quit gracefully
            media_worker_thread_->terminate();
            media_worker_thread_->wait(1000);
        }
        
        media_worker_thread_ = nullptr;
        media_worker_ = nullptr;
        
        ve::log::info("Media processing worker thread stopped");
    }
}

void MainWindow::cleanup_timeline_worker() {
    if (timeline_worker_thread_) {
        // Request thread to quit and wait for completion
        timeline_worker_thread_->quit();
        if (!timeline_worker_thread_->wait(3000)) {
            // Force terminate if it doesn't quit gracefully
            timeline_worker_thread_->terminate();
            timeline_worker_thread_->wait(1000);
        }
        
        timeline_worker_thread_ = nullptr;
        timeline_worker_ = nullptr;
        
        ve::log::info("Timeline processing worker thread stopped");
    }
}

void MainWindow::set_timeline(ve::timeline::Timeline* timeline) {
    timeline_ = timeline;
    if (timeline_panel_) {
        timeline_panel_->set_timeline(timeline);
    }
    update_window_title();
}

void MainWindow::set_playback_controller(ve::playback::PlaybackController* controller) {
    playback_controller_ = controller;
    if (viewer_panel_) {
        viewer_panel_->set_playback_controller(controller);
    }
    
    // Connect playback state changes to timer control
    if (controller) {
        controller->add_state_callback([this](ve::playback::PlaybackState state) {
            if (state == ve::playback::PlaybackState::Playing) {
                if (position_update_timer_) position_update_timer_->start();
            } else {
                if (position_update_timer_) position_update_timer_->stop();
            }
        });
    }
}

void MainWindow::on_media_processed(const ve::ui::MediaInfo& info) {
    if (info.success) {
        // Add to media browser on UI thread
        add_media_to_browser(info.filePath, info.probeResult);
        
        status_label_->setText("Media file imported successfully!");
        status_label_->setStyleSheet("color: green;");
        ve::log::info("Successfully imported file: " + info.filePath.toStdString());
    } else {
        status_label_->setText("Failed to import media file!");
        status_label_->setStyleSheet("color: red;");
        QMessageBox::warning(this, "Import Failed", 
                           QString("Could not import '%1':\n%2")
                           .arg(QFileInfo(info.filePath).fileName())
                           .arg(info.errorMessage));
        ve::log::warn("Failed to import file '" + info.filePath.toStdString() + "': " + info.errorMessage.toStdString());
    }
}

void MainWindow::on_media_processing_error(const QString& error) {
    status_label_->setText("Media processing error!");
    status_label_->setStyleSheet("color: red;");
    QMessageBox::critical(this, "Media Processing Error", error);
    ve::log::error("Media processing error: " + error.toStdString());
}

void MainWindow::on_media_progress(int percentage, const QString& status) {
    // Throttled progress updates from worker - safe to update UI
    status_label_->setText(QString("%1 (%2%)").arg(status).arg(percentage));
    status_label_->setStyleSheet("color: blue;");
}

void MainWindow::on_timeline_processed(const ve::ui::TimelineInfo& info) {
    SCOPE("on_timeline_processed");
    
    if (!info.success) {
        status_label_->setText("Failed to add media to timeline!");
        status_label_->setStyleSheet("color: red;");
        QMessageBox::warning(this, "Add to Timeline Failed", 
                           QString("Could not add media to timeline:\n%1")
                           .arg(info.errorMessage));
        return;
    }
    
    // Queue the timeline update for chunked processing to prevent UI freeze
    timeline_update_queue_.enqueue(info);
    if (!timeline_update_pump_->isActive()) {
        timeline_update_pump_->start(0); // Start pumping immediately
    }
    
    ve::log::info("Timeline update queued for background processing");
}

void MainWindow::on_timeline_processing_error(const QString& error) {
    status_label_->setText("Timeline processing error!");
    status_label_->setStyleSheet("color: red;");
    QMessageBox::critical(this, "Timeline Processing Error", error);
    ve::log::error("Timeline processing error: " + error.toStdString());
}

void MainWindow::on_timeline_progress(int percentage, const QString& status) {
    // Throttled progress updates from worker - safe to update UI
    status_label_->setText(QString("%1 (%2%)").arg(status).arg(percentage));
    status_label_->setStyleSheet("color: blue;");
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (project_modified_) {
        auto reply = QMessageBox::question(this, "Save Changes?",
            "The project has been modified. Do you want to save your changes?",
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            
        if (reply == QMessageBox::Save) {
            save_project();
            event->accept();
        } else if (reply == QMessageBox::Discard) {
            event->accept();
        } else {
            event->ignore();
            return;
        }
    }
    
    event->accept();
}

void MainWindow::create_menus() {
    // Helper lambda to reduce QAction boilerplate
    auto mk = [this](QMenu* menu, QAction*& slot_store, const QString& text, const QKeySequence& shortcut, auto method, bool checkable=false, bool checked=true){
        slot_store = new QAction(text, this);
        if(!shortcut.isEmpty()) slot_store->setShortcut(shortcut);
        if(checkable){ slot_store->setCheckable(true); slot_store->setChecked(checked);}    
        connect(slot_store, &QAction::triggered, this, method);
        menu->addAction(slot_store);
        return slot_store;
    };

    QMenu* file_menu = menuBar()->addMenu("&File");
    mk(file_menu, new_action_, "&New Project", QKeySequence::New, &MainWindow::new_project);
    mk(file_menu, open_action_, "&Open Project...", QKeySequence::Open, &MainWindow::open_project);
    file_menu->addSeparator();
    mk(file_menu, save_action_, "&Save Project", QKeySequence::Save, &MainWindow::save_project);
    mk(file_menu, save_as_action_, "Save Project &As...", QKeySequence::SaveAs, &MainWindow::save_project_as);
    file_menu->addSeparator();
    mk(file_menu, import_action_, "&Import Media...", QKeySequence(QStringLiteral("Ctrl+I")), &MainWindow::import_media);
    mk(file_menu, export_action_, "&Export Timeline...", QKeySequence(QStringLiteral("Ctrl+E")), &MainWindow::export_timeline);
    file_menu->addSeparator();
    mk(file_menu, quit_action_, "&Quit", QKeySequence::Quit, &MainWindow::quit_application);

    QMenu* edit_menu = menuBar()->addMenu("&Edit");
    mk(edit_menu, undo_action_, "&Undo", QKeySequence::Undo, &MainWindow::undo);
    mk(edit_menu, redo_action_, "&Redo", QKeySequence::Redo, &MainWindow::redo);
    edit_menu->addSeparator();
    mk(edit_menu, cut_action_, "Cu&t", QKeySequence::Cut, &MainWindow::cut);
    mk(edit_menu, copy_action_, "&Copy", QKeySequence::Copy, &MainWindow::copy);
    mk(edit_menu, paste_action_, "&Paste", QKeySequence::Paste, &MainWindow::paste);
    mk(edit_menu, delete_action_, "&Delete", QKeySequence::Delete, &MainWindow::delete_selection);
    edit_menu->addSeparator();
    mk(edit_menu, add_to_timeline_action_, "Add Selected Media to &Timeline", QKeySequence(QStringLiteral("Ctrl+T")), &MainWindow::add_selected_media_to_timeline);

    QMenu* playback_menu = menuBar()->addMenu("&Playback");
    mk(playback_menu, play_pause_action_, "&Play/Pause", QKeySequence(QStringLiteral("Space")), &MainWindow::play_pause);
    mk(playback_menu, stop_action_, "&Stop", QKeySequence(QStringLiteral("Ctrl+Space")), &MainWindow::stop);
    playback_menu->addSeparator();
    mk(playback_menu, step_forward_action_, "Step &Forward", QKeySequence(QStringLiteral("Right")), &MainWindow::step_forward);
    mk(playback_menu, step_backward_action_, "Step &Backward", QKeySequence(QStringLiteral("Left")), &MainWindow::step_backward);
    playback_menu->addSeparator();
    mk(playback_menu, go_to_start_action_, "Go to &Start", QKeySequence(QStringLiteral("Home")), &MainWindow::go_to_start);
    mk(playback_menu, go_to_end_action_, "Go to &End", QKeySequence(QStringLiteral("End")), &MainWindow::go_to_end);

    QMenu* view_menu = menuBar()->addMenu("&View");
    QAction* dummy=nullptr; // temporary holder for helper
    mk(view_menu, dummy, "Zoom &In", QKeySequence(QStringLiteral("Ctrl++")), &MainWindow::zoom_in);
    mk(view_menu, dummy, "Zoom &Out", QKeySequence(QStringLiteral("Ctrl+-")), &MainWindow::zoom_out);
    mk(view_menu, dummy, "Zoom &Fit", QKeySequence(QStringLiteral("Ctrl+0")), &MainWindow::zoom_fit);
    view_menu->addSeparator();
    mk(view_menu, dummy, "&Timeline", QKeySequence(), &MainWindow::toggle_timeline, true, true);
    mk(view_menu, dummy, "&Media Browser", QKeySequence(), &MainWindow::toggle_media_browser, true, true);
    mk(view_menu, dummy, "&Properties", QKeySequence(), &MainWindow::toggle_properties, true, true);

    QMenu* help_menu = menuBar()->addMenu("&Help");
    mk(help_menu, dummy, "&About", QKeySequence(), &MainWindow::about);
    mk(help_menu, dummy, "About &Qt", QKeySequence(), &MainWindow::about_qt);

    QMenu* dev_menu = menuBar()->addMenu("&Development");
    mk(dev_menu, dummy, "Create &Test Timeline Content", QKeySequence(), &MainWindow::create_test_timeline_content);
}

void MainWindow::create_toolbars() {
    // Main toolbar
    QToolBar* main_toolbar = addToolBar("Main");
    main_toolbar->setObjectName("MainToolbar");
    main_toolbar->addAction(new_action_);
    main_toolbar->addAction(open_action_);
    main_toolbar->addAction(save_action_);
    main_toolbar->addSeparator();
    main_toolbar->addAction(import_action_);
    main_toolbar->addAction(export_action_);
    
    // Edit toolbar
    QToolBar* edit_toolbar = addToolBar("Edit");
    edit_toolbar->setObjectName("EditToolbar");
    edit_toolbar->addAction(undo_action_);
    edit_toolbar->addAction(redo_action_);
    edit_toolbar->addSeparator();
    edit_toolbar->addAction(cut_action_);
    edit_toolbar->addAction(copy_action_);
    edit_toolbar->addAction(paste_action_);
    edit_toolbar->addAction(delete_action_);
    
    // Playback toolbar
    QToolBar* playback_toolbar = addToolBar("Playback");
    playback_toolbar->setObjectName("PlaybackToolbar");
    playback_toolbar->addAction(go_to_start_action_);
    playback_toolbar->addAction(step_backward_action_);
    playback_toolbar->addAction(play_pause_action_);
    playback_toolbar->addAction(step_forward_action_);
    playback_toolbar->addAction(stop_action_);
    playback_toolbar->addAction(go_to_end_action_);
}

void MainWindow::create_status_bar() {
    status_label_ = new QLabel("Ready");
    statusBar()->addWidget(status_label_);
    
    statusBar()->addPermanentWidget(new QLabel("|"));
    
    time_label_ = new QLabel("00:00:00.000");
    statusBar()->addPermanentWidget(time_label_);
    
    statusBar()->addPermanentWidget(new QLabel("|"));
    
    fps_label_ = new QLabel("30 fps");
    statusBar()->addPermanentWidget(fps_label_);
}

void MainWindow::create_dock_widgets() {
    // Timeline dock
    timeline_dock_ = new QDockWidget("Timeline", this);
    timeline_dock_->setObjectName("TimelineDock");
    timeline_panel_ = new TimelinePanel();
    
    // Connect command system to timeline panel
    timeline_panel_->set_command_executor([this](std::unique_ptr<ve::commands::Command> cmd) {
        return execute_command(std::move(cmd));
    });
    
    timeline_dock_->setWidget(timeline_panel_);
    timeline_dock_->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, timeline_dock_);
    
    // Media browser dock - Tree widget for media files
    media_browser_dock_ = new QDockWidget("Media Browser", this);
    media_browser_dock_->setObjectName("MediaBrowserDock");
    media_browser_ = new QTreeWidget();
    media_browser_->setHeaderLabels({"Name", "Duration", "Format", "Resolution"});
    media_browser_->setRootIsDecorated(false);
    media_browser_->setAlternatingRowColors(true);
    media_browser_->setDragDropMode(QAbstractItemView::DragOnly);
    media_browser_->setDefaultDropAction(Qt::CopyAction);
    
    // Add helpful placeholder item
    add_media_browser_placeholder();
    
    media_browser_dock_->setWidget(media_browser_);
    media_browser_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, media_browser_dock_);
    
    // Properties dock (placeholder for now)
    properties_dock_ = new QDockWidget("Properties", this);
    properties_dock_->setObjectName("PropertiesDock");
    property_panel_ = new QLabel("Properties\n(Coming Soon)");
    property_panel_->setAlignment(Qt::AlignCenter);
    properties_dock_->setWidget(property_panel_);
    properties_dock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, properties_dock_);
}

void MainWindow::setup_layout() {
    // Central widget - viewer panel
    viewer_panel_ = new ViewerPanel();
    setCentralWidget(viewer_panel_);
    
    // Set dock widget sizes
    resizeDocks({timeline_dock_}, {300}, Qt::Vertical);
    resizeDocks({media_browser_dock_, properties_dock_}, {250, 250}, Qt::Horizontal);
}

void MainWindow::connect_signals() {
    // Connect media browser signals
    if (media_browser_) {
        qDebug() << "Setting up media browser signals and context menu";
        connect(media_browser_, &QTreeWidget::itemDoubleClicked,
                this, &MainWindow::on_media_item_double_clicked);
        
        // Enable context menu for media browser
        media_browser_->setContextMenuPolicy(Qt::CustomContextMenu);
        qDebug() << "Context menu policy set to CustomContextMenu";
        
        connect(media_browser_, &QTreeWidget::customContextMenuRequested,
                this, &MainWindow::on_media_browser_context_menu);
        qDebug() << "Context menu signal connected";
    } else {
        qDebug() << "Warning: media_browser_ is null!";
    }
    
    // Connect timeline panel signals
    if (timeline_panel_) {
        // Debounced seek scheduling to reduce excessive decoder seeks during rapid scrubbing
        static QTimer* seekDebounceTimer = nullptr; // static ensures single instance tied to lambda lifetime
        if(!seekDebounceTimer) {
            seekDebounceTimer = new QTimer(this);
            seekDebounceTimer->setSingleShot(true);
        }
        static int64_t pendingSeekUs = -1;
        connect(timeline_panel_, &TimelinePanel::time_changed,
                this, [this](ve::TimePoint tp){
                    auto r = tp.to_rational();
                    pendingSeekUs = (r.num * 1000000) / r.den;
                    if(time_label_) time_label_->setText(QString("Time: %1s").arg(pendingSeekUs/1'000'000.0,0,'f',2));
                    if(!seekDebounceTimer) return; // safety
                    // Restart debounce (issue seek after 35ms inactivity)
                    seekDebounceTimer->stop();
                    seekDebounceTimer->setInterval(35); // Tune N ms here
                    seekDebounceTimer->start();
                });
        connect(seekDebounceTimer, &QTimer::timeout, this, [this](){
            if(pendingSeekUs >= 0 && playback_controller_) {
                bool was_playing = playback_controller_->state() == ve::playback::PlaybackState::Playing;
                playback_controller_->seek(pendingSeekUs);
                if(!was_playing) { /* preview decode handled internally */ }
            }
        });
        connect(timeline_panel_, &TimelinePanel::clip_added,
                this, &MainWindow::on_timeline_clip_added);
    }
    
    // Connect viewer panel signals
    if (viewer_panel_) {
        connect(viewer_panel_, &ViewerPanel::play_pause_requested,
                this, &MainWindow::play_pause);
        connect(viewer_panel_, &ViewerPanel::stop_requested,
                this, &MainWindow::stop);
    }

    // Connect to Application signals for project lifecycle & modifications
    if(auto* app = ve::app::Application::instance()) {
        connect(app, &ve::app::Application::project_changed, this, &MainWindow::on_project_state_changed);
        connect(app, &ve::app::Application::project_modified, this, &MainWindow::on_project_dirty);
    }
}

void MainWindow::update_window_title() {
    QString title = "Video Editor";
    if (timeline_) {
        title += " - " + QString::fromStdString(timeline_->name());
        if (project_modified_) {
            title += "*";
        }
    }
    setWindowTitle(title);
}

void MainWindow::update_actions() {
    bool has_timeline = timeline_ != nullptr;
    bool has_playback = playback_controller_ != nullptr;
    bool has_command_history = command_history_ != nullptr;
    
    // Enable/disable actions based on state
    save_action_->setEnabled(has_timeline && project_modified_);
    save_as_action_->setEnabled(has_timeline);
    export_action_->setEnabled(has_timeline);
    
    // Undo/Redo actions
    if (has_command_history && has_timeline) {
        undo_action_->setEnabled(command_history_->can_undo());
        redo_action_->setEnabled(command_history_->can_redo());
        
        // Update action text with descriptions
        std::string undo_desc = command_history_->undo_description();
        std::string redo_desc = command_history_->redo_description();
        
        if (!undo_desc.empty()) {
            undo_action_->setText(QString("&Undo %1").arg(QString::fromStdString(undo_desc)));
        } else {
            undo_action_->setText("&Undo");
        }
        
        if (!redo_desc.empty()) {
            redo_action_->setText(QString("&Redo %1").arg(QString::fromStdString(redo_desc)));
        } else {
            redo_action_->setText("&Redo");
        }
    } else {
        undo_action_->setEnabled(false);
        redo_action_->setEnabled(false);
        undo_action_->setText("&Undo");
        redo_action_->setText("&Redo");
    }
    
    play_pause_action_->setEnabled(has_playback);
    stop_action_->setEnabled(has_playback);
    step_forward_action_->setEnabled(has_playback);
    step_backward_action_->setEnabled(has_playback);
    go_to_start_action_->setEnabled(has_playback);
    go_to_end_action_->setEnabled(has_playback);
    
    // Update status bar with helpful guidance
    if (!has_timeline) {
        status_label_->setText("No project loaded. Use File → New Project to get started.");
        status_label_->setStyleSheet("color: orange; font-weight: bold;");
    } else if (has_timeline && media_browser_ && media_browser_->topLevelItemCount() == 0) {
        status_label_->setText("Project ready. Use File → Import Media to add content.");
        status_label_->setStyleSheet("color: blue;");
    } else if (has_timeline && media_browser_ && media_browser_->topLevelItemCount() > 0) {
        status_label_->setText("Ready - drag media to timeline or right-click for options.");
        status_label_->setStyleSheet("color: green;");
    }
}

// Slot implementations (stubs for now)
void MainWindow::new_project() { 
    ve::log::info("New project requested");
    
    // Initialize a new timeline if we don't have one
    if (!timeline_) {
        timeline_ = new ve::timeline::Timeline();
        
        // Set the timeline in the timeline panel
        if (timeline_panel_) {
            timeline_panel_->set_timeline(timeline_);
        }
        
        // Initialize or reset playback controller
        if (!playback_controller_) {
            playback_controller_ = new ve::playback::PlaybackController();
            set_playback_controller(playback_controller_);
        } else {
            playback_controller_->set_video_callback(nullptr);
            playback_controller_->clear_state_callbacks();
            set_playback_controller(playback_controller_); // rewire callbacks
        }

        // Update UI state
        project_modified_ = false;
        setWindowTitle("Video Editor - New Project");
        
        status_label_->setText("New project created. Use File → Import Media to add content.");
        status_label_->setStyleSheet("color: green;");
        
        QMessageBox::information(this, "New Project Created", 
            "New project created successfully!\n\n"
            "Next steps:\n"
            "1. Go to File → Import Media to add video/audio files\n"
            "2. Drag media from the Media Browser to the Timeline\n"
            "3. Right-click on media items for additional options");
        
        ve::log::info("New project initialized successfully");
    } else {
        // Project already exists, ask if user wants to create a new one
        QMessageBox::StandardButton reply = QMessageBox::question(this, "New Project",
            "A project is already open. Create a new project?\n\n"
            "Note: Any unsaved changes will be lost.",
            QMessageBox::Yes | QMessageBox::No);
            
        if (reply == QMessageBox::Yes) {
            // Create a new timeline to replace the existing one
            delete timeline_;
            timeline_ = new ve::timeline::Timeline();
            
            // Update timeline panel with new timeline
            if (timeline_panel_) {
                timeline_panel_->set_timeline(timeline_);
            }
            
            // Clear media browser
            if (media_browser_) {
                media_browser_->clear();
                add_media_browser_placeholder(); // Add helpful placeholder
            }
            
            // Reset UI state
            project_modified_ = false;
            setWindowTitle("Video Editor - New Project");
            status_label_->setText("New project created. Use File → Import Media to add content.");
            status_label_->setStyleSheet("color: green;");
            
            ve::log::info("New project created, previous project cleared");
        }
    }
}
void MainWindow::open_project() { ve::log::info("Open project requested"); }
void MainWindow::save_project() { ve::log::info("Save project requested"); }
void MainWindow::save_project_as() { ve::log::info("Save project as requested"); }
void MainWindow::import_media() {
    ve::log::info("Import media requested");
    
    // Show file dialog for media selection
    QStringList filters;
    filters << "Video Files (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm *.m4v)"
            << "Audio Files (*.mp3 *.wav *.aac *.flac *.ogg *.wma *.m4a)"
            << "All Media Files (*.mp4 *.avi *.mov *.mkv *.wmv *.flv *.webm *.m4v *.mp3 *.wav *.aac *.flac *.ogg *.wma *.m4a)"
            << "All Files (*.*)";
    
    QString lastDir = QSettings().value("last_import_directory", 
                                       QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)).toString();
    
    QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        "Import Media Files",
        lastDir,
        filters.join(";;")
    );
    
    if (filePaths.isEmpty()) {
        ve::log::info("Import cancelled by user");
        return;
    }
    
    // Save the directory for next time
    QSettings().setValue("last_import_directory", QFileInfo(filePaths.first()).dir().absolutePath());
    
    // Process each selected file
    for (const QString& filePath : filePaths) {
        import_single_file(filePath);
    }
}
void MainWindow::export_timeline() { ve::log::info("Export timeline requested"); }
void MainWindow::quit_application() { close(); }

void MainWindow::undo() { 
    if (!timeline_ || !command_history_) {
        ve::log::debug("Cannot undo: no timeline or command history");
        return;
    }
    
    bool success = command_history_->undo(*timeline_);
        if (success) {
            project_modified_ = true;
            // Direct UI flag for immediate feedback
            update_window_title();
            // Propagate to Application via timeline callback
            if (timeline_) timeline_->mark_modified();
            update_actions();
        
        // Refresh timeline display
        if (timeline_panel_) {
            timeline_panel_->refresh();
        }
        
        // Update status
        std::string desc = command_history_->undo_description();
        if (!desc.empty()) {
            status_label_->setText(QString("Undid: %1").arg(QString::fromStdString(desc)));
        } else {
            status_label_->setText("Undo completed");
        }
    } else {
        status_label_->setText("Undo failed");
    }
}

void MainWindow::redo() { 
    if (!timeline_ || !command_history_) {
        ve::log::debug("Cannot redo: no timeline or command history");
        return;
    }
    
    bool success = command_history_->redo(*timeline_);
        if (success) {
            project_modified_ = true;
            // Propagate to Application via timeline callback
            if (timeline_) timeline_->mark_modified();
            update_window_title();
            update_actions();
        
        // Refresh timeline display
        if (timeline_panel_) {
            timeline_panel_->refresh();
        }
        
        // Update status
        std::string desc = command_history_->redo_description();
        if (!desc.empty()) {
            status_label_->setText(QString("Redid: %1").arg(QString::fromStdString(desc)));
        } else {
            status_label_->setText("Redo completed");
        }
    } else {
        status_label_->setText("Redo failed");
    }
}
void MainWindow::cut() { ve::log::info("Cut requested"); }
void MainWindow::copy() { ve::log::info("Copy requested"); }
void MainWindow::paste() { ve::log::info("Paste requested"); }
void MainWindow::delete_selection() { ve::log::info("Delete requested"); }

void MainWindow::play_pause() { 
    if (playback_controller_) {
        if (playback_controller_->state() == ve::playback::PlaybackState::Playing) {
            playback_controller_->pause();
        } else {
            playback_controller_->play();
        }
    }
}


void MainWindow::stop() {
    if (playback_controller_) {
        playback_controller_->stop();
        playback_controller_->seek(0);
    }
    if (timeline_panel_) {
        timeline_panel_->set_current_time(us_to_tp(0));
        timeline_panel_->update();
    }
    if (time_label_) time_label_->setText(QString("Time: %1s").arg(0.0, 0, 'f', 2));
}

void MainWindow::step_forward() {
    if (!playback_controller_) return;
    playback_controller_->pause();
    qint64 frame_us = playback_controller_->frame_duration_guess_us();
    qint64 cur_us = playback_controller_->current_time_us();
    qint64 dur_us = timeline_duration_us(timeline_);
    qint64 tgt = cur_us + frame_us; if(dur_us>0 && tgt>dur_us) tgt = dur_us;
    playback_controller_->seek(tgt);
    playback_controller_->step_once(); // decode one frame
    if (timeline_panel_) { timeline_panel_->set_current_time(us_to_tp(tgt)); timeline_panel_->update(); }
    if (time_label_) time_label_->setText(QString("Time: %1s").arg(tgt / 1'000'000.0, 0, 'f', 2));
}


void MainWindow::step_backward() {
    if (!playback_controller_) return;
    playback_controller_->pause();
    qint64 frame_us = playback_controller_->frame_duration_guess_us();
    qint64 cur_us = playback_controller_->current_time_us();
    qint64 tgt = cur_us - frame_us; if(tgt < 0) tgt = 0;
    playback_controller_->seek(tgt);
    playback_controller_->step_once();
    if (timeline_panel_) { timeline_panel_->set_current_time(us_to_tp(tgt)); timeline_panel_->update(); }
    if (time_label_) time_label_->setText(QString("Time: %1s").arg(tgt / 1'000'000.0, 0, 'f', 2));
}

void MainWindow::go_to_start() { 
    if (playback_controller_) {
        playback_controller_->seek(0);
        if (timeline_panel_) timeline_panel_->set_current_time(ve::TimePoint{0, 1000000});
        if (time_label_) time_label_->setText(QString("Time: %1s").arg(0.0, 0, 'f', 2));
    }
}

void MainWindow::go_to_end() { 
    ve::log::info("Go to end requested"); 
    if (playback_controller_) {
        int64_t duration = playback_controller_->duration_us();
        ve::log::info("Duration: " + std::to_string(duration) + " us");
        if (duration > 0) {
            int64_t target_time = duration - 1000; // Go to end minus 1ms
            ve::log::info("Going to end: " + std::to_string(target_time) + " us");
            playback_controller_->seek(target_time);
            if (timeline_panel_) timeline_panel_->set_current_time(ve::TimePoint{target_time, 1000000});
            if (time_label_) time_label_->setText(QString("Time: %1s").arg(target_time / 1000000.0, 0, 'f', 2));
        } else {
            ve::log::warn("Cannot go to end: duration is 0 or unknown");
        }
    }
}

void MainWindow::zoom_in() { 
    if (timeline_panel_) {
        timeline_panel_->zoom_in();
    }
}

void MainWindow::zoom_out() { 
    if (timeline_panel_) {
        timeline_panel_->zoom_out();
    }
}

void MainWindow::zoom_fit() { 
    if (timeline_panel_) {
        timeline_panel_->zoom_fit();
    }
}

void MainWindow::toggle_timeline() { timeline_dock_->setVisible(!timeline_dock_->isVisible()); }
void MainWindow::toggle_media_browser() { media_browser_dock_->setVisible(!media_browser_dock_->isVisible()); }
void MainWindow::toggle_properties() { properties_dock_->setVisible(!properties_dock_->isVisible()); }

void MainWindow::about() {
    QMessageBox::about(this, "About Video Editor",
        "Professional Video Editor v0.1\n\n"
        "Built with Qt and modern C++\n"
        "© 2025 Video Editor Team");
}

void MainWindow::about_qt() {
    QMessageBox::aboutQt(this);
}

void MainWindow::on_playback_time_changed(ve::TimePoint time) {
    // Convert TimePoint to microseconds
    const auto& rational = time.to_rational();
    int64_t time_us = (rational.num * 1000000) / rational.den;
    
    // Update time display
    int64_t total_ms = time_us / 1000;
    int hours = static_cast<int>(total_ms / 3600000);
    int minutes = static_cast<int>((total_ms % 3600000) / 60000);
    int seconds = static_cast<int>((total_ms % 60000) / 1000);
    int milliseconds = static_cast<int>(total_ms % 1000);
    
    QString time_str = QString("%1:%2:%3.%4")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milliseconds, 3, 10, QChar('0'));
        
    time_label_->setText(time_str);
    
    // Update timeline
    if (timeline_panel_) {
        timeline_panel_->set_current_time(ve::TimePoint{time_us, 1000000});
    }
}

void MainWindow::on_playback_state_changed() {
    update_actions();
}

void MainWindow::import_single_file(const QString& filePath) {
    ve::log::info("Importing file: " + filePath.toStdString());
    
    // Update status bar to show processing has started
    status_label_->setText("Processing media file in background...");
    status_label_->setStyleSheet("color: blue;");
    
    // Process the media file in the background worker thread
    QMetaObject::invokeMethod(media_worker_, "processMedia", 
                             Qt::QueuedConnection,
                             Q_ARG(QString, filePath));
}

void MainWindow::add_media_to_browser(const QString& filePath, const ve::media::ProbeResult& probe) {
    // Remove placeholder items when adding real media
    remove_media_browser_placeholder();
    
    auto* item = new QTreeWidgetItem(media_browser_);
    
    // Store the full path as user data for later use
    item->setData(0, Qt::UserRole, filePath);
    
    // Column 0: File name
    item->setText(0, QFileInfo(filePath).fileName());
    
    // Column 1: Duration
    QString duration = "Unknown";
    if (probe.duration_us > 0) {
        int64_t seconds = probe.duration_us / 1000000;
        int hours = seconds / 3600;
        int minutes = (seconds % 3600) / 60;
        int secs = seconds % 60;
        
        if (hours > 0) {
            duration = QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
        } else {
            duration = QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0'));
        }
    }
    item->setText(1, duration);
    
    // Column 2: Format
    QString format = QString::fromStdString(probe.format);
    if (format.isEmpty()) format = "Unknown";
    item->setText(2, format);
    
    // Column 3: Resolution (for video files)
    QString resolution = "N/A";
    for (const auto& stream : probe.streams) {
        if (stream.type == "video" && stream.width > 0 && stream.height > 0) {
            resolution = QString("%1x%2").arg(stream.width).arg(stream.height);
            if (stream.fps > 0) {
                resolution += QString(" @ %1 fps").arg(stream.fps, 0, 'f', 1);
            }
            break;
        }
    }
    item->setText(3, resolution);
    
    // Set icon based on media type
    bool hasVideo = false, hasAudio = false;
    for (const auto& stream : probe.streams) {
        if (stream.type == "video") hasVideo = true;
        if (stream.type == "audio") hasAudio = true;
    }
    
    // Set tooltip with detailed information
    QString tooltip = QString("File: %1\nFormat: %2\nSize: %3 MB\n")
                     .arg(filePath)
                     .arg(QString::fromStdString(probe.format))
                     .arg(probe.size_bytes / (1024 * 1024));
    
    for (size_t i = 0; i < probe.streams.size(); ++i) {
        const auto& stream = probe.streams[i];
        tooltip += QString("\nStream %1 (%2):").arg(i).arg(QString::fromStdString(stream.type));
        tooltip += QString("\n  Codec: %1").arg(QString::fromStdString(stream.codec));
        
        if (stream.type == "video") {
            tooltip += QString("\n  Resolution: %1x%2").arg(stream.width).arg(stream.height);
            if (stream.fps > 0) tooltip += QString("\n  Frame Rate: %1 fps").arg(stream.fps);
        } else if (stream.type == "audio") {
            tooltip += QString("\n  Sample Rate: %1 Hz").arg(stream.sample_rate);
            tooltip += QString("\n  Channels: %1").arg(stream.channels);
        }
        
        if (stream.bitrate > 0) {
            tooltip += QString("\n  Bitrate: %1 kbps").arg(stream.bitrate / 1000);
        }
    }
    
    item->setToolTip(0, tooltip);
    item->setToolTip(1, tooltip);
    item->setToolTip(2, tooltip);
    item->setToolTip(3, tooltip);
    
    // Resize columns to content
    for (int i = 0; i < 4; ++i) {
        media_browser_->resizeColumnToContents(i);
    }
    
    // Update actions to refresh status bar
    update_actions();
    
    ve::log::info("Added media to browser: " + filePath.toStdString());
}

bool MainWindow::execute_command(std::unique_ptr<ve::commands::Command> command) {
    if (!timeline_ || !command_history_ || !command) {
        ve::log::warn("Cannot execute command: missing timeline, command history, or command is null");
        return false;
    }
    
    bool success = command_history_->execute(std::move(command), *timeline_);
        if (success) {
            project_modified_ = true;
            // Propagate to Application via timeline callback
            if (timeline_) timeline_->mark_modified();
            update_window_title();
            update_actions();
        
        // Refresh timeline display
        if (timeline_panel_) {
            timeline_panel_->refresh();
        }
        
        status_label_->setText("Command executed successfully");
        ve::log::debug("Command executed through command system");
    } else {
        status_label_->setText("Command execution failed");
        ve::log::warn("Command execution failed");
    }
    
    return success;
}

// Application project state changed (new/open/save/close)
void MainWindow::on_project_state_changed() {
    // Reset modified state based on application signal (treated as clean snapshot)
    project_modified_ = false;
    update_window_title();
    update_actions();
}

// Application reported timeline modifications (dirty)
void MainWindow::on_project_dirty() {
    if(!project_modified_) {
        project_modified_ = true;
        update_window_title();
        update_actions();
    }
}

void MainWindow::on_media_item_double_clicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    
    if (!item || !viewer_panel_) return;
    
    // Get the file path from the item data
    QString filePath = item->data(0, Qt::UserRole).toString();
    if (filePath.isEmpty()) return;
    
    ve::log::info("Loading media into viewer: " + filePath.toStdString());
    
    // Load the media in the viewer panel
    if (viewer_panel_->load_media(filePath)) {
        ve::log::info("Media loaded successfully in viewer");
        // Ensure playback controller exists & is wired
        if (!playback_controller_) {
            ve::log::info("Creating playback controller on-demand");
            playback_controller_ = new ve::playback::PlaybackController();
            set_playback_controller(playback_controller_);
        }

        // Also load the media in the playback controller for playback functionality
        if (playback_controller_) {
            if (playback_controller_->load_media(filePath.toStdString())) {
                ve::log::info("Media loaded successfully in playback controller");
                // Autoplay after load (optional UX improvement)
                playback_controller_->play();
            } else {
                ve::log::warn("Failed to load media in playback controller");
            }
        } else {
            ve::log::warn("No playback controller available");
        }
    } else {
        ve::log::warn("Failed to load media in viewer");
    }
}

void MainWindow::on_media_browser_context_menu(const QPoint& pos) {
    qDebug() << "Context menu requested at position:" << pos;
    qDebug() << "media_browser_ pointer:" << (void*)media_browser_;
    
    if (!media_browser_) {
        qDebug() << "media_browser_ is null!";
        return;
    }
    
    QTreeWidgetItem* item = media_browser_->itemAt(pos);
    if (!item) {
        qDebug() << "No item at position - available items:" << media_browser_->topLevelItemCount();
        return;
    }
    
    QString filePath = item->data(0, Qt::UserRole).toString();
    if (filePath.isEmpty()) {
        qDebug() << "No file path in item data";
        return;
    }
    
    qDebug() << "Creating context menu for file:" << filePath;
    
    QMenu contextMenu(this);
    // Avoid deprecated addAction(const QString&) overloads – create actions explicitly
    QAction* addToTimelineAction = new QAction("Add to Timeline", &contextMenu);
    contextMenu.addAction(addToTimelineAction);
    QAction* loadInViewerAction = new QAction("Load in Viewer", &contextMenu);
    contextMenu.addAction(loadInViewerAction);
    
    qDebug() << "Context menu created with" << contextMenu.actions().size() << "actions";
    
    QPoint globalPos = media_browser_->mapToGlobal(pos);
    qDebug() << "Showing context menu at global position:" << globalPos;
    
    QAction* selectedAction = contextMenu.exec(globalPos);
    
    if (selectedAction == addToTimelineAction) {
        qDebug() << "Add to Timeline selected";
        add_media_to_timeline(filePath);
    } else if (selectedAction == loadInViewerAction) {
        qDebug() << "Load in Viewer selected";
        on_media_item_double_clicked(item, 0);
    } else {
        qDebug() << "No action selected or context menu cancelled";
    }
}

void MainWindow::add_media_to_timeline(const QString& filePath) {
    qDebug() << "add_media_to_timeline called with:" << filePath;
    
    if (!timeline_) {
        qDebug() << "No timeline available";
        QMessageBox::warning(this, "Add to Timeline Failed", "No timeline available. Please create a new project first.");
        return;
    }

    // Update status and start background processing
    status_label_->setText("Processing media for timeline in background...");
    status_label_->setStyleSheet("color: blue;");
    
    ve::log::info("Adding media to timeline: " + filePath.toStdString());
    
    // Process the media file in the background timeline worker thread
    // Clear any previous context
    timeline_worker_->setProperty("timeline_start_time_us", QVariant());
    timeline_worker_->setProperty("timeline_track_index", QVariant());
    QMetaObject::invokeMethod(timeline_worker_, "processForTimeline", Qt::QueuedConnection, Q_ARG(QString, filePath));
    
    qDebug() << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "Background processing started";
}

void MainWindow::add_selected_media_to_timeline() {
    qDebug() << "add_selected_media_to_timeline called";
    
    if (!media_browser_) {
        qDebug() << "No media browser available";
        return;
    }
    
    // Get currently selected item in media browser
    QTreeWidgetItem* selectedItem = nullptr;
    QList<QTreeWidgetItem*> selectedItems = media_browser_->selectedItems();
    
    if (selectedItems.isEmpty()) {
        // If no selection, try to get the first item
        if (media_browser_->topLevelItemCount() > 0) {
            selectedItem = media_browser_->topLevelItem(0);
            qDebug() << "No selection, using first item";
        } else {
            QMessageBox::information(this, "Add to Timeline", 
                                   "No media selected. Please import media first and select it in the Media Browser.");
            qDebug() << "No items available in media browser";
            return;
        }
    } else {
        selectedItem = selectedItems.first();
        qDebug() << "Using selected item";
    }
    
    if (!selectedItem) {
        qDebug() << "No valid item found";
        return;
    }
    
    QString filePath = selectedItem->data(0, Qt::UserRole).toString();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "Add to Timeline", "Selected media has no file path.");
        qDebug() << "Selected item has no file path";
        return;
    }
    
    qDebug() << "Adding media to timeline:" << filePath;
    add_media_to_timeline(filePath);
}

void MainWindow::on_timeline_clip_added(const QString& filePath, ve::TimePoint start_time, int track_index) {
    if(!timeline_) {
        ve::log::warn("No timeline available for adding clips");
        return;
    }
    // Store placement context on worker and invoke async probe
    timeline_worker_->setProperty("timeline_start_time_us", QVariant::fromValue<int64_t>(start_time.to_rational().num));
    timeline_worker_->setProperty("timeline_track_index", QVariant(track_index));
    status_label_->setText("Processing media for timeline...");
    status_label_->setStyleSheet("color: blue;");
    QMetaObject::invokeMethod(timeline_worker_, "processForTimeline", Qt::QueuedConnection, Q_ARG(QString, filePath));
}

void MainWindow::add_media_to_timeline(const QString& filePath, ve::TimePoint start_time, int track_index) {
    // Explicit placement version used by drag-drop
    if(!timeline_) {
        QMessageBox::warning(this, "Add to Timeline Failed", "No timeline available. Please create a new project first.");
        return;
    }
    timeline_worker_->setProperty("timeline_start_time_us", QVariant::fromValue<int64_t>(start_time.to_rational().num));
    timeline_worker_->setProperty("timeline_track_index", QVariant(track_index));
    status_label_->setText("Processing media for timeline in background...");
    status_label_->setStyleSheet("color: blue;");
    QMetaObject::invokeMethod(timeline_worker_, "processForTimeline", Qt::QueuedConnection, Q_ARG(QString, filePath));
}

void MainWindow::update_playback_position() {
    if (playback_controller_ && timeline_panel_) {
        auto current_time_us = playback_controller_->current_time_us();
        timeline_panel_->set_current_time(ve::TimePoint{current_time_us, 1000000});
        
        // Update time display in status bar
        if (time_label_) {
            auto seconds = current_time_us / 1000000.0;
            time_label_->setText(QString("Time: %1s").arg(seconds, 0, 'f', 2));
        }
    }
}

void MainWindow::create_test_timeline_content() {
    if (!timeline_) return;
    
    // Add some test tracks
    // Capture created track IDs (potential diagnostics / future use)
    const auto video_track_id = timeline_->add_track(ve::timeline::Track::Video, "Video Track 1");
    const auto audio_track_id = timeline_->add_track(ve::timeline::Track::Audio, "Audio Track 1");
    
    // Get the tracks
    auto* video_track = timeline_->get_track(video_track_id);
    auto* audio_track = timeline_->get_track(audio_track_id);
    
    if (video_track && audio_track) {
        // Create test video segments
        ve::timeline::Segment video_seg1;
        video_seg1.id = video_track->generate_segment_id();
        video_seg1.name = "Test Video Clip 1";
        video_seg1.start_time = ve::TimePoint{0};
        video_seg1.duration = ve::TimeDuration{5000000, 1000000}; // 5 seconds
        
        ve::timeline::Segment video_seg2;
        video_seg2.id = video_track->generate_segment_id();
        video_seg2.name = "Test Video Clip 2";
        video_seg2.start_time = ve::TimePoint{6000000, 1000000}; // 6 seconds
        video_seg2.duration = ve::TimeDuration{4000000, 1000000}; // 4 seconds
        
        // Create test audio segments
        ve::timeline::Segment audio_seg1;
        audio_seg1.id = audio_track->generate_segment_id();
        audio_seg1.name = "Test Audio Clip 1";
        audio_seg1.start_time = ve::TimePoint{0};
        audio_seg1.duration = ve::TimeDuration{9000000, 1000000}; // 9 seconds
        
        // Add segments to tracks
    if(!video_track->add_segment(video_seg1)) ve::log::warn("Failed to add test video segment 1");
    if(!video_track->add_segment(video_seg2)) ve::log::warn("Failed to add test video segment 2");
    if(!audio_track->add_segment(audio_seg1)) ve::log::warn("Failed to add test audio segment 1");
        
        // Refresh the timeline display
        if (timeline_panel_) {
            timeline_panel_->refresh();
        }
        
        ve::log::info("Created test timeline content with sample video and audio clips");
    }
}

void MainWindow::add_media_browser_placeholder() {
    if (!media_browser_) return;
    
    // Add a helpful placeholder item when no media is loaded
    auto* placeholder = new QTreeWidgetItem(media_browser_);
    placeholder->setText(0, "No media files imported yet");
    placeholder->setText(1, "Use File → Import Media");
    placeholder->setText(2, "to add content");
    placeholder->setText(3, "");
    
    // Style it differently to show it's not real media
    QFont italic_font = placeholder->font(0);
    italic_font.setItalic(true);
    for (int i = 0; i < 4; ++i) {
        placeholder->setFont(i, italic_font);
        placeholder->setForeground(i, QColor(128, 128, 128)); // Gray text
    }
    
    // Store identifier so we can remove it later
    placeholder->setData(0, Qt::UserRole, "PLACEHOLDER");
    
    // Disable interactions with placeholder
    placeholder->setFlags(placeholder->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEnabled);
}

void MainWindow::remove_media_browser_placeholder() {
    if (!media_browser_) return;
    
    // Find and remove placeholder items
    for (int i = media_browser_->topLevelItemCount() - 1; i >= 0; --i) {
        QTreeWidgetItem* item = media_browser_->topLevelItem(i);
        if (item && item->data(0, Qt::UserRole).toString() == "PLACEHOLDER") {
            delete media_browser_->takeTopLevelItem(i);
        }
    }
}

void MainWindow::flushTimelineBatch() {
    SCOPE("flushTimelineBatch");
    
    QElapsedTimer budget; 
    budget.start();
    const qint64 budgetMs = 12;   // Keep UI responsive - max 12ms per batch
    int processed = 0;
    
    while (!timeline_update_queue_.isEmpty()) {
        if (budget.elapsed() > budgetMs && processed > 0) {
            // Time budget exceeded, schedule next batch
            timeline_update_pump_->start(0);
            return;
        }
        
        TimelineInfo info = timeline_update_queue_.dequeue();
        
        // Process one timeline update item
        if (!timeline_) {
            QMessageBox::warning(this, "Add to Timeline Failed", "No timeline available. Please create a new project first.");
            continue;
        }
        
        // Build media source / clip (single clip representing both streams)
        auto media_source = std::make_shared<ve::timeline::MediaSource>();
        media_source->path = info.filePath.toStdString();
        media_source->duration = ve::TimeDuration{info.probeResult.duration_us, 1000000};
        media_source->format_name = info.probeResult.format;
        
        for (const auto& stream : info.probeResult.streams) {
            if (stream.type == "video") {
                media_source->width = stream.width;
                media_source->height = stream.height;
                if (stream.fps > 0) {
                    media_source->frame_rate = ve::TimeRational{static_cast<int64_t>(stream.fps * 1000), 1000};
                }
            } else if (stream.type == "audio") {
                media_source->sample_rate = stream.sample_rate;
                media_source->channels = stream.channels;
            }
        }

        std::string clip_name = QFileInfo(info.filePath).baseName().toStdString();
        auto clip_id = timeline_->add_clip(media_source, clip_name);

        // Ensure target track exists (expand as needed)
        while (static_cast<int>(timeline_->tracks().size()) <= info.track_index) {
            ve::timeline::Track::Type track_type = info.has_video ? ve::timeline::Track::Video : ve::timeline::Track::Audio;
            std::string auto_name = (track_type == ve::timeline::Track::Video ? "Video " : "Audio ") + std::to_string(timeline_->tracks().size() + 1);
            // Track added to satisfy placement requirement; id retained for potential logging
            const auto new_track_id = timeline_->add_track(track_type, auto_name);
            (void)new_track_id; // silence unused for now (may log later)
        }

        // Create segment referencing clip
        ve::timeline::Segment segment;
        segment.clip_id = clip_id;
        segment.start_time = info.start_time;
        segment.duration = media_source->duration;
        segment.name = clip_name;

        auto* track = timeline_->tracks()[info.track_index].get();
        if(track && track->add_segment(segment)) {
            ve::log::info("Added segment (clip) to track index " + std::to_string(info.track_index));
        } else {
            ve::log::warn("Failed to add segment to track index " + std::to_string(info.track_index));
        }
        
        processed++;
        
        // Check budget every few items
        if (processed % 4 == 0 && budget.elapsed() > budgetMs) {
            break;
        }
    }
    
    // Update UI after batch completion
    if (timeline_panel_) {
        timeline_panel_->set_timeline(timeline_);
        
        // Light UI update - defer heavy painting to next frame
        QTimer::singleShot(0, this, [this]() {
            SCOPE("deferred timeline UI update");
            timeline_panel_->update();
            status_label_->setText("Media added to timeline successfully!");
            status_label_->setStyleSheet("color: green;");
            ve::log::info("Timeline UI updated successfully");
        });
    }
    
    // Continue processing remaining items if any
    if (!timeline_update_queue_.isEmpty()) {
        timeline_update_pump_->start(0);
    }
    
    ve::log::info("Processed " + std::to_string(processed) + " timeline updates in " + std::to_string(budget.elapsed()) + "ms");
}

// End of MainWindow implementation
} // namespace ve::ui

#include "main_window.moc"
