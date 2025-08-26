# timeline Module
Responsibility: Project timeline model: tracks, clips, segments, edit operations, selection, playhead.
Current: Mutable timeline with Track/Segment/Clip management, basic duration computation.
Planned: Immutable snapshot generation for playback threads, advanced edit ops (ripple, roll, slip), range operations, serialization hooks.
Dependencies: core (time types), future persistence module.
