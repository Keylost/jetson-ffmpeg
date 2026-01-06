#!/usr/bin/env python2
###############################################################################
#
# Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
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

import argparse
import os.path

parser = argparse.ArgumentParser(description="Get the RGBA value of each pixel in given image")
parser.add_argument('-i', '--image', type=str, required=True, help="The image path")
parser.add_argument('-o', '--output', type=str, required=True, help="The output file used to store the RGBA data")
args = parser.parse_args()

if os.path.isfile(args.image):
    try:
        from PIL import Image
    except ImportError:
        print "WARN: don't error out if PIL is not installed, instead to create a color pattern."
        print "Alternatively, you can install it by <sudo apt-get install python-imaging>"
        file = open(args.output, 'w')
        file.write('\n\n//Auto-generated file! Do not edit it!\n\n')
        file.write('unsigned int image_w = 640;\n')
        file.write('unsigned int image_h = 480;\n')
        file.write('char image_pixels_array[] = {\n')
        for h in range(480):
            for w in range(640):
                b = w % 255;
                g = h % 255;
                r = (w+ h) % 255;
                file.write(str(b)+', ' + str(g) +', ' + str(r) +', 200,\n')
        file.write('};');
        file.close()
        quit()

    # Get pixel values from the input image and store the result into header file
    im = Image.open(args.image)
    pix = im.load()
    dim = im.size
    print "The image size is " + str(dim[0]) + " x " + str(dim[1])
    file = open(args.output, 'w')
    file.write('\n\n//Auto-generated file! Do not edit it!\n\n')
    file.write('unsigned int image_w = ' + str(dim[0]) + ';\n')
    file.write('unsigned int image_h = ' + str(dim[1]) + ';\n')
    file.write('char image_pixels_array[] = {\n')
    for h in range(dim[1]):
        for w in range(dim[0]):
            # Pixel values are ranged by 'R, G, B, A'
            # And alpha value is fixed as 120
            file.write(str(pix[w, h][0]) + \
                    ', ' + str(pix[w, h][1]) + \
                    ', ' + str(pix[w, h][2]) + \
                    ', 120,' + '\n')
    file.write('};');
    file.close()
else:
    print "ERROR: the image file is not existing"
