//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//--------------------------------------------------------------------------------------
// File: CommonConstants.h
//
// Common constants for the TiledLighting11 sample.
//--------------------------------------------------------------------------------------

#pragma once

#include "..\\..\\DXUT\\Core\\DXUT.h"

namespace TiledLighting11
{
    static const int MAX_NUM_LIGHTS = 2*1024;
    static const int MAX_NUM_GRID_OBJECTS = 280;

    // These must match their counterparts in CommonHeader.h
    static const int MAX_NUM_SHADOWCASTING_POINTS = 12;
    static const int MAX_NUM_SHADOWCASTING_SPOTS = 12;

    // no MSAA, 2x MSAA, and 4x MSAA
    enum MSAASetting
    {
        MSAA_SETTING_NO_MSAA = 0,
        MSAA_SETTING_2X_MSAA,
        MSAA_SETTING_4X_MSAA,
        NUM_MSAA_SETTINGS
    };
    static const int g_nMSAASampleCount[NUM_MSAA_SETTINGS] = {1,2,4};

} // namespace TiledLighting11

//--------------------------------------------------------------------------------------
// EOF
//--------------------------------------------------------------------------------------
