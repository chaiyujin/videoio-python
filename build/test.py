import os
import cv2
import numpy as np
from tqdm import trange
from timeit import timeit

# test_vpath = os.path.expanduser("~/Movies/test_4k.webm")
# test_vpath = os.path.expanduser("~/Movies/hello.flv")
test_vpath = os.path.expanduser("~/Videos/30fps.mp4")


def test_cv2():
    reader = cv2.VideoCapture(test_vpath)
    n_frames = int(reader.get(cv2.CAP_PROP_FRAME_COUNT))
    for ifrm in range(n_frames):
        if ifrm == 20:
            break
        with timeit("read"):
            ret, img = reader.read()
        if not ret:
            break
        # cv2.imshow('img', img)
        # cv2.waitKey(1)


def test_ffms():
    import ffms
    reader = ffms.VideoReader(test_vpath)
    with timeit("seek"):
        reader.seek_frame(300)
    for ifrm in range(reader.n_frames):
        if ifrm == 10:
            break
        with timeit("read"):
            ret, img = reader.read()
        print(img.shape)
        if not ret:
            break
        cv2.imshow('img', img)
        cv2.waitKey(0)


test_cv2()
test_ffms()
