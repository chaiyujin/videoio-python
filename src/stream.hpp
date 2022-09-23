#pragma once
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include "common.hpp"

namespace vio {

/**
 * The class hold data and contexts for a stream.
 * */
class StreamData {

protected:
    // ffmpeg related pointers, RAII
    std::unique_ptr<AVStream,       void(*)(AVStream *)>       stream_;
    std::unique_ptr<AVCodec const,  void(*)(AVCodec const *)>  codec_;
    std::unique_ptr<AVCodecContext, void(*)(AVCodecContext *)> codec_ctx_;
    std::unique_ptr<AVFrame,        void(*)(AVFrame *)>        frame_;
    std::unique_ptr<AVFrame,        void(*)(AVFrame *)>        tmp_frame_; // for conversion
    std::unique_ptr<SwsContext,     void(*)(SwsContext *)>     sws_ctx_;   // video conversion context
    std::unique_ptr<SwrContext,     void(*)(SwrContext *)>     swr_ctx_;   // audio conversion context

public:
    StreamData()
        : stream_   (nullptr, [](AVStream       * ) {})
        , codec_    (nullptr, [](AVCodec const  * ) {})
        , codec_ctx_(nullptr, [](AVCodecContext *x) { if (x) avcodec_free_context(&x); })
        , frame_    (nullptr, [](AVFrame        *x) { if (x) av_frame_free(&x);        })
        , tmp_frame_(nullptr, [](AVFrame        *x) { if (x) av_frame_free(&x);        })
        , sws_ctx_  (nullptr, [](SwsContext     *x) { if (x) sws_freeContext(x);       })
        , swr_ctx_  (nullptr, [](SwrContext     *x) { if (x) swr_free(&x);             })
    {
        set_frame(av_frame_alloc());
    }

    virtual ~StreamData() {}
    virtual void reset() {
        sws_ctx_  .reset();
        swr_ctx_  .reset();
        frame_    .reset();
        tmp_frame_.reset();
        stream_   .reset();
        codec_ctx_.reset();
        codec_    .reset();
    }

    AVStream          * stream()       { return stream_.get();    }
    AVCodec const     * codec()        { return codec_.get();     }
    AVCodecContext    * codec_ctx()    { return codec_ctx_.get(); }
    AVFrame           * frame()        { return frame_.get();     }
    AVFrame           * tmp_frame()    { return tmp_frame_.get(); }
    SwsContext        * sws_ctx()      { return sws_ctx_.get();   }
    SwrContext        * swr_ctx()      { return swr_ctx_.get();   }

    void set_stream(AVStream *ptr)          { stream_   .reset(ptr);   }
    void set_codec(AVCodec const *ptr)      { codec_    .reset(ptr);   }
    void set_codec_ctx(AVCodecContext *ptr) { codec_ctx_.reset(ptr);   }
    void set_frame(AVFrame *frame)          { frame_    .reset(frame); }
    void set_tmp_frame(AVFrame *frame)      { tmp_frame_.reset(frame); }
    void set_sws_ctx(SwsContext *ptr)       { sws_ctx_  .reset(ptr);   }
    void set_swr_ctx(SwrContext *ptr)       { swr_ctx_  .reset(ptr);   }
};


class CircleBuffer {
    size_t head_;
    size_t tail_;
    size_t size_;
    std::vector<AVFrame *> buffer_;
    bool allocated_;

    void _inc(size_t & _idx) {
        _idx = (_idx + 1) % buffer_.size();
    }

    void _cleanup() {
        for (auto * & p : buffer_) {
            if (p) {
                av_frame_free(&p);
                p = nullptr;
            }
        }
        buffer_.clear();
        allocated_ = false;
        head_ = tail_ = 0;
    }

public:
    CircleBuffer()
        : head_(0), tail_(0)  // [head_, tail_)
        , size_(0), buffer_()
        , allocated_(false)
        // ! one extra item for judging full queue.
        // ! so that, full is (tail_ + 1 == head_), empty is (tail_ == head_)
    {}
    ~CircleBuffer() {
        this->_cleanup();
    }

    void clear() {
        this->_cleanup();
    }

    void allocate(size_t size, enum AVPixelFormat pixFmt, int width, int height) {
        this->_cleanup();
        buffer_.resize(size + 1, nullptr);
        size_ = size;
        for (size_t i = 0; i < buffer_.size(); ++i) {
            buffer_[i] = AllocateFrame(pixFmt, width, height);
        }
        allocated_ = true;
    }

    size_t size() const {
        return size_;
    }

    size_t n_elements() const {
        if (tail_ >= head_) {
            return tail_ - head_;
        } else {
            return tail_ + buffer_.size() - head_;
        }
    }

    bool is_empty() const {
        return tail_ == head_;
    }

    bool is_full() const {
        return (tail_ + 1 == head_) || (head_ == 0 && tail_ == size_);
    }

    void push_back() {
        this->_inc(tail_);
        if (tail_ == head_) {
            this->_inc(head_);
        }
    }

    void pop_front() {
        // ! must check not empty before pop;
        if (!this->is_empty()) {
            this->_inc(head_);
        }
    }

    AVFrame * offset_front(size_t _off) {
        assert(allocated_);
        return buffer_[(head_ + _off) % buffer_.size()];
    }

    AVFrame * offset_back(size_t _off) {
        assert(allocated_);
        _off += 1; // since 'tail_-1' point to last element
        size_t t = tail_;
        if (t < _off) {
            t += buffer_.size();
        }
        return buffer_[t - _off];
    }
};

class InputStreamData : public StreamData {
public:
    InputStreamData(): buffer_(), pkt_({}) {}
    auto buffer() const -> CircleBuffer const & { return buffer_; }
    auto buffer()       -> CircleBuffer       & { return buffer_; }
    auto packet()       -> AVPacket           & { return pkt_; }
    auto image_size() const -> std::pair<int, int> const & { return image_size_; }
    auto image_size()       -> std::pair<int, int>       & { return image_size_; }

private:
    CircleBuffer buffer_;
    AVPacket pkt_;
    std::pair<int, int> image_size_;
};

}
