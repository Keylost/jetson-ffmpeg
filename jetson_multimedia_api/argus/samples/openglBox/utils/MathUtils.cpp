/*
 * Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "MathUtils.h"
#include <math.h>

void MathUtils::createFrustumMatrix(float left, float right, float bottom, float top,
                                    float znear, float zfar, float mat[16])
{
    float temp, temp2, temp3, temp4;
    temp = 2.0 * znear;
    temp2 = right - left;
    temp3 = top - bottom;
    temp4 = zfar - znear;
    mat[0] = temp / temp2;
    mat[1] = 0.0;
    mat[2] = 0.0;
    mat[3] = 0.0;
    mat[4] = 0.0;
    mat[5] = temp / temp3;
    mat[6] = 0.0;
    mat[7] = 0.0;
    mat[8] = (right + left) / temp2;
    mat[9] = (top + bottom) / temp3;
    mat[10] = (-zfar - znear) / temp4;
    mat[11] = -1.0;
    mat[12] = 0.0;
    mat[13] = 0.0;
    mat[14] = (-temp * zfar) / temp4;
    mat[15] = 0.0;
}

void MathUtils::createPerspectiveMatrix(float fovyInDegrees, float aspectRatio,
                                        float znear, float zfar, float mat[16])
{
    float ymax, xmax;
    ymax = znear * tanf(fovyInDegrees * M_PI / 360.0);
    xmax = ymax * aspectRatio;
    createFrustumMatrix(-xmax, xmax, -ymax, ymax, znear, zfar, mat);
}

void MathUtils::createRotationMatrix(float angle, float x, float y, float z, float mat[16])
{
    float mag, cosA, sinA, sqX, sqY, sqZ, xY, xZ, yZ;
    cosA = cosf(angle * M_PI / 180.0f);
    sinA = sinf(angle * M_PI / 180.0f);
    mag = sqrtf(x*x+y*y+z*z);
    x /= mag;
    y /= mag;
    z /= mag;
    sqX = x*x;
    sqY = y*y;
    sqZ = z*z;

    xY = x * y;
    xZ = x * z;
    yZ = y * z;

    mat[0] = sqX + (cosA * (1.0f - sqX));
    mat[1] = xY - (cosA * xY) + (sinA * z);
    mat[2] = xZ - (cosA * xZ) - (sinA * y);
    mat[3] = 0.0f;
    mat[4] = xY - (cosA * xY) - (sinA *z);
    mat[5] = sqY + (cosA * (1.0f - sqY));
    mat[6] = yZ - (cosA * yZ) + (sinA * x);
    mat[7] = 0.0f;
    mat[8] = xZ - (cosA * xZ) + (sinA * y);
    mat[9] = yZ - (cosA * yZ) - (sinA * x);
    mat[10] = sqZ + (cosA * (1.0f - sqZ));
    mat[11] = 0.0f;
    mat[12] = 0.0f;
    mat[13] = 0.0f;
    mat[14] = 0.0f;
    mat[15] = 1.0f;
}
