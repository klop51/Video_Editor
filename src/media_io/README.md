# media_io Module
Responsibility: Media probing and (future) demux layer abstracting container formats, exposing elementary streams.
Current: Implements probe_file() to collect stream metadata.
Planned: Packet reader API, error resilience, format normalization, stream selection.
Dependencies: FFmpeg (through future integration), core.
