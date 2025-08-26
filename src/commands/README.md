# commands Module
Responsibility: Encapsulate edit operations with undo/redo support using Command pattern and history stack.
Current: CommandHistory integration used by UI (see main_window.cpp). Provides execute/undo/redo and description strings.
Planned: Coalescing for drag operations, macro commands, performance instrumentation (latency stats).
Dependencies: timeline, core (logging/time).
