# ui Module
Responsibility: Qt-based user interface: main window, panels (viewer, timeline, media browser), interaction wiring to commands and playback.
Current: MainWindow with background worker threads, ViewerPanel for frame display, TimelinePanel for track/segment visualization.
Planned: Dockable panels expansion (properties, effects), non-blocking render integration, performance HUD.
Dependencies: app, timeline, playback, commands, core.
