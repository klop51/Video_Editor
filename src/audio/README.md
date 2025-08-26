# audio Module
Responsibility: Audio engine (decode integration, mixing graph, output device, waveform extraction).

Current: Stub `AudioEngine`.

Planned:
- Pull-based mixer graph with fixed block size.
- Master clock publication (audio PTS drives video synchronization).
- Waveform tile generation and cache interface.
- Resampling, basic gain/pan nodes, level meters.

Dependencies: `core`, later `decode`, `cache` (waveform tiles).
