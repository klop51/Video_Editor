# Professional Video Editor - Build Guide

> Complete step-by-step guide for building a professional video editor from the current foundation

## 📋 Table of Contents
- [Prerequisites](#prerequisites)
- [Current Status](#current-status)
- [Development Phases](#development-phases)
- [Phase-by-Phase Implementation](#phase-by-phase-implementation)
- [Testing Strategy](#testing-strategy)
- [Performance Targets](#performance-targets)
- [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Development Environment
- **OS**: Windows 10/11 (primary), macOS/Linux (future)
- **Compiler**: MSVC 2022 (17.0+) or Clang 15+
- **CMake**: 3.25 or higher
- **vcpkg**: Latest version
- **GPU**: DirectX 12 compatible or Vulkan 1.2+

### Required Libraries (via vcpkg)
```json
{
  "dependencies": [
    "ffmpeg",
    "fmt", 
    "spdlog",
    "catch2",
    "qt6-base",
    "qt6-multimedia", 
    "opencolorio",
    "glfw3",
    "vulkan"
  ]
}
```

### Hardware Requirements
- **CPU**: 8+ cores recommended (Intel i7/AMD Ryzen 7)
- **RAM**: 16GB minimum, 32GB recommended
- **GPU**: GTX 1060/RX 580 minimum, RTX 3060/RX 6600 XT recommended
- **Storage**: SSD for project files, additional HDD for media cache

---

## Current Status

### ✅ Completed (Foundation - 5%)
- [x] CMake build system with vcpkg integration
- [x] Core utilities (logging, time, profiling)
- [x] Basic media probing functionality
- [x] Project structure and documentation
- [x] Cross-platform build configuration

### 🔄 Next Immediate Steps
1. Complete media decode pipeline
2. Implement basic playback
3. Create timeline data structures
4. Build Qt UI foundation

---

## Development Phases

### Phase 1: Media Engine (Weeks 1-4)
**Goal**: Robust media decode and basic playback
**Files to create/modify**:
- `src/media_io/src/demuxer.cpp`
- `src/decode/src/video_decoder.cpp`
- `src/decode/src/audio_decoder.cpp`
- `src/playback/` (new module)

### Phase 2: Timeline Core (Weeks 5-8)
**Goal**: Complete timeline model with undo/redo
**Files to create**:
- `src/timeline/` (new module)
- `src/commands/` (new module)

### Phase 3: UI Foundation (Weeks 9-12)
**Goal**: Qt-based interface with basic panels
**Files to create**:
- `src/ui/` (new module)
- `src/app/` (new module)

### Phase 4: GPU Rendering (Weeks 13-16)
**Goal**: Hardware-accelerated playback and effects
**Files to create**:
- `src/render/` (new module)
- `src/gpu/` (new module)

---

## Phase-by-Phase Implementation

### 🎬 PHASE 1: MEDIA ENGINE

#### Step 1.1: Enhanced Demuxer
Create `src/media_io/include/media_io/demuxer.hpp`:
```cpp
#pragma once
#include "core/time.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ve::media_io {

struct StreamInfo {
    int index;
    enum Type { Video, Audio, Subtitle } type;
    std::string codec_name;
    // Video specific
    int width = 0, height = 0;
    ve::TimeRational frame_rate;
    // Audio specific  
    int sample_rate = 0, channels = 0;
};

class Demuxer {
public:
    static std::unique_ptr<Demuxer> create(const std::string& path);
    
    bool open(const std::string& path);
    std::vector<StreamInfo> get_streams() const;
    bool seek(ve::TimePoint timestamp);
    
    // Packet reading
    struct Packet {
        int stream_index;
        ve::TimePoint pts, dts;
        std::vector<uint8_t> data;
        bool is_keyframe;
    };
    
    bool read_packet(Packet& packet);
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ve::media_io
```

#### Step 1.2: Video Decoder Implementation
Create `src/decode/include/decode/video_decoder.hpp`:
```cpp
#pragma once
#include "media_io/demuxer.hpp"
#include "core/time.hpp"
#include <memory>

namespace ve::decode {

struct VideoFrame {
    int width, height;
    enum Format { RGBA8, YUV420P, NV12 } format;
    std::vector<uint8_t> data;
    ve::TimePoint pts;
    size_t stride;
};

class VideoDecoder {
public:
    static std::unique_ptr<VideoDecoder> create(const media_io::StreamInfo& stream);
    
    bool configure(const media_io::StreamInfo& stream);
    bool decode(const media_io::Demuxer::Packet& packet, VideoFrame& frame);
    void flush();
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ve::decode
```

#### Step 1.3: Basic Playback Controller
Create `src/playback/include/playback/controller.hpp`:
```cpp
#pragma once
#include "decode/video_decoder.hpp"
#include "core/time.hpp"
#include <functional>
#include <thread>
#include <atomic>

namespace ve::playback {

class Controller {
public:
    using FrameCallback = std::function<void(const decode::VideoFrame&)>;
    
    Controller();
    ~Controller();
    
    bool load_media(const std::string& path);
    void set_frame_callback(FrameCallback callback);
    
    void play();
    void pause();
    void stop();
    void seek(ve::TimePoint time);
    
    bool is_playing() const { return is_playing_.load(); }
    ve::TimePoint current_time() const { return current_time_.load(); }
    
private:
    void playback_thread();
    
    std::unique_ptr<media_io::Demuxer> demuxer_;
    std::unique_ptr<decode::VideoDecoder> video_decoder_;
    FrameCallback frame_callback_;
    
    std::thread playback_thread_;
    std::atomic<bool> is_playing_{false};
    std::atomic<bool> should_stop_{false};
    std::atomic<ve::TimePoint> current_time_{0};
};

} // namespace ve::playback
```

**Implementation Tasks for Phase 1**:
1. Implement FFmpeg-based demuxer using existing patterns
2. Create video decoder with software/hardware decode options
3. Add audio decoder following same pattern
4. Build playback controller with threading
5. Add frame queue and buffering
6. Create performance monitoring

---

### 🗂️ PHASE 2: TIMELINE CORE

#### Step 2.1: Timeline Data Structures
Create `src/timeline/include/timeline/project.hpp`:
```cpp
#pragma once
#include "core/time.hpp"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace ve::timeline {

using ClipId = uint64_t;
using TrackId = uint64_t;

struct MediaSource {
    std::string path;
    std::string hash;  // For relink detection
    ve::TimeDuration duration;
    // Metadata cached from probe
};

struct MediaClip {
    ClipId id;
    std::shared_ptr<MediaSource> source;
    ve::TimePoint in_time, out_time;  // Source timecode
    std::string name;
};

struct Segment {
    ClipId clip_id;
    ve::TimePoint start_time;  // Timeline position
    ve::TimeDuration duration;
    // Per-instance effect overrides
};

class Track {
public:
    enum Type { Video, Audio };
    
    Track(TrackId id, Type type) : id_(id), type_(type) {}
    
    TrackId id() const { return id_; }
    Type type() const { return type_; }
    
    // Edit operations
    bool insert_segment(const Segment& segment, ve::TimePoint at);
    bool delete_range(ve::TimePoint start, ve::TimeDuration duration);
    bool move_segment(size_t index, ve::TimePoint new_start);
    
    const std::vector<Segment>& segments() const { return segments_; }
    
private:
    TrackId id_;
    Type type_;
    std::vector<Segment> segments_;  // Always sorted by start_time
    
    void validate_no_overlap() const;
};

class Timeline {
public:
    Timeline();
    
    // Track management
    TrackId add_track(Track::Type type);
    void remove_track(TrackId id);
    Track* get_track(TrackId id);
    const std::vector<std::unique_ptr<Track>>& tracks() const { return tracks_; }
    
    // Clip management
    ClipId add_clip(std::shared_ptr<MediaSource> source, 
                    const std::string& name = "");
    MediaClip* get_clip(ClipId id);
    
    // Project properties
    ve::TimeRational frame_rate() const { return frame_rate_; }
    void set_frame_rate(ve::TimeRational rate) { frame_rate_ = rate; }
    
private:
    std::vector<std::unique_ptr<Track>> tracks_;
    std::unordered_map<ClipId, std::unique_ptr<MediaClip>> clips_;
    
    ve::TimeRational frame_rate_{30, 1};  // Default 30fps
    TrackId next_track_id_{1};
    ClipId next_clip_id_{1};
};

} // namespace ve::timeline
```

#### Step 2.2: Command System
Create `src/commands/include/commands/command.hpp`:
```cpp
#pragma once
#include "timeline/project.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ve::commands {

class Command {
public:
    virtual ~Command() = default;
    virtual bool execute(timeline::Timeline& timeline) = 0;
    virtual bool undo(timeline::Timeline& timeline) = 0;
    virtual std::string description() const = 0;
    
    // For command coalescing
    virtual bool can_merge_with(const Command& other) const { return false; }
    virtual std::unique_ptr<Command> merge_with(std::unique_ptr<Command> other) const { return nullptr; }
};

class CommandHistory {
public:
    CommandHistory(size_t max_history = 200);
    
    bool execute(std::unique_ptr<Command> command, timeline::Timeline& timeline);
    bool undo(timeline::Timeline& timeline);
    bool redo(timeline::Timeline& timeline);
    
    bool can_undo() const { return current_index_ > 0; }
    bool can_redo() const { return current_index_ < commands_.size(); }
    
    const std::vector<std::unique_ptr<Command>>& commands() const { return commands_; }
    size_t current_index() const { return current_index_; }
    
private:
    std::vector<std::unique_ptr<Command>> commands_;
    size_t current_index_{0};
    size_t max_history_;
    
    void trim_history();
    bool try_coalesce(std::unique_ptr<Command>& new_command);
};

// Specific command implementations
class InsertSegmentCommand : public Command {
public:
    InsertSegmentCommand(timeline::TrackId track_id, 
                        timeline::Segment segment,
                        ve::TimePoint at);
    
    bool execute(timeline::Timeline& timeline) override;
    bool undo(timeline::Timeline& timeline) override;
    std::string description() const override;
    
private:
    timeline::TrackId track_id_;
    timeline::Segment segment_;
    ve::TimePoint position_;
    // Store undo info
};

} // namespace ve::commands
```

**Implementation Tasks for Phase 2**:
1. Implement timeline data structures with validation
2. Create command system with undo/redo
3. Add specific edit commands (insert, delete, move, blade)
4. Implement time utilities and conversion functions
5. Add serialization for project files (JSON initially)
6. Create comprehensive unit tests

---

### 🖥️ PHASE 3: UI FOUNDATION

#### Step 3.1: Application Framework
Create `src/app/include/app/application.hpp`:
```cpp
#pragma once
#include "timeline/project.hpp"
#include "commands/command.hpp"
#include "playback/controller.hpp"
#include <QApplication>
#include <memory>

namespace ve::app {

class Application : public QApplication {
    Q_OBJECT
    
public:
    Application(int argc, char** argv);
    ~Application();
    
    static Application* instance();
    
    // Project management
    bool new_project();
    bool open_project(const QString& path);
    bool save_project(const QString& path = "");
    void close_project();
    
    timeline::Timeline* current_timeline() { return timeline_.get(); }
    commands::CommandHistory* command_history() { return command_history_.get(); }
    playback::Controller* playback_controller() { return playback_controller_.get(); }
    
public slots:
    void execute_command(std::unique_ptr<commands::Command> command);
    void undo();
    void redo();
    
signals:
    void timeline_changed();
    void playback_time_changed(ve::TimePoint time);
    
private:
    static Application* instance_;
    
    std::unique_ptr<timeline::Timeline> timeline_;
    std::unique_ptr<commands::CommandHistory> command_history_;
    std::unique_ptr<playback::Controller> playback_controller_;
    
    QString current_project_path_;
    bool project_modified_{false};
};

} // namespace ve::app
```

#### Step 3.2: Main Window
Create `src/ui/include/ui/main_window.hpp`:
```cpp
#pragma once
#include <QMainWindow>
#include <QDockWidget>
#include <memory>

QT_BEGIN_NAMESPACE
class QMenuBar;
class QToolBar;
class QStatusBar;
QT_END_NAMESPACE

namespace ve::ui {

class TimelinePanel;
class ViewerPanel;
class MediaBrowser;
class EffectControls;

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    
protected:
    void closeEvent(QCloseEvent* event) override;
    
private slots:
    void new_project();
    void open_project();
    void save_project();
    void save_project_as();
    void import_media();
    void export_timeline();
    void undo();
    void redo();
    void about();
    
private:
    void create_menus();
    void create_toolbars();
    void create_status_bar();
    void create_dock_widgets();
    void setup_layout();
    
    // Panels
    TimelinePanel* timeline_panel_;
    ViewerPanel* viewer_panel_;
    MediaBrowser* media_browser_;
    EffectControls* effect_controls_;
    
    // Dock widgets
    QDockWidget* timeline_dock_;
    QDockWidget* media_browser_dock_;
    QDockWidget* effect_controls_dock_;
    
    // Actions
    QAction* new_action_;
    QAction* open_action_;
    QAction* save_action_;
    QAction* save_as_action_;
    QAction* undo_action_;
    QAction* redo_action_;
    // ... more actions
};

} // namespace ve::ui
```

#### Step 3.3: Timeline Panel
Create `src/ui/include/ui/timeline_panel.hpp`:
```cpp
#pragma once
#include "timeline/project.hpp"
#include "core/time.hpp"
#include <QWidget>
#include <QScrollArea>
#include <QGraphicsView>

namespace ve::ui {

class TimelinePanel : public QWidget {
    Q_OBJECT
    
public:
    explicit TimelinePanel(QWidget* parent = nullptr);
    
    void set_timeline(timeline::Timeline* timeline);
    void set_zoom(double zoom_factor);
    void set_current_time(ve::TimePoint time);
    
public slots:
    void refresh();
    
signals:
    void time_changed(ve::TimePoint time);
    void selection_changed();
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    
private:
    timeline::Timeline* timeline_;
    ve::TimePoint current_time_;
    double zoom_factor_{1.0};
    
    // Visual parameters
    static constexpr int TRACK_HEIGHT = 60;
    static constexpr int TIMECODE_HEIGHT = 30;
    static constexpr int PIXELS_PER_SECOND = 100;
    
    // Drawing helpers
    void draw_timecode_ruler(QPainter& painter);
    void draw_tracks(QPainter& painter);
    void draw_segments(QPainter& painter, const timeline::Track& track, int track_y);
    void draw_playhead(QPainter& painter);
    
    // Coordinate conversion
    ve::TimePoint pixel_to_time(int x) const;
    int time_to_pixel(ve::TimePoint time) const;
    int track_at_y(int y) const;
};

} // namespace ve::ui
```

**Implementation Tasks for Phase 3**:
1. Set up Qt application framework
2. Create main window with dockable panels
3. Implement timeline visualization
4. Add basic viewer panel for playback
5. Create media browser for asset management
6. Implement keyboard shortcuts and menu system
7. Add status bar and progress indicators

---

### 🎮 PHASE 4: GPU RENDERING

#### Step 4.1: Graphics API Abstraction
Create `src/render/include/render/gpu_context.hpp`:
```cpp
#pragma once
#include <memory>
#include <vector>
#include <string>

namespace ve::render {

struct Texture {
    uint32_t id;
    int width, height;
    enum Format { RGBA8, RGBA16F, RGBA32F } format;
};

struct Shader {
    uint32_t id;
    std::string name;
};

class GpuContext {
public:
    static std::unique_ptr<GpuContext> create();
    virtual ~GpuContext() = default;
    
    // Resource management
    virtual Texture create_texture(int width, int height, Texture::Format format) = 0;
    virtual void destroy_texture(const Texture& texture) = 0;
    virtual void upload_texture_data(const Texture& texture, const void* data) = 0;
    
    virtual Shader create_shader(const std::string& vertex_src, 
                                const std::string& fragment_src) = 0;
    virtual void destroy_shader(const Shader& shader) = 0;
    
    // Rendering
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual void dispatch_compute(const Shader& shader, int groups_x, int groups_y) = 0;
    virtual void draw_quad(const Shader& shader, const std::vector<Texture>& inputs) = 0;
    
    // Debug
    virtual std::string get_info() const = 0;
    
protected:
    GpuContext() = default;
};

// Platform-specific implementations
class VulkanContext : public GpuContext {
    // Vulkan implementation
};

class D3D12Context : public GpuContext {
    // DirectX 12 implementation  
};

} // namespace ve::render
```

#### Step 4.2: Render Graph
Create `src/render/include/render/render_graph.hpp`:
```cpp
#pragma once
#include "gpu_context.hpp"
#include "decode/video_decoder.hpp"
#include <functional>
#include <unordered_map>

namespace ve::render {

using NodeId = uint64_t;
using ParameterHash = uint64_t;

struct RenderContext {
    GpuContext* gpu_context;
    ve::TimePoint current_time;
    int output_width, output_height;
};

class RenderNode {
public:
    virtual ~RenderNode() = default;
    virtual Texture render(const RenderContext& context) = 0;
    virtual ParameterHash get_hash() const = 0;
    virtual std::string get_name() const = 0;
    
protected:
    std::vector<NodeId> input_nodes_;
};

class RenderGraph {
public:
    RenderGraph();
    
    NodeId add_node(std::unique_ptr<RenderNode> node);
    void connect_nodes(NodeId output_node, NodeId input_node);
    void remove_node(NodeId node_id);
    
    Texture render_frame(const RenderContext& context, NodeId output_node);
    void clear_cache();
    
private:
    std::unordered_map<NodeId, std::unique_ptr<RenderNode>> nodes_;
    std::unordered_map<ParameterHash, Texture> memoization_cache_;
    NodeId next_node_id_{1};
    
    bool has_cycle(NodeId start_node) const;
    std::vector<NodeId> topological_sort(NodeId output_node) const;
};

// Built-in render nodes
class VideoSourceNode : public RenderNode {
public:
    VideoSourceNode(timeline::ClipId clip_id);
    Texture render(const RenderContext& context) override;
    ParameterHash get_hash() const override;
    std::string get_name() const override;
    
private:
    timeline::ClipId clip_id_;
    std::unique_ptr<decode::VideoDecoder> decoder_;
};

class BrightnessNode : public RenderNode {
public:
    BrightnessNode(float brightness);
    void set_brightness(float brightness);
    
    Texture render(const RenderContext& context) override;
    ParameterHash get_hash() const override;
    std::string get_name() const override;
    
private:
    float brightness_;
    Shader brightness_shader_;
};

} // namespace ve::render
```

**Implementation Tasks for Phase 4**:
1. Choose primary graphics API (Vulkan recommended)
2. Implement GPU context abstraction
3. Create render graph system with memoization
4. Add basic shader compilation and management
5. Implement core render nodes (source, transform, color)
6. Add GPU memory management
7. Integrate with playback system

---

## Testing Strategy

### Unit Tests Structure
```
tests/
├── core/
│   ├── test_time.cpp
│   ├── test_logging.cpp
│   └── test_profiling.cpp
├── media_io/
│   ├── test_demuxer.cpp
│   └── test_media_probe.cpp
├── decode/
│   ├── test_video_decoder.cpp
│   └── test_audio_decoder.cpp
├── timeline/
│   ├── test_project.cpp
│   ├── test_track.cpp
│   └── test_segments.cpp
├── commands/
│   ├── test_command_history.cpp
│   └── test_edit_commands.cpp
├── render/
│   ├── test_render_graph.cpp
│   └── test_gpu_context.cpp
└── integration/
    ├── test_playback.cpp
    ├── test_editing_workflow.cpp
    └── test_export.cpp
```

### Performance Benchmarks
Create `tests/benchmarks/benchmark_playback.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "playback/controller.hpp"

TEST_CASE("Playback Performance", "[benchmark]") {
    BENCHMARK("4K Frame Decode") {
        // Measure 4K frame decode time
        return decode_4k_frame();
    };
    
    BENCHMARK("Timeline Edit Operations") {
        // Measure edit operation latency
        return perform_edit_sequence();
    };
}
```

### Golden Image Tests
Create `tests/golden/` directory with reference frames and comparison utilities.

---

## Performance Targets

### Playback Performance
| Metric | Target | Measurement |
|--------|---------|-------------|
| 4K30 playback | ≥28 fps avg | Frame render time |
| 4K60 playback | ≥50 fps avg | Frame render time |
| Scrub latency | <120ms | Seek to first frame |
| Edit operation | <5ms p95 | Command execution time |

### Memory Usage
| Metric | Target | Measurement |
|--------|---------|-------------|
| Base memory | <500MB | RSS without media |
| Frame cache | 3x working set | Adaptive based on available RAM |
| GPU memory | <2GB | Texture + buffer allocation |

### Export Performance
| Metric | Target | Measurement |
|--------|---------|-------------|
| 4K H.264 export | ≥0.7x realtime | Encoding speed |
| 1080p H.264 export | ≥2x realtime | Encoding speed |

---

## Implementation Checklist by Phase

### Phase 1: Media Engine ✅ Tasks
- [ ] Set up FFmpeg integration properly
- [ ] Implement robust demuxer with error handling
- [ ] Create video decoder with hardware fallback
- [ ] Add audio decoder and resampling
- [ ] Build playback controller with threading
- [ ] Add frame queue and buffering system
- [ ] Implement basic caching strategy
- [ ] Create performance monitoring
- [ ] Add media format validation
- [ ] Write comprehensive unit tests

### Phase 2: Timeline Core 📋 Tasks
- [ ] Design and implement timeline data structures
- [ ] Create command pattern with undo/redo
- [ ] Implement all basic edit operations
- [ ] Add time conversion utilities
- [ ] Create project serialization (JSON)
- [ ] Add comprehensive validation
- [ ] Implement command coalescing
- [ ] Write edit operation tests
- [ ] Add stress tests for large timelines
- [ ] Create timeline integrity validation

### Phase 3: UI Foundation 🖥️ Tasks
- [ ] Set up Qt application framework
- [ ] Create main window with docking
- [ ] Implement timeline panel rendering
- [ ] Add basic viewer panel
- [ ] Create media browser functionality
- [ ] Implement keyboard shortcuts
- [ ] Add menu and toolbar system
- [ ] Create status bar and progress
- [ ] Implement drag-and-drop
- [ ] Add basic preferences system

### Phase 4: GPU Rendering 🎮 Tasks
- [ ] Choose and implement graphics API
- [ ] Create GPU context abstraction
- [ ] Build render graph system
- [ ] Implement shader compilation
- [ ] Add basic render nodes
- [ ] Create memoization system
- [ ] Implement GPU memory management
- [ ] Add debug visualization
- [ ] Integrate with playback
- [ ] Optimize performance

---

## Troubleshooting

### Common Build Issues

#### vcpkg Dependencies
```bash
# Clear vcpkg cache if dependencies fail
rm -rf build/vcpkg_installed
cmake --preset dev-debug

# Check vcpkg installation
vcpkg list
```

#### FFmpeg Integration
```cpp
// Ensure proper FFmpeg initialization
av_register_all();  // For older FFmpeg versions
avformat_network_init();
```

#### Qt Integration
```cmake
# In CMakeLists.txt
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia)
target_link_libraries(your_target Qt6::Core Qt6::Widgets Qt6::Multimedia)
```

### Performance Issues

#### Frame Drops
- Check decode thread priority
- Verify GPU memory usage
- Monitor frame cache hit ratio
- Profile CPU usage in decode path

#### Memory Leaks
- Use smart pointers consistently
- Verify GPU resource cleanup
- Check FFmpeg resource management
- Monitor cache eviction

### Debugging Tools

#### Logging Configuration
```cpp
// Enable debug logging
ve::log::set_json_mode(false);
spdlog::set_level(spdlog::level::debug);
```

#### Performance Profiling
```cpp
// Use built-in profiling
VE_PROFILE_SCOPE("decode_frame");
auto timer = ve::profiling::ScopedTimer("operation_name");
```

---

## Next Steps

1. **Start with Phase 1**: Focus on getting robust media decode working
2. **Set up testing early**: Create unit tests as you build each component
3. **Profile frequently**: Measure performance against targets regularly
4. **Document decisions**: Keep DECISIONS.md updated with architectural choices
5. **Iterate quickly**: Build minimal working versions first, optimize later

Remember: This is a complex project that will take 12-18 months for a full professional editor. Focus on getting each phase working well before moving to the next one.

---

**Happy Building! 🚀**
