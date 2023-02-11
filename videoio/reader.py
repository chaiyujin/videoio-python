from typing import Optional, Tuple
import numpy as np
import numpy.typing as npt

from .bind.videoio import VideoReader as CPP_VideoReader


class _VideoReader():
    def __init__(self):
        self._reader = CPP_VideoReader()

    def open(self, filename: str, pix_fmt: str = "bgr"):
        self._reader.release()
        self._reader.open(filename, pix_fmt=pix_fmt)

    def read(self) -> Tuple[bool, Optional[npt.NDArray[np.uint8]]]:
        got, im = self._reader.read()
        if not got:
            return False, None
        return got, im

    def release(self):
        self._reader.release()
    
    def seek_frame(self, ifrm: int) -> bool:
        return self._reader.seek_frame(ifrm)

    def seek_msec(self, ts: float) -> bool:
        return self._reader.seek_msec(ts)

    @property
    def fps(self) -> float:
        return self._reader.fps

    @property
    def frame_count(self) -> int:
        return self._reader.n_frames

    @property
    def duration(self) -> float:
        return self._reader.duration

    @property
    def frame_width(self) -> int:
        return self._reader.width
    
    @property
    def frame_height(self) -> int:
        return self._reader.height
    
    @property
    def image_size(self) -> Tuple[int, int]:
        return self._reader.image_size

    @property
    def pos_frames(self) -> int:
        return self._reader.curr_iframe

    @property
    def pos_msec(self) -> float:
        return self._reader.curr_msec
    
    def get(self, prop: int):
        # fmt: off
        import cv2
        if   prop == cv2.CAP_PROP_FPS: return self.fps
        elif prop == cv2.CAP_PROP_FRAME_COUNT:  return self.frame_count
        elif prop == cv2.CAP_PROP_FRAME_HEIGHT: return self.frame_height
        elif prop == cv2.CAP_PROP_FRAME_WIDTH: return self.frame_width
        elif prop == cv2.CAP_PROP_POS_FRAMES: return self.pos_frames
        elif prop == cv2.CAP_PROP_POS_MSEC: return self.pos_msec
        else:
            raise ValueError("The property '{}' is not impl!".format(prop))
        # fmt: on


class VideoReader(_VideoReader):
    def __init__(self, filename: str = "", pix_fmt: str = "bgr"):
        super().__init__()
        if len(filename) > 0:
            self._reader.open(filename, pix_fmt=pix_fmt)


class BytesVideoReader(_VideoReader):
    def __init__(self, bytes: npt.NDArray[np.uint8], pix_fmt: str = "bgr"):
        super().__init__()
        self._reader.open_bytes(bytes, pix_fmt=pix_fmt)
