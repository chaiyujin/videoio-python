import sys
import numpy as np
from tqdm import trange
from videoio import VideoReader

vpath = sys.argv[1]
reader_file = VideoReader()
reader_file.open(vpath)

nparr = np.fromfile(vpath, dtype=np.uint8)
print(nparr.shape)

with open(vpath, "rb") as fp:
    data = fp.read()
    print(len(data))

nparr = np.frombuffer(data, dtype=np.uint8)
print(nparr.shape)

reader = VideoReader()
reader.open_bytes(nparr)

print(reader_file.n_frames, reader.n_frames)
for i in trange(reader_file.n_frames):
    got, im0 = reader_file.read()
    got, im1 = reader.read()
    assert got
    assert np.all(im0 == im1)
