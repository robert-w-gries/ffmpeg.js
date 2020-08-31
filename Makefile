# Compile FFmpeg and all its dependencies to JavaScript.
# You need emsdk environment installed and activated, see:
# <https://kripken.github.io/emscripten-site/docs/getting_started/downloads.html>.

COMMON_FILTERS = aresample scale crop overlay hstack vstack
COMMON_DEMUXERS = matroska ogg mov mp3 wav image2 concat
COMMON_DECODERS = vp8 h264 vorbis opus mp3 aac pcm_s16le mjpeg png

LIBS = \
	build/ffmpeg/libavutil/libavutil.a \
    build/ffmpeg/libavcodec/libavcodec.a \
    build/ffmpeg/libavfilter/libavfilter.a \
    build/ffmpeg/libavformat/libavformat.a \
	build/ffmpeg/libswresample/libswresample.a

DECODERS = mp3
FFMPEG = build/ffmpeg/libavcodec/libavcodec.a

WEBM_MUXERS = webm ogg null
WEBM_ENCODERS = libvpx_vp8 libopus
FFMPEG_WEBM_BC = build/ffmpeg-webm/ffmpeg.bc
FFMPEG_WEBM_PC_PATH = ../opus/dist/lib/pkgconfig
WEBM_SHARED_DEPS = \
	build/opus/dist/lib/libopus.so \
	build/libvpx/dist/lib/libvpx.so

MP4_MUXERS = mp4 mp3 null
MP4_ENCODERS = libx264 libmp3lame aac
FFMPEG_MP4_BC = build/ffmpeg-mp4/ffmpeg.bc
FFMPEG_MP4_PC_PATH = ../x264/dist/lib/pkgconfig
MP4_SHARED_DEPS = \
	build/lame/dist/lib/libmp3lame.so \
	build/x264/dist/lib/libx264.so

all: ffmpeg
ffmpeg: ffmpeg.js

clean: clean-js \
	clean-opus clean-libvpx clean-ffmpeg \
	clean-lame clean-x264
clean-js:
	rm -f ffmpeg*.js
clean-opus:
	cd build/opus && git clean -xdf
clean-libvpx:
	cd build/libvpx && git clean -xdf
clean-lame:
	cd build/lame && git clean -xdf
clean-x264:
	cd build/x264 && git clean -xdf
clean-ffmpeg:
	cd build/ffmpeg && git clean -xdf

build/opus/configure:
	cd build/opus && ./autogen.sh

build/opus/dist/lib/libopus.so: build/opus/configure
	cd build/opus && \
	emconfigure ./configure \
		CFLAGS=-O3 \
		--prefix="$$(pwd)/dist" \
		--disable-static \
		--disable-doc \
		--disable-extra-programs \
		--disable-asm \
		--disable-rtcd \
		--disable-intrinsics \
		--disable-hardening \
		--disable-stack-protector \
		&& \
	emmake make -j && \
	emmake make install

build/libvpx/dist/lib/libvpx.so:
	cd build/libvpx && \
	git reset --hard && \
	patch -p1 < ../libvpx-fix-ld.patch && \
	emconfigure ./configure \
		--prefix="$$(pwd)/dist" \
		--target=generic-gnu \
		--disable-dependency-tracking \
		--disable-multithread \
		--disable-runtime-cpu-detect \
		--enable-shared \
		--disable-static \
		\
		--disable-examples \
		--disable-docs \
		--disable-unit-tests \
		--disable-webm-io \
		--disable-libyuv \
		--disable-vp8-decoder \
		--disable-vp9 \
		&& \
	emmake make -j && \
	emmake make install

build/lame/dist/lib/libmp3lame.so:
	cd build/lame/lame && \
	git reset --hard && \
	patch -p2 < ../../lame-fix-ld.patch && \
	emconfigure ./configure \
		CFLAGS="-DNDEBUG -O3" \
		--prefix="$$(pwd)/../dist" \
		--host=x86-none-linux \
		--disable-static \
		\
		--disable-gtktest \
		--disable-analyzer-hooks \
		--disable-decoder \
		--disable-frontend \
		&& \
	emmake make -j && \
	emmake make install

build/x264/dist/lib/libx264.so:
	cd build/x264 && \
	emconfigure ./configure \
		--prefix="$$(pwd)/dist" \
		--extra-cflags="-Wno-unknown-warning-option" \
		--host=x86-none-linux \
		--disable-cli \
		--enable-shared \
		--disable-opencl \
		--disable-thread \
		--disable-interlaced \
		--bit-depth=8 \
		--chroma-format=420 \
		--disable-asm \
		\
		--disable-avs \
		--disable-swscale \
		--disable-lavf \
		--disable-ffms \
		--disable-gpac \
		--disable-lsmash \
		&& \
	emmake make -j && \
	emmake make install

# TODO(Kagami): Emscripten documentation recommends to always use shared
# libraries but it's not possible in case of ffmpeg because it has
# multiple declarations of `ff_log2_tab` symbol. GCC builds FFmpeg fine
# though because it uses version scripts and so `ff_log2_tag` symbols
# are not exported to the shared libraries. Seems like `emcc` ignores
# them. We need to file bugreport to upstream. See also:
# - <https://kripken.github.io/emscripten-site/docs/compiling/Building-Projects.html>
# - <https://github.com/kripken/emscripten/issues/831>
# - <https://ffmpeg.org/pipermail/libav-user/2013-February/003698.html>
FFMPEG_COMMON_ARGS = \
	--cc=emcc \
	--ranlib=emranlib \
	--enable-cross-compile \
	--target-os=none \
	--arch=x86 \
	--disable-runtime-cpudetect \
	--disable-asm \
	--disable-fast-unaligned \
	--disable-pthreads \
	--disable-w32threads \
	--disable-os2threads \
	--disable-debug \
	--disable-stripping \
	--disable-programs \
	\
    --disable-everything \
    --disable-network \
    --disable-autodetect \
    --enable-small \
    --enable-decoder=mp3 \
    --enable-parser=mp3 \
    --enable-demuxer=mp3 \
    --enable-protocol=file

build/ffmpeg/libavcodec/libavcodec.a:
	cd build/ffmpeg && \
	emconfigure ./configure \
		$(FFMPEG_COMMON_ARGS) \
		$(addprefix --enable-decoder=,$(DECODERS)) \
		&& \
	emmake make -j

EMCC_COMMON_ARGS = \
	--closure 1 \
	-Os \
	-s SINGLE_FILE=1 \
	-s MODULARIZE=1 \
	-s EXPORT_ES6=1 \
	-s USE_ES6_IMPORT_META=0 \
	-s ENVIRONMENT=web \
	-s ALLOW_MEMORY_GROWTH=1 \
    -s MALLOC=emmalloc \
	-Ibuild/ffmpeg/ \
	-o $@

ffmpeg.js: $(FFMPEG)
	emcc --bind $(LIBS) build/bindings.cpp $(EMCC_COMMON_ARGS)