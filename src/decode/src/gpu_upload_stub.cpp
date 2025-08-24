#include "decode/gpu_upload.hpp"
#include <atomic>

namespace ve::decode {

namespace { class StubUploader final : public IGpuUploader {
public:
    std::optional<UploadResult> upload_rgba(const VideoFrame& rgba_frame) override {
        UploadResult r;
        r.handle.id = next_id_++;
        r.handle.width = rgba_frame.width;
        r.handle.height = rgba_frame.height;
        r.reused = false;
        return r;
    }
private:
    static std::atomic<uint64_t> next_id_;
};
std::atomic<uint64_t> StubUploader::next_id_{1};
}

std::unique_ptr<IGpuUploader> create_stub_uploader() { return std::make_unique<StubUploader>(); }

} // namespace ve::decode
