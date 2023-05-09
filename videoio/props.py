import os
from typing import Any, Dict, Optional

import ffmpeg


def get_video_properties(video_path: str) -> Optional[Dict[str, Any]]:
    if not os.path.exists(video_path):
        return None

    probe: Any = ffmpeg.probe(video_path)  # type: ignore
    # only return the info of first video track
    for s in probe["streams"]:
        if s["codec_type"] != "video":
            continue
        fps_num, fps_den = s["avg_frame_rate"].split("/")
        return dict(
            width=int(s["width"]),
            height=int(s["height"]),
            duration=float(s["duration"]),
            fps=float(fps_num) / float(fps_den),
            n_frames=int(s["nb_frames"]),
        )
    return None
