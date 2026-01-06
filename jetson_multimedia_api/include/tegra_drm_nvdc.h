/*
 * Copyright (c) 2018-2019, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _TEGRA_DRM_NVDC_H_
#define _TEGRA_DRM_NVDC_H_

#define fourcc_code_tegra(a,b,c,d) ((__u32)(a) | ((__u32)(b) << 8) | \
        ((__u32)(c) << 16) | ((__u32)(d) << 24))

/*
 * 2 plane YCrCb, 10 bits per channel
 * index 0 = Y plane, [15:0] Y
 * index 1 = Cr:Cb plane, [31:0] Cr:Cb
 *
 * Pixel packing:
 *
 *          Y plane
 * MS-Byte            LS-Byte
 * Y9Y8Y7Y6Y5Y4Y3Y2 | Y1Y0XXXXXX
 *
 *          UV plane
 * MS-Byte                                            LS-Byte
 * V9V8V7V6V5V4V3V2 | V1V0XXXXXX | U9U8U7U6U5U4U3U2 | U1U0XXXXXX
 */

#define DRM_FORMAT_TEGRA_P010_709 fourcc_code_tegra('H', 'D', '0', '1') /* 2x2 subsampled Cr:Cb plane BT.709 */
#define DRM_FORMAT_TEGRA_P010_2020 fourcc_code_tegra('U', 'H', 'D', '0') /* 2x2 subsampled Cr:Cb plane BT.2020 */


struct drm_tegra_hdr_metadata_smpte_2086 {
    // idx 0 : G, 1 : B, 2 : R
    __u16 display_primaries_x[3];          // normalized x chromaticity cordinate. It shall be in the range of 0 to 50000
    __u16 display_primaries_y[3];          // normalized y chromaticity cordinate. It shall be in the range of 0 to 50000
    __u16 white_point_x;                   // normalized x chromaticity cordinate of white point of mastering display
    __u16 white_point_y;                   // normalized y chromaticity cordinate of white point of mastering display
    __u32 max_display_parameter_luminance; // nominal maximum display luminance in units of 0.0001 candelas per square metre
    __u32 min_display_parameter_luminance; // nominal minimum display luminance in units of 0.0001 candelas per square metre
};

#endif
