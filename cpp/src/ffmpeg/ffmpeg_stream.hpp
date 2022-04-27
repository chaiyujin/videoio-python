#pragma once
#include "_common.hpp"
// #include "_types.hpp"
#include "ffmpeg_fn.hpp"

struct int2 {
    int32_t x = 0, y = 0;
};

inline int32_t prod(int2 const & _v) {
    return _v.x * _v.y;
}

struct Config
{
    struct Audio
    {
        int32_t  sample_rate;
        int32_t  channels;
        uint64_t layout;
        int32_t  bitrate;

        Audio(int32_t _sr=0, int32_t _chs=0, uint64_t _layout=0, int32_t _br=0)
            : sample_rate(_sr), channels(_chs), layout(_layout), bitrate(_br) {}
    };

    struct Video
    {
        int2     resolution;
        int32_t  bpp;
        AVRational fps;
        int32_t  rotation;
        int32_t  bitrate;

        Video(int2     _res={0,0}, int32_t _bpp=0,
            AVRational _fps={0,1}, int32_t _rotation=0,
            int32_t      _br =0)
            : resolution(_res), bpp(_bpp)
            , fps(_fps), rotation(_rotation)
            , bitrate(_br) {}
    };

    Audio audio;
    Video video;
};

class Stream
{
public:
    Stream()
        : type_     (AVMEDIA_TYPE_UNKNOWN)
        , codec_    (nullptr)
        , codec_ctx_(nullptr)
        , stream_   (nullptr)
        , frame_    (nullptr)
        , tmp_frame_(nullptr)
        , sws_ctx_  (nullptr)
        , swr_ctx_  (nullptr)
        , config_   (Config())
    {
        set_frame(av_frame_alloc());
    }

    virtual ~Stream() {}
    virtual void reset()
    {
        type_ = AVMEDIA_TYPE_UNKNOWN;
        codec_    .reset();
        codec_ctx_.reset();
        stream_   .reset();
        frame_    .reset();
        tmp_frame_.reset();
        sws_ctx_  .reset();
        swr_ctx_  .reset();
    }

    AVMediaType      type()   const { return type_;            }
    const Config   & config() const { return config_;          }
    AVStream       * stream()       { return stream_.get();    }
    AVCodec const  * codec()        { return codec_.get();     }
    AVCodecContext * codec_ctx()    { return codec_ctx_.get(); }
    AVFrame        * frame()        { return frame_.get();     }
    AVFrame        * tmp_frame()    { return tmp_frame_.get(); }
    SwsContext     * sws_ctx()      { return sws_ctx_.get();   }
    SwrContext     * swr_ctx()      { return swr_ctx_.get();   }

    void set_type(AVMediaType type)         { type_ = type; }
    void set_config(const Config &cfg)      { config_ = cfg; }
    void set_stream(AVStream *ptr)          { stream_   .reset(ptr,   [](AVStream       *x) {}); }
    void set_codec(AVCodec const *ptr)      { codec_    .reset(ptr,   [](AVCodec const  *x) {}); }
    void set_codec_ctx(AVCodecContext *ptr) { codec_ctx_.reset(ptr,   [](AVCodecContext *x) { if (x) avcodec_free_context(&x); }); }
    void set_frame(AVFrame *frame)          { frame_    .reset(frame, [](AVFrame        *x) { if (x) av_frame_free(&x);        }); }
    void set_tmp_frame(AVFrame *frame)      { tmp_frame_.reset(frame, [](AVFrame        *x) { if (x) av_frame_free(&x);        }); }
    void set_sws_ctx(SwsContext *ptr)       { sws_ctx_  .reset(ptr,   [](SwsContext     *x) { if (x) sws_freeContext(x);       }); }
    void set_swr_ctx(SwrContext *ptr)       { swr_ctx_  .reset(ptr,   [](SwrContext     *x) { if (x) swr_free(&x);             }); }

protected:
    AVMediaType                     type_;
    std::shared_ptr<AVCodec const>  codec_;
    std::shared_ptr<AVCodecContext> codec_ctx_;
    std::shared_ptr<AVStream>       stream_;
    std::shared_ptr<AVFrame>        frame_;
    std::shared_ptr<AVFrame>        tmp_frame_; // for conversion
    std::shared_ptr<SwsContext>     sws_ctx_;   // video conversion context
    std::shared_ptr<SwrContext>     swr_ctx_;   // audio conversion context
    Config                          config_;
};
