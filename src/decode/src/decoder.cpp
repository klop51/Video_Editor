#include "decode/decoder.hpp"
#if VE_HAVE_FFMPEG
#include <memory>
#endif

namespace ve::decode {

#if VE_HAVE_FFMPEG
std::unique_ptr<IDecoder> create_ffmpeg_decoder();
#endif

std::unique_ptr<IDecoder> create_decoder() {
#if VE_HAVE_FFMPEG
    return create_ffmpeg_decoder();
#else
    return nullptr;
#endif
}

} // namespace ve::decode
