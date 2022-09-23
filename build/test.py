import os
import cv2
import numpy as np
from tqdm import trange, tqdm
from timeit import timeit

# test_vpath = os.path.expanduser("~/Movies/test_4k.webm")
test_vpath = os.path.expanduser("~/Movies/hello.flv")
# test_vpath = os.path.expanduser("~/Movies/ours-full.mp4")
# test_vpath = os.path.expanduser("~/Movies/cc-by/indentfy the pay off your services are giving [C90sXVMN2rE].webm")
# test_vpath = os.path.expanduser("~/Videos/30fps.mp4")
# test_vpath = "../assets/050.mp4"
# test_vpath = os.path.expanduser("~/Videos/iu/IU.mp4")
# test_vpath = "emma.mp4"


def test_cv2():
    reader = cv2.VideoCapture(test_vpath)
    n_frames = int(reader.get(cv2.CAP_PROP_FRAME_COUNT))
    fps = reader.get(cv2.CAP_PROP_FPS)
    print(n_frames, fps)
    for ifrm in trange(n_frames):
        with timeit("read"):
            ret, img = reader.read()
        if not ret:
            break
        tqdm.write(str(ifrm))
        # if ifrm < 20:
        #     cv2.imwrite("test_cv.png", img)
        # else:
        #     break

        # if ifrm > 7500:
        # cv2.imwrite("test_cv.png", img)
        # cv2.imshow('img', img)
        # cv2.waitKey(1)
    return ifrm


def test_ffutils():
    import videoio
    reader = videoio.VideoReader()
    reader.open(test_vpath)
    print(reader.image_size, reader.width, reader.height, reader.n_frames, reader.fps)

    # for i in range(100):
    #     print(reader.n_frames, reader.duration)
    #     idx = np.random.randint(0, reader.n_frames)
    #     msec = idx * 1000 / reader.fps
    #     with timeit("seek {}, {:.3f}".format(idx, msec)):
    #         reader.seek_msec(msec)
    #         ret, img = reader.read()
    #     # if ret:
    #     #     cv2.imshow('img', img)
    #     #     cv2.waitKey(0)

    # reader.seek_msec(7000 / reader.fps * 1000)
    reader.seek_frame(880)
    reader.read()
    print(reader.curr_iframe, reader.curr_msec)
    reader.seek_frame(100)
    reader.read()
    print(reader.curr_iframe, reader.curr_msec)
    quit()
    for ifrm in range(reader.n_frames):
        with timeit("read"):
            ret, img = reader.read()
        if not ret:
            break
        tqdm.write(str(ifrm))
        # if ifrm < 21:
        #     cv2.imwrite("test_ff.png", img)
        # else:
        #     break
        # if ifrm > 7500:
        # if ifrm > 390:
        #     cv2.imwrite("test_ff.png", img)
        # cv2.imshow('img', img)
        # cv2.waitKey(0)

# test_cv2()
test_ffutils()
