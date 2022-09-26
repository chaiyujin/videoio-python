#!/bin/bash
# ---------------------------------------------
# A easy bash script to install ffmpeg.
# Please Read official guide for more details.
# https://trac.ffmpeg.org/wiki/CompilationGuide
# ---------------------------------------------

set -o errexit

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Get version and install dir.
if [ -z $FFMPEG_VERSION ]; then FFMPEG_VERSION=5.1.1; fi
if [ -z $FFMPEG_HOME    ]; then FFMPEG_HOME="$HOME/ffmpeg_build"; fi
echo -e "[notice] FFmpeg $GREEN$FFMPEG_VERSION$NC will be installed at '$GREEN$FFMPEG_HOME$NC'."
if [ -f $FFMPEG_HOME/bin/ffmpeg ]; then
  echo -e "[error]  '$RED$FFMPEG_HOME/bin/ffmpeg$NC' is found! Please set '${GREEN}FFMPEG_HOME$NC' to another dir."
  exit 1;
fi

mkdir -p $FFMPEG_HOME

# Download and unzip.
URL="https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz"
FILE="$(dirname $FFMPEG_HOME)/ffmpeg-$FFMPEG_VERSION.tar.xz"
SRC_DIR="$(dirname $FFMPEG_HOME)/ffmpeg-$FFMPEG_VERSION"
if [ ! -f "$FFMPEG_HOME/unzip.done" ] || [ ! -d $SRC_DIR ]; then
  wget -c $URL -O $FILE
  echo "[notice] Unzip the downloaded package."
  tar -Jxf $FILE -C $(dirname $FFMPEG_HOME)
  if [ ! -f "$FFMPEG_HOME/unzip.done" ]; then
    touch $FFMPEG_HOME/unzip.done
  fi
fi

# > Step 1: Install dependency by 'apt'
sudo apt install \
  nasm libass-dev libfdk-aac-dev libmp3lame-dev \
  libopus-dev libvorbis-dev libvpx-dev libx264-dev libx265-dev \
;

# > Step 2: Options.
CODEC_OPTS=()
CODEC_OPTS+=("--enable-gpl")      # libs under GPL license.
CODEC_OPTS+=("--enable-nonfree")  # Non-free libs.
CODEC_OPTS+=("--enable-libass")
CODEC_OPTS+=("--enable-libfdk-aac")
CODEC_OPTS+=("--enable-libfreetype")
CODEC_OPTS+=("--enable-libmp3lame")
CODEC_OPTS+=("--enable-libopus")
CODEC_OPTS+=("--enable-libvorbis")
CODEC_OPTS+=("--enable-libvpx")
CODEC_OPTS+=("--enable-libx264")
CODEC_OPTS+=("--enable-libx265")

# > Step3: Build & Install
# make sure the directory is correct.
cd $SRC_DIR && \
PATH="$FFMPEG_HOME/bin:$PATH" \
PKG_CONFIG_PATH="$FFMPEG_HOME/lib/pkgconfig" \
pkg_config='pkg-config --static' \
./configure \
  --prefix="$FFMPEG_HOME" \
  --pkg-config-flags="--static" \
  --extra-cflags="-I$FFMPEG_HOME/include" \
  --extra-ldflags="-L$FFMPEG_HOME/lib" \
  --extra-libs="-lpthread -lm" \
  --bindir="$FFMPEG_HOME/bin" \
  --cc=gcc --host-cflags= --host-ldflags= \
  --enable-static --enable-shared --enable-pthreads \
  ${CODEC_OPTS[@]} && \
PATH="$FFMPEG_HOME/bin:$PATH" make -j8 && \
make install
