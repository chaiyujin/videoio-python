#pragma once
#include "log.hpp"

extern "C" {
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
}

#define DEFAULT_AVIO_BUFFER_SZ 32768

namespace vio {

class AVIOBase{
public:
    AVIOBase(size_t buffer_size)
        : buffer_size_(buffer_size)
        , buffer_(static_cast<uint8_t *>(av_malloc(buffer_size_)))
        , ctx_(nullptr)
    {}
    virtual ~AVIOBase() {
        if (ctx_) avio_context_free(&ctx_);
        av_free(buffer_);
    }

    virtual int read(unsigned char* buf, int buf_size) = 0;
    virtual int64_t seek(int64_t offset, int whence) = 0;

    void associateFormatContext(AVFormatContext * fmt_ctx) {
        if (fmt_ctx && ctx_) {
            fmt_ctx->pb = ctx_;
            fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;
            fmt_ctx->iformat = this->probeInputFormat();
            if (fmt_ctx->iformat) {
                // spdlog::info(
                //     "AVInputFormat: {} ({}), SEEK_TO_TS {}"
                //     , fmt_ctx->iformat->name
                //     , fmt_ctx->iformat->long_name
                //     , (bool)(fmt_ctx->iformat->flags & AVFMT_SEEK_TO_PTS)
                // );
            }
        }
    }

    auto probeInputFormat() -> const AVInputFormat * {
        int probe_size = 1 * 1024 + AVPROBE_PADDING_SIZE;
        AVProbeData probe_data = {};
        probe_data.filename = "";
        probe_data.buf = (uint8_t*)av_malloc(probe_size);
        probe_data.buf_size = probe_size - AVPROBE_PADDING_SIZE;
        memset(probe_data.buf, 0, probe_size);
        int len = this->read(probe_data.buf, probe_size - AVPROBE_PADDING_SIZE);
        probe_data.buf_size = len;

        const AVInputFormat * inp_fmt = av_probe_input_format(&probe_data, 1);

        av_free(probe_data.buf);

        // seek back to the beginning
        this->seek(0, SEEK_SET);

        return inp_fmt;
    }

protected:
    int buffer_size_;
    uint8_t * buffer_;
    AVIOContext * ctx_;
};


class AVFileIOContext : public AVIOBase {
public:
    AVFileIOContext(const std::string & filename, size_t buffer_size = DEFAULT_AVIO_BUFFER_SZ)
        : AVIOBase(buffer_size)
        , input_file_(nullptr)
    {
        input_file_ = fopen(filename.c_str(), "rb");
        if (input_file_ == nullptr) {
            spdlog::error("Error opening video file: {}", filename);
        }
        ctx_ = avio_alloc_context(
            AVIOBase::buffer_,
            AVIOBase::buffer_size_,
            0,
            this,
            &AVFileIOContext::ReadFile,
            nullptr, // no write function
            &AVFileIOContext::SeekFile
        );
    }

    ~AVFileIOContext() {
        fclose(input_file_);
    }

    int read(unsigned char* buf, int buf_size) {
        return ReadFile(this, buf, buf_size);
    }
    
    int64_t seek(int64_t offset, int whence) {
        return SeekFile(this, offset, whence);
    }

    static int ReadFile(void* opaque, uint8_t * buf, int buf_size) {
        AVFileIOContext * h = static_cast<AVFileIOContext *>(opaque);
        if (feof(h->input_file_)) {
            return AVERROR_EOF;
        }
        size_t ret = fread(buf, 1, buf_size, h->input_file_);
        if ((int)ret < buf_size) {
            if (ferror(h->input_file_)) {
                return -1;
            }
        }
        return ret;
    }

    static int64_t SeekFile(void* opaque, int64_t offset, int whence) {
        AVFileIOContext * h = static_cast<AVFileIOContext *>(opaque);
        switch (whence) {
        case SEEK_CUR: // from current position
        case SEEK_END: // from eof
        case SEEK_SET: // from beginning of file
            return fseek(h->input_file_, static_cast<long>(offset), whence);
            break;
        case AVSEEK_SIZE:
            int64_t cur = ftell(h->input_file_);
            fseek(h->input_file_, 0L, SEEK_END);
            int64_t size = ftell(h->input_file_);
            fseek(h->input_file_, cur, SEEK_SET);
            return size;
        }

        return -1;
    }

private:
    FILE * input_file_;
};


// class VideoIOContext {
// public:
//     explicit VideoIOContext(const std::string & fname)
//         : work_buf_size_(VIO_BUFFER_SZ)
//         , work_buf_((uint8_t*)av_malloc(work_buf_size_))
//         , input_file_(nullptr)
//         , input_buf_(nullptr)
//         , input_buf_size_(0)
//     {
//         input_file_ = fopen(fname.c_str(), "rb");
//         if (input_file_ == nullptr) {
//             LOG(ERROR) << "Error opening video file " << fname;
//         }
//         ctx_ = avio_alloc_context(
//             static_cast<unsigned char*>(work_buf_.get()),
//             work_buf_size_,
//             0,
//             this,
//             &VideoIOContext::readFile,
//             nullptr, // no write function
//             &VideoIOContext::seekFile);
//     }

//     explicit VideoIOContext(const char* buffer, int size)
//         : work_buf_size_(VIO_BUFFER_SZ),
//             work_buf_((uint8_t*)av_malloc(work_buf_size_)),
//             input_file_(nullptr),
//             input_buf_(buffer),
//             input_buf_size_(size) {
//         ctx_ = avio_alloc_context(
//             static_cast<unsigned char*>(work_buf_.get()),
//             work_buf_size_,
//             0,
//             this,
//             &VideoIOContext::readMemory,
//             nullptr, // no write function
//             &VideoIOContext::seekMemory);
//     }
    
//     ~VideoIOContext() {
//         av_free(ctx_);
//         if (input_file_) {
//             fclose(input_file_);
//         }
//     }
    
//     int read(unsigned char* buf, int buf_size) {
//         if (input_buf_) {
//             return readMemory(this, buf, buf_size);
//         } else if (input_file_) {
//             return readFile(this, buf, buf_size);
//         } else {
//             return -1;
//         }
//     }
    
//     int64_t seek(int64_t offset, int whence) {
//         if (input_buf_) {
//             return seekMemory(this, offset, whence);
//         } else if (input_file_) {
//             return seekFile(this, offset, whence);
//         } else {
//             return -1;
//         }
//     }
    
//     static int readFile(void* opaque, unsigned char* buf, int buf_size) {
//         VideoIOContext* h = static_cast<VideoIOContext*>(opaque);
//         if (feof(h->input_file_)) {
//             return AVERROR_EOF;
//         }
//         size_t ret = fread(buf, 1, buf_size, h->input_file_);
//         if (ret < buf_size) {
//             if (ferror(h->input_file_)) {
//                 return -1;
//             }
//         }
//         return ret;
//     }
    
//     static int64_t seekFile(void* opaque, int64_t offset, int whence) {
//         VideoIOContext* h = static_cast<VideoIOContext*>(opaque);
//         switch (whence) {
//         case SEEK_CUR: // from current position
//         case SEEK_END: // from eof
//         case SEEK_SET: // from beginning of file
//             return fseek(h->input_file_, static_cast<long>(offset), whence);
//             break;
//         case AVSEEK_SIZE:
//             int64_t cur = ftell(h->input_file_);
//             fseek(h->input_file_, 0L, SEEK_END);
//             int64_t size = ftell(h->input_file_);
//             fseek(h->input_file_, cur, SEEK_SET);
//             return size;
//         }
    
//         return -1;
//     }
    
//     static int readMemory(void* opaque, unsigned char* buf, int buf_size) {
//         VideoIOContext* h = static_cast<VideoIOContext*>(opaque);
//         if (buf_size < 0) {
//         return -1;
//         }
    
//         int reminder = h->input_buf_size_ - h->offset_;
//         int r = buf_size < reminder ? buf_size : reminder;
//         if (r < 0) {
//         return AVERROR_EOF;
//         }
    
//         memcpy(buf, h->input_buf_ + h->offset_, r);
//         h->offset_ += r;
//         return r;
//     }
    
//     static int64_t seekMemory(void* opaque, int64_t offset, int whence) {
//         VideoIOContext* h = static_cast<VideoIOContext*>(opaque);
//         switch (whence) {
//         case SEEK_CUR: // from current position
//             h->offset_ += offset;
//             break;
//         case SEEK_END: // from eof
//             h->offset_ = h->input_buf_size_ + offset;
//             break;
//         case SEEK_SET: // from beginning of file
//             h->offset_ = offset;
//             break;
//         case AVSEEK_SIZE:
//             return h->input_buf_size_;
//         }
//         return h->offset_;
//     }
    
//     AVIOContext* get_avio() {
//         return ctx_;
//     }

// private:
//     int work_buf_size_;
//     DecodedFrame::AvDataPtr work_buf_;
//     // for file mode
//     FILE* input_file_;
    
//     // for memory mode
//     const char* input_buf_;
//     int input_buf_size_;
//     int offset_ = 0;
    
//     AVIOContext* ctx_;
// };
 
}
