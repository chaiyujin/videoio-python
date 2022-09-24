import numpy as np
from videoio import VideoWriter, VideoReader

writer = VideoWriter()
writer.set_log_level("debug")
writer.open("test.mp4", (640, 480), 30)
im = np.full((480, 640, 3), 120, np.uint8)
for i in range(100):
    writer.write(im)
writer.close()

reader = VideoReader()
reader.open("test.mp4")
print(f"fps {reader.fps}, duration {reader.duration}, n_frames {reader.n_frames}")
frames = []

while True:
    got, im = reader.read()
    if not got:
        break
    frames.append(im)
print(len(frames))
