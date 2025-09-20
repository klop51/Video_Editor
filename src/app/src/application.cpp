#include "app/application.hpp"
#include "ui/main_window.hpp"
#include "core/log.hpp"
#include "core/profiling.hpp"
#include "persistence/project_serializer.hpp"
#include <QDir>
#include <QStandardPaths>
#include <QTimer>

namespace ve::app {

Application* Application::instance_ = nullptr;

Application::Application(int argc, char** argv)
    : QApplication(argc, argv)
    , project_modified_(false)
{
    instance_ = this;
    
    setApplicationName("Video Editor");
    setApplicationVersion("0.1.0");
    setOrganizationName("Video Editor Team");
    setOrganizationDomain("videoeditor.dev");
    
    // ve::log::info("Application starting"); // Temporarily disabled to test
    
    // Create core components
    timeline_ = std::make_unique<ve::timeline::Timeline>();
    playback_controller_ = std::make_unique<ve::playback::PlaybackController>();
    
    create_main_window();
    setup_connections();
}

Application::~Application() {
    ve::log::info("Application shutting down");
    // Emit profiling summary (best-effort)
    ve::prof::Accumulator::instance().write_json("profiling.json");
    instance_ = nullptr;
}

Application* Application::instance() {
    return instance_;
}

int Application::run() {
    if (!main_window_) {
        ve::log::error("Main window not created");
        return -1;
    }
    
    ve::log::info("Showing main window...");
    main_window_->show();
    
    // Schedule a mid-run profiling snapshot after a short delay (e.g., 5s)
    QTimer::singleShot(5000, this, [](){
        ve::log::info("Writing mid-run profiling snapshot: profiling_runtime.json");
        ve::prof::Accumulator::instance().write_json("profiling_runtime.json");
    });
    
    ve::log::info("Entering Qt event loop...");
    int result = exec();
    ve::log::info("Qt event loop exited with code: " + std::to_string(result));
    
    return result;
}

bool Application::new_project() {
    close_project();
    
    timeline_ = std::make_unique<ve::timeline::Timeline>();
    timeline_->set_name("Untitled Project");
    timeline_->set_modified_callback([this]{ on_project_modified(); });
    
    if (main_window_) {
        main_window_->set_timeline(timeline_.get());
    }
    
    current_project_path_.clear();
    project_modified_ = false;
    
    emit project_changed();
    ve::log::info("New project created");
    
    return true;
}

bool Application::open_project(const QString& path) {
    ve::log::info("Open project requested: " + path.toStdString());
    if(path.isEmpty()){
        ve::log::warn("Open project called with empty path");
        return false;
    }
    // Reset current timeline
    timeline_ = std::make_unique<ve::timeline::Timeline>();
    timeline_->set_modified_callback([this]{ on_project_modified(); });
    auto res = ve::persistence::load_timeline_json(*timeline_, path.toStdString());
    if(!res.success){
        ve::log::error(std::string("Failed to load project: ") + res.error);
        // Revert to clean new project to keep app usable
        timeline_ = std::make_unique<ve::timeline::Timeline>();
        if (main_window_) main_window_->set_timeline(timeline_.get());
        return false;
    }
    if (main_window_) {
        main_window_->set_timeline(timeline_.get());
    }
    current_project_path_ = path;
    project_modified_ = false;
    emit project_changed();
    ve::log::info("Project loaded successfully");
    return true;
}

bool Application::save_project(const QString& path) {
    QString save_path = path.isEmpty() ? current_project_path_ : path;
    
    if (save_path.isEmpty()) {
        ve::log::warn("No save path specified");
        return false;
    }
    
    ve::log::info("Save project requested: " + save_path.toStdString());
    if(!timeline_){
        ve::log::warn("No timeline to save");
        return false;
    }
    auto res = ve::persistence::save_timeline_json(*timeline_, save_path.toStdString());
    if(!res.success){
        ve::log::error(std::string("Failed to save project: ") + res.error);
        return false;
    }
    current_project_path_ = save_path;
    project_modified_ = false;
    emit project_changed();
    ve::log::info("Project saved successfully");
    return true;
}

void Application::close_project() {
    if (timeline_) {
        timeline_.reset();
    }
    
    if (playback_controller_) {
        playback_controller_->stop();
        playback_controller_->close_media();
    }
    
    current_project_path_.clear();
    project_modified_ = false;
    
    emit project_changed();
    ve::log::info("Project closed");
}

void Application::on_project_modified() {
    project_modified_ = true;
    emit project_modified();
}

void Application::create_main_window() {
    ve::log::info("Creating main window...");
    main_window_ = std::make_unique<ve::ui::MainWindow>();
    ve::log::info("Main window object created");
    
    main_window_->set_timeline(timeline_.get());
    ve::log::info("Timeline set on main window");
    
    main_window_->set_playback_controller(playback_controller_.get());
    ve::log::info("Playback controller set on main window");
    
    ve::log::info("Main window created");
}

void Application::setup_connections() {
    if(timeline_) {
        timeline_->set_modified_callback([this]{ on_project_modified(); });
    }
}

} // namespace ve::app

// #include "application.moc"  // Temporarily removed due to AutoMoc warning
