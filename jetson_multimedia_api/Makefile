###############################################################################
#
# Copyright (c) 2016-2020, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###############################################################################

SUBDIRS = \
	samples/00_video_decode \
	samples/01_video_encode \
	samples/02_video_dec_cuda \
	samples/03_video_cuda_enc \
	samples/04_video_dec_trt \
	samples/05_jpeg_encode \
	samples/06_jpeg_decode \
	samples/07_video_convert \
	samples/08_video_dec_drm \
	samples/09_camera_jpeg_capture \
	samples/10_camera_recording \
	samples/12_camera_v4l2_cuda \
	samples/13_multi_camera \
	samples/14_multivideo_decode \
	samples/15_multivideo_encode \
	samples/16_multivideo_transcode \
	samples/backend \
	samples/frontend \
	samples/v4l2cuda \
	samples/unittest_samples/decoder_unit_sample \
	samples/unittest_samples/encoder_unit_sample \
	samples/unittest_samples/transform_unit_sample \
	samples/unittest_samples/camera_unit_sample

.PHONY: all
all:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		echo "Make in $$subdir";\
		$(MAKE) -C $$subdir;\
		if [ $$? -ne 0 ]; then exit 1; fi;\
	done

.PHONY: clean
clean:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
		echo "Clean in $$subdir";\
		$(MAKE) -C $$subdir clean;\
	done
