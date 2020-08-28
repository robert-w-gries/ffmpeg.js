#include <emscripten/bind.h>
extern "C" {
    #include "libavcodec/avcodec.h"
}

using namespace emscripten;

class AVCodecWrapper {
public:
    AVCodecWrapper(AVCodec codec) : codec(codec) {}
    AVCodec codec;
};

EMSCRIPTEN_BINDINGS(my_module) {
    class_<AVCodecWrapper>("AVCodecWrapper")
        .constructor<AVCodec>()
        .property("codec", &AVCodecWrapper::codec);

    function("avcodec_find_decoder_by_name", &avcodec_find_decoder_by_name, allow_raw_pointers());
    function("avcodec_alloc_context3", &avcodec_alloc_context3, allow_raw_pointers());
    function("avcodec_open2", &avcodec_open2, allow_raw_pointers());
    function("av_packet_alloc", &av_packet_alloc, allow_raw_pointers());
    function("av_frame_alloc", &av_frame_alloc, allow_raw_pointers());
}
