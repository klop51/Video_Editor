#include <catch2/catch_test_macros.hpp>

#ifdef VE_ENABLE_QT_UI_TESTS
#include <QApplication>
#include <QMenu>
#include <QPoint>
#include "ui/timeline_panel.hpp"
#include "timeline/timeline.hpp"

// Basic UI smoke tests (non-visual) to ensure core widgets construct and expose minimal API.

TEST_CASE("UI smoke: create TimelinePanel", "[ui]") {
    int argc = 0; char** argv = nullptr;
    static QApplication app(argc, argv); // one per process
    ve::ui::TimelinePanel panel;
    REQUIRE(panel.zoom_factor() >= 0.0);
}

TEST_CASE("UI smoke: timeline panel context menu generation", "[ui]") {
    int argc = 0; char** argv = nullptr;
    static QApplication app(argc, argv);
    ve::ui::TimelinePanel panel;
    // Provide a simple timeline so the panel has something to bind
    auto timeline = std::make_shared<ve::timeline::Timeline>();
    timeline->add_track(ve::timeline::Track::Video, "V1");
    panel.set_timeline(timeline);
    panel.refresh();
    // Simulate a right-click position (0,0) for now; ensure no crash during menu creation.
    // If TimelinePanel has a method to build a context menu, call it indirectly by sending event later.
    // For now we just ensure refresh + set_timeline didn't invalidate internal state.
    REQUIRE(panel.timeline() != nullptr);
}
#endif
