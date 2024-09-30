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
| JetPack 6.0.x | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: |
| JetPack 6.1.x | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_orange_diamond: | :large_blue_circle: | :large_blue_circle: | :large_blue_circle: |

### FFmpeg support list

  - Currently the library is compatible with FFmpeg 4.2, 4.4 and 6.0.

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
	
**2.patch ffmpeg and build**

    clone one of supported ffmpeg versions
    git clone git://source.ffmpeg.org/ffmpeg.git -b release/4.2 --depth=1
    or
    git clone git://source.ffmpeg.org/ffmpeg.git -b release/4.4 --depth=1
    or
    git clone git://source.ffmpeg.org/ffmpeg.git -b release/6.0 --depth=1
    cd ffmpeg
    get and apply patch for your ffmpeg version
    wget -O ffmpeg_nvmpi.patch https://github.com/Keylost/jetson-ffmpeg/raw/master/ffmpeg_patches/ffmpeg4.2_nvmpi.patch
    or
    wget -O ffmpeg_nvmpi.patch https://github.com/Keylost/jetson-ffmpeg/raw/master/ffmpeg_patches/ffmpeg4.4_nvmpi.patch
    or
    wget -O ffmpeg_nvmpi.patch https://github.com/Keylost/jetson-ffmpeg/raw/master/ffmpeg_patches/ffmpeg6.0_nvmpi.patch
    git apply ffmpeg_nvmpi.patch
    ./configure --enable-nvmpi
    make
    sudo make install
    
**3.using**
  
**Decode h264 video example**

    ffmpeg -c:v h264_nvmpi -i <input.mp4> -f null -
  
**Encode h264 video example**

    ffmpeg -i <input.mp4> -c:v h264_nvmpi <output.mp4>

**Transcode h264 to h265 video example**

    ffmpeg -c:v h264_nvmpi -i <input.mp4> -c:v h265_nvmpi <output.mp4>
