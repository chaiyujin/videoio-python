#include <ffms.h>
#include <string>
#include <iostream>

int main() {
    std::string _fpath = "/Users/chaiyujin/Movies/hello.flv";

    FFMS_ErrorInfo errinfo_;

    FFMS_Indexer * indexer = FFMS_CreateIndexer(_fpath.c_str(), &errinfo_);
    FFMS_Index   * _index  = FFMS_DoIndexing2(indexer, FFMS_IEH_ABORT, &errinfo_);

    /* We now have enough information to create the video source object */
    auto * video_source = FFMS_CreateVideoSource(
        _fpath.c_str(), 0, _index, 1, FFMS_SEEK_NORMAL, &errinfo_
    );

    /* Get the first frame for examination so we know what we're getting. This is required
    because resolution and colorspace is a per frame property and NOT global for the video. */
    const FFMS_Frame *propframe = FFMS_GetFrame(video_source, 0, &errinfo_);
    if (propframe == nullptr) {
        FFMS_DestroyVideoSource(video_source);
        video_source = nullptr;
        return 1;
    }

    std::cout << propframe->EncodedWidth << " " << propframe->EncodedHeight << std::endl;

    /* If you want to change the output colorspace or resize the output frame size,
    now is the time to do it. IMPORTANT: This step is also required to prevent
    resolution and colorspace changes midstream. You can you can always tell a frame's
    original properties by examining the Encoded* properties in FFMS_Frame. */
    /* See libavutil/pixfmt.h for the list of pixel formats/colorspaces.
    To get the name of a given pixel format, strip the leading PIX_FMT_
    and convert to lowercase. For example, PIX_FMT_YUV420P becomes "yuv420p". */

    /* A -1 terminated list of the acceptable output formats. */
    int pixfmts[2];
    pixfmts[0] = FFMS_GetPixFmt("rgba");
    pixfmts[1] = -1;

    if (FFMS_SetOutputFormatV2(video_source, pixfmts,
                               propframe->EncodedWidth,
                               propframe->EncodedHeight,
                               FFMS_RESIZER_BICUBIC, &errinfo_))
    {
        FFMS_DestroyVideoSource(video_source);
        video_source = nullptr;
        return 1;
    }

    {
        const FFMS_Frame *curframe = FFMS_GetFrame(video_source, 300, &errinfo_);
        if (curframe == NULL) {
            exit(1);
        }
    }

    return 0;
}
