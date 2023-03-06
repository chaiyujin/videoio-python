import os
import logging
from typing import Any, Dict, Optional
from shutil import move

import ffmpeg
import numpy as np
import numpy.typing as npt

from .bind.videoio import VideoWriter as CPP_VideoWriter

logger = logging.getLogger("videio.VideoWriter")


class VideoWriter(object):

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.release()

    def __init__(
        self,
        output_path: str,
        fps: float,
        audio_source: Optional[str] = None,
        pix_fmt: str = "bgr",
        quality: str = "medium",
        crf: Optional[int] = None,
        makedirs: bool = False,
    ):
        # makedirs
        dirname = os.path.dirname(output_path)
        if len(dirname) > 0:
            if not os.path.isdir(dirname):
                assert makedirs, f"Failed to find directory of '{output_path}'"
                os.makedirs(dirname, exist_ok=True)

        # check audio_source
        if audio_source is not None:
            if not os.path.exists(audio_source):
                logger.warning(f"Ignore audio_source (not found): '{audio_source}'")
                audio_source = None

        # fmt: off
        self._cfg: Dict[str, Any] = { "fps": fps }
        # convert pix_fmt
        pix_fmt = pix_fmt.lower()
        if   pix_fmt == "rgb": pix_fmt = "rgb24"
        elif pix_fmt == "bgr": pix_fmt = "bgr24"
        self._cfg["pix_fmt"] = pix_fmt

        # get preset crf from quality
        if crf is None:
            if   quality == "lossless": crf = 0
            elif quality == "high":     crf = 18
            elif quality == "medium":   crf = 23
            elif quality == "low":      crf = 28
            else: raise ValueError(f"Unknown quality: '{quality}', can be: lossless | high | medium | low .")
        self._cfg["crf"] = crf
        # fmt: on

        self._writer: Optional[CPP_VideoWriter] = None
        self._audio_source: Optional[str] = audio_source
        self._output_path: str = output_path
        self._tmp_vpath: str = os.path.join(os.path.dirname(output_path), "_" + os.path.basename(output_path))

    @property
    def output_path(self) -> str:
        return self._output_path

    @property
    def tmp_video_path(self) -> str:
        return self._tmp_vpath

    @property
    def audio_source(self) -> Optional[str]:
        return self._audio_source

    def write(self, frame: npt.NDArray[Any]) -> bool:
        if frame.dtype in [np.float32, np.float64]:  # type: ignore
            frame = np.clip(frame * 255, 0, 255).astype(np.uint8)  # type: ignore
        if frame.shape[0] % 2 == 1 or frame.shape[1] % 2 == 1:
            frame = frame[: frame.shape[0] // 2 * 2, : frame.shape[1] // 2 * 2]

        if self._writer is None:
            h, w = frame.shape[:2]
            self._writer = CPP_VideoWriter()
            self._writer.open(self._tmp_vpath, (w, h), **self._cfg)

        return self._writer.write(frame[..., :3])

    def close(self):
        if self._writer is None:
            return
        self._writer.release()
        self._writer = None

        if self._audio_source is None:
            move(self._tmp_vpath, self._output_path)
        else:
            extra_kwargs = dict(shortest=None, vcodec="libx264", crf=self._cfg["crf"], acodec="aac")
            in_audio = ffmpeg.input(self._audio_source).audio
            in_video = ffmpeg.input(self._tmp_vpath).video
            cmd = ffmpeg.output(in_audio, in_video, self._output_path, **extra_kwargs)
            cmd.run(overwrite_output=True, quiet=True)
            if os.path.exists(self._tmp_vpath):
                os.remove(self._tmp_vpath)

    """ Compatible with cv2 """

    def release(self):
        self.close()
