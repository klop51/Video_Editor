#include "app/application.hpp"
#include "ui/main_window.hpp"
#include "core/log.hpp"
#include <QDir>
#include <QStandardPaths>

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
    
    ve::log::info("Application starting");
    
    // Create core components
    timeline_ = std::make_unique<ve::timeline::Timeline>();
    playback_controller_ = std::make_unique<ve::playback::Controller>();
    
    create_main_window();
    setup_connections();
}

Application::~Application() {
    ve::log::info("Application shutting down");
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
    
    main_window_->show();
    ve::log::info("Application started successfully");
    
    return exec();
}

bool Application::new_project() {
    close_project();
    
    timeline_ = std::make_unique<ve::timeline::Timeline>();
    timeline_->set_name("Untitled Project");
    
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
    // TODO: Implement project file loading
    ve::log::info("Open project requested: " + path.toStdString());
    
    // For now, just create a new project
    if (new_project()) {
        current_project_path_ = path;
        return true;
    }
    
    return false;
}

bool Application::save_project(const QString& path) {
    QString save_path = path.isEmpty() ? current_project_path_ : path;
    
    if (save_path.isEmpty()) {
        ve::log::warn("No save path specified");
        return false;
    }
    
    // TODO: Implement project file saving
    ve::log::info("Save project requested: " + save_path.toStdString());
    
    current_project_path_ = save_path;
    project_modified_ = false;
    
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
}

void Application::create_main_window() {
    main_window_ = std::make_unique<ve::ui::MainWindow>();
    main_window_->set_timeline(timeline_.get());
    main_window_->set_playback_controller(playback_controller_.get());
}

void Application::setup_connections() {
    // TODO: Set up signal connections between components
    // For example:
    // connect(timeline_.get(), &Timeline::modified, this, &Application::on_project_modified);
}

} // namespace ve::app

#include "application.moc"
