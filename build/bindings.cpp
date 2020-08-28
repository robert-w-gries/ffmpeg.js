#include <emscripten/bind.h>
extern "C" {
    #include "libavcodec/avcodec.h"
}

using namespace emscripten;

void printf_wrapper(const char *s) {
    printf("%s\n", s);
}

class AVCodecWrapper {
public:
    AVCodecWrapper(std::string decoder_name) {
        codec = avcodec_find_decoder_by_name(decoder_name.c_str());
        if (!codec) {
            printf_wrapper("no mp3 decoder");
        }

        packet = av_packet_alloc();
        if (!packet) {
            printf_wrapper("could not allocate packet");
        }

        parser = av_parser_init(codec->id);
        if (!parser) {
            printf_wrapper("Parser not found");
        }

        context = avcodec_alloc_context3(codec);
        if (!context) {
            printf_wrapper("Error while creating avcodec context");
        }

        int ret = avcodec_open2(context, codec, 0);
        if (ret < 0) {
            printf_wrapper("Could not open codec");
        }
    }
    ~AVCodecWrapper() {
        avcodec_free_context(&context);
        av_parser_close(parser);
        av_packet_free(&packet);
        av_frame_free(&frame);
    }

    void decode(const uint8_t *inBuffer, const uint8_t *outBuffer) {

    }
private:
    AVCodec *codec;
    AVPacket *packet;
    AVFrame *frame;
    AVCodecContext *context;
    AVCodecParserContext *parser;
};

EMSCRIPTEN_BINDINGS(my_module) {
    class_<AVCodecWrapper>("AVCodecWrapper")
        .constructor<std::string>();
}
