# videio-python
It's a simple videoio for python implemented by FFmpeg.

## Highlights:
- The api is almost the same as `cv2`, but the `VideoWriter` is much easier to create.
- `VideoReader`: support **frame-accurate seeking**.
- `VideoWriter`: support high-quality compressing (low `crf` for `libx264` encoded `.mp4`); audio merging.

## Warning
This repo is still under development. It's only tested on `.mp4` with `libx264` codec so far.

## A simple example:
```python
import argparse
import cv2
import numpy as np
from tqdm import trange
from videoio import VideoReader, VideoWriter

parser = argparse.ArgumentParser()
parser.add_argument("vpath", type=str)
vpath = parser.parse_args().vpath

reader = VideoReader(vpath)
# Get video properties
print("Video '{}': fps {}, duration {}ms, frame_count: {}, size: {}x{}".format(
    vpath,
    reader.fps,
    reader.duration,
    reader.frame_count,
    reader.frame_width,
    reader.frame_height
))

# With cv2.CAP_PROP_*
print("Video '{}': fps {}, frame_count: {}, size: {}x{}".format(
    vpath,
    reader.get(cv2.CAP_PROP_FPS),
    reader.get(cv2.CAP_PROP_FRAME_COUNT),
    reader.get(cv2.CAP_PROP_FRAME_WIDTH),
    reader.get(cv2.CAP_PROP_FRAME_HEIGHT)
))

# Frame-Accurate Seeking
for ifrm in np.random.randint(reader.frame_count, size=(10,), dtype=np.int64):
    reader.seek_frame(ifrm)
    print("Try to seek frame # {}. Got {} ({}ms).".format(
        ifrm, reader.pos_frames, reader.pos_msec
    ))
    # Also, cv2.CAP_PROP_POS_*
    assert reader.pos_frames == reader.get(cv2.CAP_PROP_POS_FRAMES)
    assert reader.pos_msec == reader.get(cv2.CAP_PROP_POS_MSEC)
    assert ifrm == reader.pos_frames, "Failed to seek accurate frame {}!".format(ifrm)

# Write into another video. The audio source is from the original video.
reader.seek_frame(0)
writer = VideoWriter("test.mp4", fps=reader.fps, audio_source=vpath, quality="high")
for i in trange(reader.frame_count, desc="write video"):
    got, im = reader.read()
    if not got or im is None:
        break
    writer.write(im)
writer.release()
reader.release()
```

# Dependency
## Python Packages
You can install following packages with `pip` or `conda`.
- numpy
- pybind11

## FFmpeg
* MacOS: `brew install ffmpeg`. The installed ffmpeg can be found in the directory `/usr/local/Cellar/ffmpeg/`.

* Ubuntu: You can use `apt` to install libraries:
```
sudo apt install ffmpeg \
    libavutil-dev libavcodec-dev \
    libavformat-dev libavdevice-dev \
    libavfilter-dev libswscale-dev \
    libswresample-dev libpostproc-dev;
```
Instead, if you want lastest FFmpeg version, you may compile from source code by running the provided script:
```
FFMPEG_VERSION=5.1.1 FFMPEG_HOME=~/ffmpeg_build bash scripts/install_ffmpeg_ubuntu.sh
```

* [ ] Windows:

# Install
If you prepare PyBind11 and FFmpeg as described in *Section Dependency*, you can simply run:
```
pip install .
```
The `setup.py` will find the installed FFmpeg.

Otherwise, you need specify the home of installed FFmpeg by setting the environment variable `FFMPEG_HOME`. E.g.,
```
FFMPEG_HOME=/usr/local/Cellar/ffmpeg/5.1.1 pip install .
```
