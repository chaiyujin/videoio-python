#include "video_reader.hpp"
#include <timer.hpp>
namespace nb = nanobind;

bool VideoReader::_openFile(std::string _filepath, std::string _pix_fmt) {
    FFMS_Indexer * indexer = FFMS_CreateIndexer(_filepath.c_str(), &errinfo_);
    if (indexer == NULL) {
        /* handle error (print errinfo_.Buffer somewhere) */
    }

    FFMS_Index *index = FFMS_DoIndexing2(indexer, FFMS_IEH_ABORT, &errinfo_);
    if (index == NULL) {
        /* handle error (you should know what to do by now) */
    } 

    /* Retrieve the track number of the first video track */
    int trackno = FFMS_GetFirstTrackOfType(index, FFMS_TYPE_VIDEO, &errinfo_);
    if (trackno < 0) {
        /* no video tracks found in the file, this is bad and you should handle it */
        /* (print the errmsg somewhere) */
    }

    /* We now have enough information to create the video source object */
    vid_src_ = FFMS_CreateVideoSource(_filepath.c_str(), trackno, index, 0, FFMS_SEEK_NORMAL, &errinfo_);
    if (vid_src_ == NULL) {
        /* handle error */
    }

    /* Since the index is copied into the video source object upon its creation,
    we can and should now destroy the index object. */
    FFMS_DestroyIndex(index);

    /* Retrieve video properties so we know what we're getting.
    As the lack of the errmsg parameter indicates, this function cannot fail. */
    vid_props_ = FFMS_GetVideoProperties(vid_src_);

    /* Get the first frame for examination so we know what we're getting. This is required
    because resolution and colorspace is a per frame property and NOT global for the video. */
    const FFMS_Frame * propframe = FFMS_GetFrame(vid_src_, 0, &errinfo_);

    /* A -1 terminated list of the acceptable output formats. */
    if (_pix_fmt == "rgb") { _pix_fmt = "rgb24"; }
    if (_pix_fmt == "bgr") { _pix_fmt = "bgr24"; }

    int pixfmts[2];
    pixfmts[0] = FFMS_GetPixFmt(_pix_fmt.c_str());
    pixfmts[1] = -1;

    if (FFMS_SetOutputFormatV2(vid_src_, pixfmts,
                               propframe->EncodedWidth,
                               propframe->EncodedHeight,
                               FFMS_RESIZER_BICUBIC, &errinfo_)) {
        /* handle error */
    }

    this->seekFrame(0);

    // printf("WxH %d %d %s\n", propframe->EncodedWidth, propframe->EncodedHeight, _pix_fmt.c_str());

    return true;
}

void VideoReader::seekFrame(int _frame_index) {
    if (vid_src_) {
        FFMS_GetFrame(vid_src_, _frame_index, &errinfo_);
        cur_idx_ = _frame_index;
    }
}

void VideoReader::seekTime(double _time) {
    if (vid_src_) {
        auto * track = vid_src_->GetTrack();
        int idx = track->ClosestFrameFromPTS(
            static_cast<int64_t>((_time * 1000 * track->TB.Den) / track->TB.Num)
        );
        this->seekFrame(idx);
    }
}

std::pair<bool, VideoReader::NpImage> VideoReader::read() {
    static size_t shape_empty[3] = { 0, 0, 0 };
    static VideoReader::NpImage empty(nullptr, 3, shape_empty);
    if (!vid_src_) {
        return {false, empty};
    }

    // TODO: out-of-range ?

    // read frame
    Timer timer;
    const FFMS_Frame *curframe = FFMS_GetFrame(vid_src_, cur_idx_, &errinfo_);
    cur_idx_ += 1;

    printf("decoding used %.3f ms\n", timer.duration());
    timer.restart();

    // get data
    auto const h = curframe->EncodedHeight;
    auto const w = curframe->EncodedWidth;
    auto const s = curframe->Linesize[0];
    uint8_t * data = new uint8_t[h * s];
    // printf("%d %d %d\n", h, w, s);
    memcpy(data, curframe->Data[0], h * s);

    // Delete 'data' when the 'owner' capsule expires
    nb::capsule owner(data, [](void *p) {
       delete[] (uint8_t *) p;
    });
    printf("memcpy used %.3f ms\n", timer.duration());

    int chs = s / w;  // TODO: channels
    size_t shape[3] = { (size_t)h, (size_t)w, (size_t)chs };
    int64_t strides[3] = { (int64_t)s, (int64_t)chs, (int64_t)1 };
    return {
        true,
        VideoReader::NpImage(data, 3, shape, owner, strides)
    };
}
