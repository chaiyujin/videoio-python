# FFmpeg Python Utils
- VideoReader
    - Similar to cv2.VideoCapture, only for video file.
    - Support frame-accurate seeking.
- [ ] VideoWriter

# Dependency
- PyBind11: `pip install pybind11`
- FFmpeg.
    - MacOS: `brew install ffmpeg`. The installed ffmpeg can be found in the directory `/usr/local/Cellar/ffmpeg/`.
    - [ ] Ubuntu:
    - [ ] Windows:

# Install
If you prepare FFmpeg as described in *Section Dependency*, you can simply run:
```
pip install .
```
The `setup.py` will find the installed FFmpeg.

Otherwise, you need specify the home of installed FFmpeg by setting the environment variable `FFMPEG_HOME`. E.g.,
```
FFMPEG_HOME=/usr/local/Cellar/ffmpeg/5.1.1 pip install .
```
