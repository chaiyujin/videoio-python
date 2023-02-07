import numpy as np
from tqdm import trange
from videoio import VideoWriter, VideoReader

reader = VideoReader("test.mp4")
frames = []

while True:
    got, im = reader.read()
    if not got:
        break
    frames.append(im)
print(len(frames))

reader.seek_frame(0)
writer = VideoWriter("test_write.mp4", fps=reader.fps, audio_source="test.mp4", quality="high")
for i in trange(reader.frame_count, desc="write video"):
    got, im = reader.read()
    if not got or im is None:
        break
    writer.write(im)
writer.release()
reader.release()
