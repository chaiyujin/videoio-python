import os
import cv2
import numpy as np
from tqdm import trange
from timeit import timeit

test_vpath = os.path.expanduser("~/Movies/test_4k.webm")
# test_vpath = os.path.expanduser("~/Movies/hello.flv")
# test_vpath = os.path.expanduser("~/Videos/30fps.mp4")
# test_vpath = "../assets/050.mp4"


def test_cv2():
    reader = cv2.VideoCapture(test_vpath)
    n_frames = int(reader.get(cv2.CAP_PROP_FRAME_COUNT))
    fps = reader.get(cv2.CAP_PROP_FPS)
    print(n_frames, fps)
    return
    for ifrm in range(n_frames):
        if ifrm == 20:
            break
        with timeit("read"):
            ret, img = reader.read()
        if not ret:
            break
        # cv2.imshow('img', img)
        # cv2.waitKey(1)


def test_ffutils():
    import ffutils
    reader = ffutils.VideoReader(test_vpath)
    print(reader.resolution, reader.width, reader.height, reader.n_frames, reader.fps)
    quit()

    for i in range(100):
        print(reader.n_frames, reader.duration)
        idx = np.random.randint(0, reader.n_frames)
        msec = idx * 1000 / 30
        with timeit("seek {}, {:.3f}".format(idx, msec)):
            reader.seek_msec(msec)
            ret, img = reader.read()
        # if ret:
        #     cv2.imshow('img', img)
        #     cv2.waitKey(0)

    for ifrm in range(reader.n_frames):
        if ifrm == 10:
            break
        with timeit("read"):
            ret, img = reader.read()
        print(img.shape)
        if not ret:
            break
        # cv2.imshow('img', img)
        # cv2.waitKey(0)


test_cv2()
test_ffutils()
