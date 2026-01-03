#!/bin/bash
#path to root ffmpeg soyrces dir must be passed as first arg to the script

FF_DIR_ROOT=${1}
FF_DIR_LIBAVCODEC=${FF_DIR_ROOT}"/libavcodec"
FF_FILE_CONFIGURE=${FF_DIR_ROOT}"/configure"
FF_FILE_LIBAVCODEC_MAKEFILE=${FF_DIR_LIBAVCODEC}"/Makefile"
FF_FILE_LIBAVCODEC_ALLCODECSC=${FF_DIR_LIBAVCODEC}"/allcodecs.c"
FF_FILE_LIBAVCODEC_VERSIONH=${FF_DIR_LIBAVCODEC}"/version.h"
FF_FILE_LIBAVCODEC_VERSIONMAJORH=${FF_DIR_LIBAVCODEC}"/version_major.h"
FF_LIBAVCODEC_VERSION_MAJOR=0
BKP_DIR=./bkp
BKP_FILE_CONFIGURE=${BKP_DIR}/configure
BKP_FILE_LIBAVCODEC_MAKEFILE=${BKP_DIR}/Makefile
BKP_FILE_LIBAVCODEC_ALLCODECSC=${BKP_DIR}/allcodecs.c

if [ ! -d "$FF_DIR_ROOT" ]; then
	echo "[E]: $FF_DIR_ROOT does not exist or not a directory. You must specify path to ffmpeg sources directory."
	exit 1
fi

if [ ! -d "$FF_DIR_LIBAVCODEC" ]; then
	echo "[E]: $FF_DIR_LIBAVCODEC does not exist or not a directory. Your ffmpeg sources are not complete or this ffmpeg version not supported by the script yet."
	exit 1
fi

if [ ! -f "$FF_FILE_CONFIGURE" ]; then
	echo "[E]: $FF_FILE_CONFIGURE does not exist or not a file. Your ffmpeg sources are not complete or this ffmpeg version not supported by the script yet."
	exit 1
fi

if [ ! -f "$FF_FILE_LIBAVCODEC_MAKEFILE" ]; then
	echo "[E]: $FF_FILE_LIBAVCODEC_MAKEFILE does not exist or not a file. Your ffmpeg sources are not complete or this ffmpeg version not supported by the script yet."
	exit 1
fi

if [ ! -f "$FF_FILE_LIBAVCODEC_ALLCODECSC" ]; then
	echo "[E]: $FF_FILE_LIBAVCODEC_ALLCODECSC does not exist or not a file. Your ffmpeg sources are not complete or this ffmpeg version not supported by the script yet."
	exit 1
fi

#read and check ffmpeg libavcodec version 
if [ -f "$FF_FILE_LIBAVCODEC_VERSIONMAJORH" ]; then
	FF_LIBAVCODEC_VERSION_MAJOR=$(grep '#define LIBAVCODEC_VERSION_MAJOR' $FF_FILE_LIBAVCODEC_VERSIONMAJORH | awk '{printf "%d",$3}')
elif [ -f "$FF_FILE_LIBAVCODEC_VERSIONH" ]; then
	FF_LIBAVCODEC_VERSION_MAJOR=$(grep '#define LIBAVCODEC_VERSION_MAJOR' $FF_FILE_LIBAVCODEC_VERSIONH | awk '{printf "%d",$3}')
else
	echo "[E]: Can't find ffmpeg libavcodec version file. Your ffmpeg sources are not complete or this ffmpeg version not supported by the script yet."
	exit 1
fi
if [ "$FF_LIBAVCODEC_VERSION_MAJOR" -eq 0 ]; then
	echo "[E]: Can't read ffmpeg libavcodec version. Your ffmpeg sources are not complete or this ffmpeg version not supported by the script yet."
	exit 1
fi

rm -rf "$BKP_DIR" 2>&1 > /dev/null
if ! mkdir "$BKP_DIR" 2>&1 > /dev/null; then
	echo "Can not create backup dir $BKP_DIR"
	exit 1
fi

cp "$FF_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE"
cp "$FF_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE"
cp "$FF_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC"

################## MODIFY configure ############################
function path_ff_configure ()
{
#add nvmpi to ffmpeg configure show_help() function.
if ! grep -q -- '--enable-nvmpi           enable nvmpi code \[no\]' "$BKP_FILE_CONFIGURE"; then
	cp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"
	sed -i '/--disable-videotoolbox/a \ \ --enable-nvmpi           enable nvmpi code [no]' "$BKP_FILE_CONFIGURE"
	if cmp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"; then return 1; fi;
fi

#add nvmpi to ffmpeg configure HWACCEL_LIBRARY_LIST.
if ! sed -n '/HWACCEL_LIBRARY_LIST="/,/^"/p' "$BKP_FILE_CONFIGURE" | grep -qx '[[:space:]]*nvmpi'; then
	cp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"
	sed -i '/HWACCEL_LIBRARY_LIST="/a \ \ \ \ nvmpi' "$BKP_FILE_CONFIGURE"
	if cmp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"; then return 1; fi;
fi

#add nvmpi avc/h264 deps. insert before h264_nvenc_encoder_deps="nvenc"
if ! grep -q 'h264_nvmpi_encoder_deps' "$BKP_FILE_CONFIGURE"; then
	cp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"
	sed -i '/h264_nvenc_encoder_deps="nvenc"/i h264_nvmpi_encoder_deps="nvmpi"\nh264_nvmpi_decoder_deps="nvmpi"\nh264_nvmpi_decoder_select="h264_mp4toannexb_bsf"' "$BKP_FILE_CONFIGURE"
	if cmp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"; then return 1; fi;
fi

#add nvmpi hevc/h265 deps. insert before hevc_nvenc_encoder_deps="nvenc"
if ! grep -q 'hevc_nvmpi_encoder_deps' "$BKP_FILE_CONFIGURE"; then
	cp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"
	sed -i '/hevc_nvenc_encoder_deps="nvenc"/i hevc_nvmpi_encoder_deps="nvmpi"\nhevc_nvmpi_decoder_deps="nvmpi"\nhevc_nvmpi_decoder_select="hevc_mp4toannexb_bsf"' "$BKP_FILE_CONFIGURE"
	if cmp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"; then return 1; fi;
fi

#add nvmpi mpeg2 deps. insert after mpeg2_cuvid_decoder_deps="cuvid"
if ! grep -q 'mpeg2_nvmpi_decoder_deps' "$BKP_FILE_CONFIGURE"; then
	cp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"
	sed -i '/mpeg2_cuvid_decoder_deps="cuvid"/a mpeg2_nvmpi_decoder_deps="nvmpi"' "$BKP_FILE_CONFIGURE"
	if cmp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"; then return 1; fi;
fi

#add nvmpi mpeg4 deps. insert after mpeg4_cuvid_decoder_deps="cuvid"
if ! grep -q 'mpeg4_nvmpi_decoder_deps' "$BKP_FILE_CONFIGURE"; then
	cp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"
	sed -i '/mpeg4_cuvid_decoder_deps="cuvid"/a mpeg4_nvmpi_decoder_deps="nvmpi"' "$BKP_FILE_CONFIGURE"
	if cmp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"; then return 1; fi;
fi

#add nvmpi vp8 deps. insert after vp8_cuvid_decoder_deps="cuvid"
if ! grep -q 'vp8_nvmpi_decoder_deps' "$BKP_FILE_CONFIGURE"; then
	cp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"
	sed -i '/vp8_cuvid_decoder_deps="cuvid"/a vp8_nvmpi_decoder_deps="nvmpi"' "$BKP_FILE_CONFIGURE"
	if cmp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"; then return 1; fi;
fi

#add nvmpi vp9 deps. insert after vp9_cuvid_decoder_deps="cuvid"
if ! grep -q 'vp9_nvmpi_decoder_deps' "$BKP_FILE_CONFIGURE"; then
	cp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"
	sed -i '/vp9_cuvid_decoder_deps="cuvid"/a vp9_nvmpi_decoder_deps="nvmpi"' "$BKP_FILE_CONFIGURE"
	if cmp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"; then return 1; fi;
fi

#insert before enabled libx264 line.
if ! grep -q 'enabled nvmpi' "$BKP_FILE_CONFIGURE"; then
	cp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"
	sed -i '/enabled libx264/i enabled nvmpi		  && require_pkg_config nvmpi nvmpi nvmpi.h nvmpi_create_decoder' "$BKP_FILE_CONFIGURE"
	if cmp "$BKP_FILE_CONFIGURE" "$BKP_FILE_CONFIGURE.1"; then return 1; fi;
fi

return 0;
}
################## MODIFY configure ############################

################## MODIFY libavcodec/Makefile ############################
function path_ff_libavcodec_Makefile ()
{
#add nvmpi avc/h264 encoder and decoder
if ! grep -q 'CONFIG_H264_NVMPI_DECODER' "$BKP_FILE_LIBAVCODEC_MAKEFILE"; then
	cp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"
	sed -i '/OBJS-\$(CONFIG_H264_NVENC_ENCODER)/i OBJS-\$(CONFIG_H264_NVMPI_DECODER)      += nvmpi_dec.o\nOBJS-$(CONFIG_H264_NVMPI_ENCODER)      += nvmpi_enc.o' "$BKP_FILE_LIBAVCODEC_MAKEFILE"
	if cmp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"; then return 1; fi;
fi

#add nvmpi hevc/h265 encoder and decoder
if ! grep -q 'CONFIG_HEVC_NVMPI_DECODER' "$BKP_FILE_LIBAVCODEC_MAKEFILE"; then
	cp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"
	sed -i '/OBJS-\$(CONFIG_HEVC_NVENC_ENCODER)/i OBJS-\$(CONFIG_HEVC_NVMPI_DECODER)      += nvmpi_dec.o\nOBJS-$(CONFIG_HEVC_NVMPI_ENCODER)      += nvmpi_enc.o' "$BKP_FILE_LIBAVCODEC_MAKEFILE"
	if cmp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"; then return 1; fi;
fi

#add nvmpi mpeg2 encoder and decoder
if ! grep -q 'CONFIG_MPEG2_NVMPI_DECODER' "$BKP_FILE_LIBAVCODEC_MAKEFILE"; then
	cp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"
	sed -i '/OBJS-\$(CONFIG_MPEG2_CUVID_DECODER)/i OBJS-\$(CONFIG_MPEG2_NVMPI_DECODER)      += nvmpi_dec.o' "$BKP_FILE_LIBAVCODEC_MAKEFILE"
	if cmp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"; then return 1; fi;
fi

#add nvmpi mpeg4 encoder and decoder
if ! grep -q 'CONFIG_MPEG4_NVMPI_DECODER' "$BKP_FILE_LIBAVCODEC_MAKEFILE"; then
	cp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"
	sed -i '/OBJS-\$(CONFIG_MPEG4_CUVID_DECODER)/i OBJS-\$(CONFIG_MPEG4_NVMPI_DECODER)      += nvmpi_dec.o' "$BKP_FILE_LIBAVCODEC_MAKEFILE"
	if cmp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"; then return 1; fi;
fi

#add nvmpi vp8 encoder and decoder
if ! grep -q 'CONFIG_VP8_NVMPI_DECODER' "$BKP_FILE_LIBAVCODEC_MAKEFILE"; then
	cp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"
	sed -i '/OBJS-\$(CONFIG_VP8_CUVID_DECODER)/i OBJS-\$(CONFIG_VP8_NVMPI_DECODER)      += nvmpi_dec.o' "$BKP_FILE_LIBAVCODEC_MAKEFILE"
	if cmp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"; then return 1; fi;
fi

#add nvmpi vp9 encoder and decoder
if ! grep -q 'CONFIG_VP9_NVMPI_DECODER' "$BKP_FILE_LIBAVCODEC_MAKEFILE"; then
	cp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"
	sed -i '/OBJS-\$(CONFIG_VP9_CUVID_DECODER)/i OBJS-\$(CONFIG_VP9_NVMPI_DECODER)      += nvmpi_dec.o' "$BKP_FILE_LIBAVCODEC_MAKEFILE"
	if cmp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$BKP_FILE_LIBAVCODEC_MAKEFILE.1"; then return 1; fi;
fi

return 0;
}
################## MODIFY libavcodec/Makefile ############################

################## MODIFY libavcodec/allcodecs.c ############################
function path_ff_libavcodec_allcodecsc ()
{
FF_CODEC_INTERFACE="extern AVCodec"
if [ "$FF_LIBAVCODEC_VERSION_MAJOR" -gt 59 ]; then
	FF_CODEC_INTERFACE="extern const FFCodec"
fi

#add nvmpi avc/h264 encoder and decoder
if ! grep -q 'ff_h264_nvmpi_decoder' "$BKP_FILE_LIBAVCODEC_ALLCODECSC"; then
	cp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"
	sed -i "/$FF_CODEC_INTERFACE ff_h264_decoder;/a $FF_CODEC_INTERFACE ff_h264_nvmpi_decoder;\n$FF_CODEC_INTERFACE ff_h264_nvmpi_encoder;" "$BKP_FILE_LIBAVCODEC_ALLCODECSC"
	if cmp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"; then return 1; fi;
fi

#add nvmpi hevc/h265 encoder and decoder
if ! grep -q 'ff_hevc_nvmpi_decoder' "$BKP_FILE_LIBAVCODEC_ALLCODECSC"; then
	cp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"
	sed -i "/$FF_CODEC_INTERFACE ff_hevc_decoder;/a $FF_CODEC_INTERFACE ff_hevc_nvmpi_decoder;\n$FF_CODEC_INTERFACE ff_hevc_nvmpi_encoder;" "$BKP_FILE_LIBAVCODEC_ALLCODECSC"
	if cmp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"; then return 1; fi;
fi

#add nvmpi mpeg2 encoder and decoder
if ! grep -q 'ff_mpeg2_nvmpi_decoder' "$BKP_FILE_LIBAVCODEC_ALLCODECSC"; then
	cp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"
	sed -i "/$FF_CODEC_INTERFACE ff_mpeg2video_decoder;/a $FF_CODEC_INTERFACE ff_mpeg2_nvmpi_decoder;" "$BKP_FILE_LIBAVCODEC_ALLCODECSC"
	if cmp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"; then return 1; fi;
fi

#add nvmpi mpeg4 encoder and decoder
if ! grep -q 'ff_mpeg4_nvmpi_decoder' "$BKP_FILE_LIBAVCODEC_ALLCODECSC"; then
	cp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"
	sed -i "/$FF_CODEC_INTERFACE ff_mpeg4_decoder;/a $FF_CODEC_INTERFACE ff_mpeg4_nvmpi_decoder;" "$BKP_FILE_LIBAVCODEC_ALLCODECSC"
	if cmp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"; then return 1; fi;
fi

#add nvmpi vp8 encoder and decoder
if ! grep -q 'ff_vp8_nvmpi_decoder' "$BKP_FILE_LIBAVCODEC_ALLCODECSC"; then
	cp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"
	sed -i "/$FF_CODEC_INTERFACE ff_vp8_decoder;/a $FF_CODEC_INTERFACE ff_vp8_nvmpi_decoder;" "$BKP_FILE_LIBAVCODEC_ALLCODECSC"
	if cmp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"; then return 1; fi;
fi

#add nvmpi vp9 encoder and decoder
if ! grep -q 'ff_vp9_nvmpi_decoder' "$BKP_FILE_LIBAVCODEC_ALLCODECSC"; then
	cp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"
	sed -i "/$FF_CODEC_INTERFACE ff_vp9_decoder;/a $FF_CODEC_INTERFACE ff_vp9_nvmpi_decoder;" "$BKP_FILE_LIBAVCODEC_ALLCODECSC"
	if cmp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$BKP_FILE_LIBAVCODEC_ALLCODECSC.1"; then return 1; fi;
fi

return 0;
}
################## MODIFY libavcodec/allcodecs.c ############################

if path_ff_configure 2>&1 > /dev/null; then echo "$FF_FILE_CONFIGURE is successfully patched!"; else echo "Patching $FF_FILE_CONFIGURE failed!"; exit 1; fi;
if path_ff_libavcodec_Makefile 2>&1 > /dev/null; then echo "$FF_FILE_LIBAVCODEC_MAKEFILE is successfully patched!"; else echo "Patching $FF_FILE_LIBAVCODEC_MAKEFILE failed!"; exit 1; fi;
if path_ff_libavcodec_allcodecsc 2>&1 > /dev/null; then echo "$FF_FILE_LIBAVCODEC_ALLCODECSC is successfully patched!"; else echo "Patching $FF_FILE_LIBAVCODEC_ALLCODECSC failed!"; exit 1; fi;

cp "$BKP_FILE_CONFIGURE" "$FF_FILE_CONFIGURE"
cp "$BKP_FILE_LIBAVCODEC_MAKEFILE" "$FF_FILE_LIBAVCODEC_MAKEFILE"
cp "$BKP_FILE_LIBAVCODEC_ALLCODECSC" "$FF_FILE_LIBAVCODEC_ALLCODECSC"

#copy nvmpi enc and dec files to ffmpeg libavcodec dir
cp ffmpeg_dev/common/libavcodec/nvmpi_dec.c ${FF_DIR_LIBAVCODEC}"/nvmpi_dec.c"
cp ffmpeg_dev/common/libavcodec/nvmpi_enc.c ${FF_DIR_LIBAVCODEC}"/nvmpi_enc.c"

echo "Success!"

rm -rf "$BKP_DIR" 2>&1 > /dev/null

exit 0
