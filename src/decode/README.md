# decode Module
Responsibility: Decode compressed audio/video frames into raw frame buffers; color conversion; frame caching.
Current: Decoder interface (IDecoder), frame structures, color conversion utilities.
Planned: Hardware acceleration abstraction, async decode queue, error concealment, performance metrics integration.
Dependencies: media_io for demux input, core for timing/logging.
