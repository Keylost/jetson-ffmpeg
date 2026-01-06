# Copyright (c) 2015-2017, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

include ../Rules.mk

APP := capture-cuda

SRCS := \
	capture.cpp \
	yuv2rgb.cu

ALL_CPPFLAGS := $(addprefix -Xcompiler ,$(filter-out -std=c++11, $(CPPFLAGS)))

# CUDA code generation flags
GENCODE_SM53 := -gencode arch=compute_53,code=sm_53
GENCODE_SM62 := -gencode arch=compute_62,code=sm_62
GENCODE_SM72 := -gencode arch=compute_72,code=sm_72
GENCODE_SM_PTX := -gencode arch=compute_72,code=compute_72
GENCODE_FLAGS := $(GENCODE_SM53) $(GENCODE_SM62) $(GENCODE_SM72) $(GENCODE_SM_PTX)

all: $(APP)

capture.o: capture.cpp
	@echo "Compiling: $<"
	$(CPP) $(CPPFLAGS) -c $<

yuv2rgb.o: yuv2rgb.cu
	@echo "Compiling: $<"
	$(NVCC) $(ALL_CPPFLAGS) $(GENCODE_FLAGS) -c $<

$(APP): capture.o yuv2rgb.o
	@echo "Linking: $@"
	$(CPP) -o $@ $^ $(CPPFLAGS) $(LDFLAGS)

clean:
	$(AT) rm -f *.o $(APP)
