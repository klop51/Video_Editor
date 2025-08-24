#pragma once
#include "timeline/timeline.hpp"
#include "playback/controller.hpp"
#include <QApplication>
#include <memory>

namespace ve::ui { class MainWindow; }

namespace ve::app {

class Application : public QApplication {
    Q_OBJECT
    
public:
    Application(int argc, char** argv);
    ~Application();
    
    static Application* instance();
    
    // Application lifecycle
    int run();
    
    // Project management
    bool new_project();
    bool open_project(const QString& path);
    bool save_project(const QString& path = "");
    void close_project();
    
    // Core components access
    ve::timeline::Timeline* timeline() { return timeline_.get(); }
    ve::playback::Controller* playback_controller() { return playback_controller_.get(); }
    
signals:
    void project_changed();
    void playback_time_changed(int64_t time_us);
    
private slots:
    void on_project_modified();
    
private:
    void create_main_window();
    void setup_connections();
    
    static Application* instance_;
    
    std::unique_ptr<ve::timeline::Timeline> timeline_;
    std::unique_ptr<ve::playback::Controller> playback_controller_;
    std::unique_ptr<ve::ui::MainWindow> main_window_;
    
    QString current_project_path_;
    bool project_modified_;
};

} // namespace ve::app
