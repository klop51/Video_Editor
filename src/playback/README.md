# playback Module
Responsibility: High-level media playback orchestration (decode loop, timing, state machine) feeding UI/render layer.
Current: Controller with background thread, callbacks for video/audio frames, basic seek & adaptive sleep based on PTS.
Planned: Integration with timeline snapshots, audio-driven master clock, dropped frame strategy, synchronization with render graph.
Dependencies: decode, media_io (probe), core.
