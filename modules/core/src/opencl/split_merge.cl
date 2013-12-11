/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2010-2012, Institute Of Software Chinese Academy Of Science, all rights reserved.
// Copyright (C) 2010-2012, Advanced Micro Devices, Inc., all rights reserved.
// Copyright (C) 2013, OpenCV Foundation, all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors as is and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the copyright holders or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#ifdef OP_MERGE

#define DECLARE_SRC_PARAM(index) __global const uchar * src##index##ptr, int src##index##_step, int src##index##_offset,
#define DECLARE_DATA(index) __global const T * src##index = \
    (__global T *)(src##index##ptr + mad24(src##index##_step, y, x * (int)sizeof(T) + src##index##_offset));
#define PROCESS_ELEM(index) dst[index] = src##index[0];

__kernel void merge(DECLARE_SRC_PARAMS_N
                    __global uchar * dstptr, int dst_step, int dst_offset,
                    int rows, int cols)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if (x < cols && y < rows)
    {
        DECLARE_DATA_N
        __global T * dst = (__global T *)(dstptr + mad24(dst_step, y, x * (int)sizeof(T) * cn + dst_offset));
        PROCESS_ELEMS_N
    }
}

#elif defined OP_SPLIT

#define DECLARE_DST_PARAM(index) , __global uchar * dst##index##ptr, int dst##index##_step, int dst##index##_offset
#define DECLARE_DATA(index) __global T * dst##index = \
    (__global T *)(dst##index##ptr + mad24(y, dst##index##_step, x * (int)sizeof(T) + dst##index##_offset));
#define PROCESS_ELEM(index) dst##index[0] = src[index];

__kernel void split(__global uchar* srcptr, int src_step, int src_offset, int rows, int cols DECLARE_DST_PARAMS)
{
    int x = get_global_id(0);
    int y = get_global_id(1);

    if (x < cols && y < rows)
    {
        DECLARE_DATA_N
        __global const T * src = (__global const T *)(srcptr + mad24(y, src_step, x *  cn * (int)sizeof(T) + src_offset));
        PROCESS_ELEMS_N
    }
}

#else
#error "No operation"
#endif
