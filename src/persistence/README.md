# persistence Module
Responsibility: Serialize and deserialize project/timeline state (JSON dev format, future binary optimized format).

Current: `save_timeline_json` placeholder writing minimal JSON, load stub.

Planned Short-Term:
- Serialize tracks, clips, segments with timing (rational to microseconds for JSON simplicity) + frame rate.
- Add project root object and version field for migration.
- Implement basic load with validation (no overlapping segments enforcement reused from timeline).

Longer-Term:
- Binary container (chunked, compressed) for large projects.
- Incremental save (append-only change log) for autosave/crash recovery.
- Migration pipeline (versioned transforms).

Security Considerations:
- Validate all numeric ranges (duration, positions) on load.
- Reject unknown required fields; ignore unrecognized optional fields with warning.
