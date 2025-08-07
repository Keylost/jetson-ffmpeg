# jetson-ffmpeg
L4T Multimedia API for ffmpeg.  
This library provides the ability to use hardware acceleration for video encoding and decoding on Nvidia Jetson platforms with the FFmpeg multimedia framework.

### Jetson/JetPack support table
  - :white_check_mark: - Fully supported.
  - :large_blue_circle: - Not tested.
  - :x: - Not supported.
  - :large_orange_diamond: - There is no JetPack version available for this platform.
    
| 			    | TK1 | TX1 | TX2 | TX2i | Nano | AGX Xavier | Xavier NX | AGX Orin | Orin NX | Orin Nano |
| ------------- | --- | --- | --- | ---- | ----	| ---------	 | --------- | -------- | ------- | --------- |
| JetPack 1.0.x | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 1.1.x | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 1.2.x | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 2.0.x | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 2.1.x | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 2.2.x | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 2.3.x | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 3.0.x | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 3.1.x | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 3.2.x | :large_orange_diamond: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 3.3.x | :large_orange_diamond: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 4.1.x | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 4.2.x | :large_orange_diamond: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 4.3.x | :large_orange_diamond: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 4.4.x | :large_orange_diamond: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 4.5.x | :large_orange_diamond: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :white_check_mark: | :white_check_mark: | :white_check_mark: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 4.6.x | :large_orange_diamond: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: | :white_check_mark: | :white_check_mark: | :white_check_mark: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 5.0.x | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :white_check_mark: | :white_check_mark: | :white_check_mark: | :large_orange_diamond: | :large_orange_diamond: |
| JetPack 5.1.x | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: |
| JetPack 6.0.x | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :white_check_mark: | :white_check_mark: | :white_check_mark: |
| JetPack 6.1.x | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :white_check_mark: | :white_check_mark: | :white_check_mark: |
| JetPack 6.2.x | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :white_check_mark: | :white_check_mark: | :white_check_mark: |

### FFmpeg support list

  - The library is currently compatible with all FFmpeg versions from 4.2 to 7.1+.
  - It may also work with versions older than 4.2, but it has not been tested.

### Supports Decoding
  - H.264/AVC (ffmpeg codec name: h264_nvmpi)
  - H.265/HEVC (ffmpeg codec name: hevc_nvmpi)
  - MPEG2 (ffmpeg codec name: mpeg2_nvmpi)
  - MPEG4 (ffmpeg codec name: mpeg4_nvmpi)
  - VP8 (ffmpeg codec name: vp8_nvmpi)
  - VP9 (ffmpeg codec name: vp9_nvmpi)
  
### Supports Encoding
  - H.264/AVC (ffmpeg codec name: h264_nvmpi)
  - H.265/HEVC (ffmpeg codec name: hevc_nvmpi)
  
### Other Features
  - Hardware accelerated video scaling during decoding

### Building and usage
**1.build and install library**

    git clone https://github.com/Keylost/jetson-ffmpeg.git
    cd jetson-ffmpeg
    mkdir build
    cd build
    cmake ..
    make
    sudo make install
    sudo ldconfig

Cmake options:
  - -DJETSON_MULTIMEDIA_API_DIR=<path_to_dir> Path to custom Jetson Multimedia API headers and common sources directory. Default: /usr/src/jetson_multimedia_api.
  - -DJETSON_MULTIMEDIA_LIB_DIR=<path_to_dir> Path to custom Jetson Multimedia libraries directory. Default: /usr/lib/aarch64-linux-gnu/tegra.
  - -DCUDA_INCLUDE_DIR=<path_to_dir> Path to custom CUDA headers directory. Default: /usr/local/cuda/include.
  - -DCUDA_LIB_DIR=<path_to_dir> Path to custom CUDA libraries directory. Default: /usr/local/cuda/lib64.
  - -DWITH_STUBS=[ON/OFF] Build nvmpi library and link stubs instead of original libraries. Default: OFF. Could be user to create automated builds or Docker images. See https://github.com/Keylost/jetson-ffmpeg/pull/9 for details and script example.

Build with stubs and custom dirs example:

    cmake -DWITH_STUBS=ON -DJETSON_MULTIMEDIA_API_DIR=/home/user/build_deps/jetson_multimedia_api ..
    make

**2.patch ffmpeg and build**

    clone one of supported ffmpeg versions (for example ffmpeg 7.1)
    git clone git://source.ffmpeg.org/ffmpeg.git -b release/7.1 --depth=1
    Go to the directory with the jetson-ffmpeg sources and patch the ffmpeg using the ffpatch.sh script.
    cd jetson-ffmpeg
    ./ffpatch.sh ../ffmpeg
    Go to ffmpeg sources directory configure and build ffmpeg with nvmpi enabled and your custom options 
    cd ../ffmpeg
    ./configure --enable-nvmpi
    make
    sudo make install
    
**3.using**
  
**Decode h264 video example**

    ffmpeg -c:v h264_nvmpi -i <input.mp4> -f null -
    
**Decode h264 video with fast scaling during decoding example**

    ffmpeg -c:v h264_nvmpi -resize:v 1920x1080 -i <input.mp4> -f null -
  
**Encode h264 video example**

    ffmpeg -i <input.mp4> -c:v h264_nvmpi <output.mp4>

**Transcode h264 to h265 video example**

    ffmpeg -c:v h264_nvmpi -i <input.mp4> -c:v hevc_nvmpi <output.mp4>
