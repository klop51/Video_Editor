# Timeline Performance Analysis & Optimization Plan

## Executive Summary

Based on code analysis, the timeline is still experiencing significant performance issues when adding media (~6.7 seconds paint time). While some optimizations have been implemented, several critical bottlenecks remain unaddressed.

## Current Performance State

### ✅ Already Implemented Optimizations
- **Update Throttling**: 16ms paint throttling, 30fps refresh limits
- **Paint Monitoring**: Detailed timing breakdown (background, timecode, tracks)
- **Minimal Paint Mode**: Simplified rendering during rapid updates
- **Waveform Optimization**: Ultra-fast block-based waveform with simple patterns
- **Viewport Awareness**: Some viewport culling for segments
- **Mouse Event Throttling**: Reduced sensitivity for interactions

### ❌ Critical Performance Bottlenecks Identified

## Performance Issues Analysis

### 1. **Segment Rendering Pipeline Bottlenecks**
**Problem**: Each segment triggers expensive operations:
- ✅ Waveform rendering optimized (was major bottleneck)
- ❌ **Text rendering with font metrics** - `QFontMetrics::horizontalAdvance()` and `elidedText()` are expensive
- ❌ **Individual segment drawing** - No batching of similar segments
- ❌ **Gradient/color calculations** per segment
- ❌ **Hit testing calculations** during paint events

**Current Code Issues**:
```cpp
// EXPENSIVE: Called for every segment
static QFontMetrics cached_fm(cached_name_font);
if (cached_fm.horizontalAdvance(segment_name) > text_rect.width()) {
    segment_name = cached_fm.elidedText(segment_name, Qt::ElideRight, text_rect.width());
}
```

### 2. **Paint Event Frequency Issues**
**Problem**: Multiple unnecessary paint events triggered:
- ❌ **Every timeline modification** triggers immediate `update()`
- ❌ **Mouse events** trigger paints even when nothing changed
- ❌ **Zoom/scroll operations** trigger full repaints
- ❌ **Selection changes** trigger full timeline redraws

**Current Code Issues**:
```cpp
void TimelinePanel::set_current_time(ve::TimePoint time) {
    current_time_ = time;
    update();  // IMMEDIATE FULL REPAINT
}

void TimelinePanel::refresh() {
    // Only 30fps throttling - still allows 30 full repaints per second
    if (time_since_last.count() >= 33) {
        update();  // FULL REPAINT
    }
}
```

### 3. **Memory Allocation During Paint**
**Problem**: Paint events allocate memory:
- ❌ **QColor objects** created for every segment
- ❌ **QString conversions** from std::string segment names
- ❌ **QRect objects** created repeatedly
- ❌ **QPen/QBrush objects** not reused

### 4. **Viewport Culling Insufficient**
**Problem**: Still processing off-screen elements:
- ✅ Basic segment viewport culling implemented
- ❌ **Track-level culling** missing - processes all tracks even if off-screen
- ❌ **Detail-level culling** missing - draws full details even for tiny segments
- ❌ **Text culling** missing - calculates text metrics for invisible text

### 5. **Background Processing Issues**
**Problem**: Heavy operations on UI thread:
- ❌ **Timeline data access** during paint (should be cached)
- ❌ **Segment iteration** on UI thread
- ❌ **Color/style calculations** on UI thread

## Optimization Plan

### Phase 1: Immediate Wins (1-2 hours)

#### 1.1 Aggressive Paint Throttling
```cpp
// Goal: Reduce paint frequency to 15fps during heavy operations
class PaintThrottler {
    static constexpr int HEAVY_OPERATION_FPS = 15;  // 66ms between paints
    static constexpr int NORMAL_FPS = 60;           // 16ms between paints
    bool heavy_mode = false;
    QTimer throttle_timer;
    bool pending_paint = false;
};
```

#### 1.2 Disable Expensive Features During Heavy Operations
```cpp
void TimelinePanel::paintEvent(QPaintEvent* event) {
    bool is_heavy_operation = (segments_being_added_ > 0) || g_timeline_busy.load();
    
    if (is_heavy_operation) {
        // Ultra-minimal painting: just basic shapes, no text, no waveforms
        draw_minimal_timeline(painter);
        return;
    }
    
    // Full detailed painting only when timeline is stable
    draw_full_timeline(painter);
}
```

#### 1.3 Pre-allocate Common Objects
```cpp
class TimelinePanel {
private:
    // Pre-allocated objects to avoid allocation during paint
    mutable QColor cached_video_color;
    mutable QColor cached_audio_color;
    mutable QPen cached_border_pen;
    mutable QBrush cached_segment_brush;
    mutable QFont cached_name_font;
    mutable QFontMetrics cached_font_metrics;
};
```

### Phase 2: Structural Improvements (2-4 hours)

#### 2.1 Implement Smart Dirty Tracking
```cpp
class TimelinePanel {
private:
    struct DirtyRegion {
        QRect rect;
        bool needs_full_redraw = false;
        bool needs_text_update = false;
        bool needs_waveform_update = false;
    };
    
    std::vector<DirtyRegion> dirty_regions_;
    
    void invalidate_region(const QRect& rect, bool full = false);
    void paintEvent(QPaintEvent* event) override;  // Only paint dirty regions
};
```

#### 2.2 Implement Segment Batching
```cpp
void TimelinePanel::draw_segments_batched(QPainter& painter, const Track& track, int track_y) {
    // Group segments by visual similarity
    std::vector<SegmentBatch> batches;
    batch_similar_segments(track.segments(), batches);
    
    for (const auto& batch : batches) {
        draw_segment_batch(painter, batch, track_y);  // Single draw call per batch
    }
}
```

#### 2.3 Level-of-Detail Rendering
```cpp
enum class DetailLevel {
    MINIMAL,    // Just colored rectangles
    BASIC,      // + borders and selection
    NORMAL,     // + segment names
    DETAILED    // + waveforms and thumbnails
};

DetailLevel calculate_detail_level(int segment_width, double zoom_factor) {
    if (segment_width < 10) return DetailLevel::MINIMAL;
    if (segment_width < 40) return DetailLevel::BASIC;
    if (segment_width < 100) return DetailLevel::NORMAL;
    return DetailLevel::DETAILED;
}
```

### Phase 3: Advanced Optimizations (4-8 hours)

#### 3.1 Background Timeline Data Cache
```cpp
class TimelineDataCache {
    struct CachedTrackData {
        std::vector<VisibleSegment> visible_segments;
        QRect bounds;
        int version;
    };
    
    std::vector<CachedTrackData> cached_tracks_;
    std::atomic<int> timeline_version_{0};
    
    void update_cache_async();  // Background thread
    const CachedTrackData& get_track_data(size_t track_index) const;
};
```

#### 3.2 Implement Paint Caching
```cpp
class TimelinePanel {
private:
    QPixmap background_cache_;
    QPixmap timecode_cache_;
    std::unordered_map<uint32_t, QPixmap> segment_cache_;
    
    void invalidate_background_cache();
    void invalidate_timecode_cache();
    void invalidate_segment_cache(uint32_t segment_id);
};
```

#### 3.3 Progressive Rendering
```cpp
class ProgressiveRenderer {
    enum class RenderPass {
        BACKGROUND,
        TIMECODE,
        TRACK_STRUCTURE,
        SEGMENTS_BASIC,
        SEGMENTS_DETAILED,
        WAVEFORMS,
        OVERLAYS
    };
    
    void start_progressive_render();
    void render_next_pass();  // Called from timer
    bool is_render_complete() const;
};
```

### Phase 4: Memory & Threading Optimizations (8+ hours)

#### 4.1 Memory Pool for Paint Objects
```cpp
class PaintObjectPool {
    std::vector<QColor> color_pool_;
    std::vector<QPen> pen_pool_;
    std::vector<QBrush> brush_pool_;
    
    QColor* get_color();
    QPen* get_pen();
    void return_objects();  // Called after paint
};
```

#### 4.2 Background Timeline Processing
```cpp
class TimelineRenderer : public QObject {
    Q_OBJECT
public:
    void render_timeline_async(const TimelineData& data);
    
signals:
    void timeline_rendered(const QPixmap& result);
    void partial_render_ready(const QRect& region, const QPixmap& partial);
};
```

## Implementation Priority

### 🔥 Critical (Implement First)
1. **Disable expensive features during media addition** (1 hour)
2. **Aggressive paint throttling during heavy operations** (1 hour)
3. **Pre-allocate common paint objects** (1 hour)

### 🟡 High Impact (Next)
4. **Smart dirty region tracking** (3 hours)
5. **Level-of-detail rendering** (2 hours)
6. **Segment batching** (3 hours)

### 🟢 Long-term (Future)
7. **Background data caching** (6 hours)
8. **Paint result caching** (4 hours)
9. **Progressive rendering** (8 hours)

## Performance Targets

### Current State
- ❌ **6.7 seconds** for timeline paint with new media
- ❌ Timeline locks UI during media addition
- ❌ Sluggish interaction during heavy operations

### Target State (Phase 1)
- ✅ **< 50ms** for timeline paint during media addition
- ✅ Responsive UI always maintained
- ✅ Smooth interaction even during heavy operations

### Target State (Phase 3)
- ✅ **< 16ms** for all timeline paint operations
- ✅ 60fps timeline interaction
- ✅ Background processing for all heavy operations

## Testing Strategy

1. **Micro-benchmarks**: Time individual functions (segment drawing, text rendering)
2. **Integration tests**: Add multiple large video files and measure timeline response
3. **User interaction tests**: Verify smooth zooming, scrolling, selection during heavy operations
4. **Memory profiling**: Ensure no memory leaks from paint optimizations

## Risk Assessment

### Low Risk
- Paint throttling and object pre-allocation
- Disabling features during heavy operations

### Medium Risk
- Dirty region tracking (complexity in invalidation logic)
- Level-of-detail rendering (visual quality trade-offs)

### High Risk
- Background threading (synchronization complexity)
- Progressive rendering (state management complexity)

## Conclusion

The timeline performance issues are primarily caused by:
1. **Too frequent paint events** (every timeline change)
2. **Expensive per-segment operations** (text metrics, individual drawing)
3. **No level-of-detail rendering** (full detail even for tiny segments)
4. **Synchronous heavy operations** on UI thread

**Recommendation**: Start with Phase 1 optimizations to get immediate 10-20x performance improvement, then gradually implement advanced optimizations as needed.