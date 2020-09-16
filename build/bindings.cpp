#include <emscripten/bind.h>
#include <emscripten/val.h>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/opt.h>
    #include <libswresample/swresample.h>
}

using namespace emscripten;

void printf_wrapper(const char *s) {
    printf("Error: %s\n", s);
}

std::vector<uint8_t> vecFromArray(const val &arr) {
  unsigned int length = arr["length"].as<unsigned int>();
  std::vector<uint8_t> vec(length);
  val heap = val::module_property("HEAPU8");
  val memory = heap["buffer"];
  val memoryView = val::global("Uint8Array").new_(memory, reinterpret_cast<uintptr_t>(vec.data()), length);
  memoryView.call<void>("set", arr);
  return vec;
}

class AVCodecWrapper {
public:
    AVCodecWrapper() {
        codec = avcodec_find_decoder(AV_CODEC_ID_MP3);
        if (!codec) {
            printf_wrapper("no mp3 decoder");
        }

        parser = av_parser_init(AV_CODEC_ID_MP3);
        if (!parser) {
            printf_wrapper("parser not found");
        }

        context = avcodec_alloc_context3(codec);
        if (!context) {
            printf_wrapper("could not create avcodec context");
        }

        int ret = avcodec_open2(context, codec, 0);
        if (ret < 0) {
            printf_wrapper("could not open codec");
        }

        swr = swr_alloc();
        if (!swr) {
            printf_wrapper("could not create swr context");
        }
    }
    ~AVCodecWrapper() {
        avcodec_free_context(&context);
        av_parser_close(parser);
        swr_free(&swr);
    }

    double getBitRate() {
        return static_cast<double>(context->bit_rate);
    }

    val decode(const val &typedArray) {
        out_buffer.clear();
        AVPacket *packet = av_packet_alloc();
        if (!packet) {
            printf_wrapper("could not allocate packet");
        }

        std::vector<uint8_t> in_buffer = vecFromArray(typedArray);
        const uint8_t *in_data = in_buffer.data();
        int in_size = in_buffer.size();

        while (in_size > 0) {
            int parsed_len = av_parser_parse2(parser, context,
                                        &packet->data, &packet->size,
                                        in_data, in_size,
                                        AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);

            in_data += parsed_len;
            in_size -= parsed_len;

            if (packet->size) {
                decode_packet(packet);
            }
        }

        // flush the buffers
        decode_packet(NULL);
        avcodec_flush_buffers(context);

        av_packet_free(&packet);
        return val(typed_memory_view(out_buffer.size(), out_buffer.data()));
    }
private:
    std::vector<float> out_buffer;
    AVCodec *codec;
    AVCodecContext *context;
    AVCodecParserContext *parser;
    struct SwrContext *swr;

    void decode_packet(AVPacket *packet) {
        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            printf_wrapper("could not allocate frame");
            return;
        }

        int ret = avcodec_send_packet(context, packet);
        if (ret < 0) {
            printf_wrapper("could not send packet");
            if (ret == AVERROR(EAGAIN)) printf_wrapper("EAGAIN");
            if (ret == AVERROR_EOF) printf_wrapper("AVERROR_EOF");
            if (ret == AVERROR(EINVAL)) printf_wrapper("EINVAL");
            if (ret == AVERROR(ENOMEM)) printf_wrapper("ENOMEM");
            return;
        }

        if (!swr_is_initialized(swr)) {
            init_swr();
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                printf_wrapper("Error during decoding\n");
                break;
            }
            float *buffer;
            av_samples_alloc((uint8_t**) &buffer, NULL, 1, frame->nb_samples, AV_SAMPLE_FMT_FLT, 0);
            int frame_count = swr_convert(swr, (uint8_t**) &buffer, frame->nb_samples, (const uint8_t**) frame->data, frame->nb_samples);
            for (int i = 0; i < frame_count; i++) {
                auto byte = *(buffer + i);
                out_buffer.push_back(byte);
            }
        }
        av_frame_free(&frame);
    }

    void init_swr() {
        av_opt_set_int(swr, "in_channel_count",  context->channels, 0);
        av_opt_set_int(swr, "out_channel_count", 1, 0);
        av_opt_set_int(swr, "in_channel_layout",  context->channel_layout, 0);
        av_opt_set_int(swr, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
        av_opt_set_int(swr, "in_sample_rate", context->sample_rate, 0);
        av_opt_set_int(swr, "out_sample_rate", 44100, 0);
        av_opt_set_sample_fmt(swr, "in_sample_fmt",  context->sample_fmt, 0);
        av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
        swr_init(swr);
        if (!swr_is_initialized(swr)) {
            printf_wrapper("Failed to initialize the resampling context");
        }
    }
};

EMSCRIPTEN_BINDINGS(my_module) {
    class_<AVCodecWrapper>("AVCodecWrapper")
        .constructor<>()
        .function("decode", &AVCodecWrapper::decode, allow_raw_pointers())
        .function("getBitRate", &AVCodecWrapper::getBitRate);
}
