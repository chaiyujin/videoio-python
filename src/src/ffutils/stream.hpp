#pragma once
#include "common.hpp"
#include <vector>
#include <string>

namespace ffutils {

struct MediaConfig {
    struct Audio {
        int32_t  sample_rate;
        int32_t  channels;
        uint64_t layout;
        int32_t  bitrate;

        Audio(int32_t _sr=0, int32_t _chs=0, uint64_t _layout=0, int32_t _br=0)
            : sample_rate(_sr), channels(_chs), layout(_layout), bitrate(_br)
        {}
    };

    struct Video {
        int2          resolution;
        AVPixelFormat pix_fmt;
        AVRational    fps;
        int32_t       rotation;
        int32_t       bitrate;

        Video(
            int2          _res={0, 0},
            AVRational    _fps={0, 1}, 
            AVPixelFormat _pix_fmt=AV_PIX_FMT_RGB24,
            int32_t       _rotation=0,
            int32_t       _br=0
        )
            : resolution(_res)
            , pix_fmt(_pix_fmt)
            , fps(_fps)
            , rotation(_rotation)
            , bitrate(_br)
        {}
    };

    Audio audio;
    Video video;
};

class Stream {

protected:
    AVMediaType type_;
    MediaConfig config_;
    // ffmpeg related pointers, RAII
    std::unique_ptr<AVCodec const,  void(*)(AVCodec const *)>  codec_;
    std::unique_ptr<AVCodecContext, void(*)(AVCodecContext *)> codec_ctx_;
    std::unique_ptr<AVStream,       void(*)(AVStream *)>       stream_;
    std::unique_ptr<AVFrame,        void(*)(AVFrame *)>        frame_;
    std::unique_ptr<AVFrame,        void(*)(AVFrame *)>        tmp_frame_; // for conversion
    std::unique_ptr<SwsContext,     void(*)(SwsContext *)>     sws_ctx_;   // video conversion context
    std::unique_ptr<SwrContext,     void(*)(SwrContext *)>     swr_ctx_;   // audio conversion context

public:
    Stream()
        : type_     (AVMEDIA_TYPE_UNKNOWN)
        , config_   ()
        , codec_    (nullptr, [](AVCodec const *) {})
        , codec_ctx_(nullptr, [](AVCodecContext *x) { if (x) avcodec_free_context(&x); })
        , stream_   (nullptr, [](AVStream       *) {})
        , frame_    (nullptr, [](AVFrame        *x) { if (x) av_frame_free(&x);        })
        , tmp_frame_(nullptr, [](AVFrame        *x) { if (x) av_frame_free(&x);        })
        , sws_ctx_  (nullptr, [](SwsContext     *x) { if (x) sws_freeContext(x);       })
        , swr_ctx_  (nullptr, [](SwrContext     *x) { if (x) swr_free(&x);             })
    {
        set_frame(av_frame_alloc());
    }

    virtual ~Stream() {}
    virtual void reset() {
        type_ = AVMEDIA_TYPE_UNKNOWN;
        config_ = MediaConfig();
        sws_ctx_  .reset();
        swr_ctx_  .reset();
        frame_    .reset();
        tmp_frame_.reset();
        stream_   .reset();
        codec_ctx_.reset();
        codec_    .reset();
    }

    AVMediaType         type()   const { return type_;            }
    MediaConfig const & config() const { return config_;          }
    AVStream          * stream()       { return stream_.get();    }
    AVCodec const     * codec()        { return codec_.get();     }
    AVCodecContext    * codec_ctx()    { return codec_ctx_.get(); }
    AVFrame           * frame()        { return frame_.get();     }
    AVFrame           * tmp_frame()    { return tmp_frame_.get(); }
    SwsContext        * sws_ctx()      { return sws_ctx_.get();   }
    SwrContext        * swr_ctx()      { return swr_ctx_.get();   }

    void set_type(AVMediaType type)         { type_ = type; }
    void set_config(MediaConfig const &cfg) { config_ = cfg; }
    void set_codec(AVCodec const *ptr)      { codec_    .reset(ptr);   }
    void set_codec_ctx(AVCodecContext *ptr) { codec_ctx_.reset(ptr);   }
    void set_stream(AVStream *ptr)          { stream_   .reset(ptr);   }
    void set_frame(AVFrame *frame)          { frame_    .reset(frame); }
    void set_tmp_frame(AVFrame *frame)      { tmp_frame_.reset(frame); }
    void set_sws_ctx(SwsContext *ptr)       { sws_ctx_  .reset(ptr);   }
    void set_swr_ctx(SwrContext *ptr)       { swr_ctx_  .reset(ptr);   }
};

}
