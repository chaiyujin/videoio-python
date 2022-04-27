import os
# import ffms
import cv2
from tqdm import trange
from timeit import timeit

test_vpath = os.path.expanduser("~/Movies/test_4k.webm")


def test_cv2():
    reader = cv2.VideoCapture(test_vpath)
    n_frames = int(reader.get(cv2.CAP_PROP_FRAME_COUNT))
    for ifrm in trange(n_frames):
        if ifrm == 20:
            break
        with timeit("read"):
            ret, img = reader.read()
        if not ret:
            break
        # cv2.imshow('img', img)
        # cv2.waitKey(1)


def test_ffms():
    reader = ffms.VideoReader(test_vpath, "bgr")
    for ifrm in trange(reader.n_frames):
        with timeit("read"):
            ret, img = reader.read()
        if not ret:
            break
        cv2.imshow('img', img)
        cv2.waitKey(1)


test_cv2()
# test_ffms()
