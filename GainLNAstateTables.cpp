/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Charles J. Cliffe
 * Copyright (c) 2021 Franco Venturi - gain API redesign

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* LNA state gain reduction tables (API Specifications - chapter 5) */


#include "SoapySDRPlay.hpp"
#include <cfloat>

#if defined(_M_X64) || defined(_M_IX86)
#define strcasecmp _stricmp
#elif defined (__GNUC__)
#include <strings.h>
#endif


void SoapySDRPlay::updateRSP1LNAstateGainReductions()
{
    static int LNAstateGR0[] = { 0, 24, 19, 43 };
    static int LNAstateGR1[] = { 0,  7, 19, 26 };
    static int LNAstateGR2[] = { 0,  5, 19, 24 };

    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    if (rfHz < 420e6) {
        LNAstateGainReductions = LNAstateGR0;
        maxLNAstate = sizeof(LNAstateGR0) / sizeof(LNAstateGR0[0]) - 1;
    } else if (rfHz < 1000e6) {
        LNAstateGainReductions = LNAstateGR1;
        maxLNAstate = sizeof(LNAstateGR1) / sizeof(LNAstateGR1[0]) - 1;
    } else if (rfHz <= 2000e6) {
        LNAstateGainReductions = LNAstateGR2;
        maxLNAstate = sizeof(LNAstateGR2) / sizeof(LNAstateGR2[0]) - 1;
    }
    return;
}

void SoapySDRPlay::updateRSP1ALNAstateGainReductions()
{
    static int LNAstateGR0[] = { 0, 6, 12, 18, 37, 42, 61 };
    static int LNAstateGR1[] = { 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 };
    static int LNAstateGR2[] = { 0, 7, 13, 19, 20, 27, 33, 39, 45, 64 };
    static int LNAstateGR3[] = { 0, 6, 12, 20, 26, 32, 38, 43, 62 };

    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    if (rfHz < 60e6) {
        LNAstateGainReductions = LNAstateGR0;
        maxLNAstate = sizeof(LNAstateGR0) / sizeof(LNAstateGR0[0]) - 1;
    } else if (rfHz < 420e6) {
        LNAstateGainReductions = LNAstateGR1;
        maxLNAstate = sizeof(LNAstateGR1) / sizeof(LNAstateGR1[0]) - 1;
    } else if (rfHz < 1000e6) {
        LNAstateGainReductions = LNAstateGR2;
        maxLNAstate = sizeof(LNAstateGR2) / sizeof(LNAstateGR2[0]) - 1;
    } else if (rfHz <= 2000e6) {
        LNAstateGainReductions = LNAstateGR3;
        maxLNAstate = sizeof(LNAstateGR3) / sizeof(LNAstateGR3[0]) - 1;
    }
    return;
}

void SoapySDRPlay::updateRSP2LNAstateGainReductions()
{
    static int LNAstateGR0[] = { 0, 10, 15, 21, 24, 34, 39, 45, 64 };
    static int LNAstateGR1[] = { 0,  7, 10, 17, 22, 41 };
    static int LNAstateGR2[] = { 0,  5, 21, 15, 15, 34 };
    static int LNAstateGR3[] = { 0,  6, 12, 18, 37 };

    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    sdrplay_api_Rsp2_AmPortSelectT amPortSel = chParams->rsp2TunerParams.amPortSel;
    if (rfHz < 420e6 && amPortSel == sdrplay_api_Rsp2_AMPORT_1) {
        LNAstateGainReductions = LNAstateGR3;
        maxLNAstate = sizeof(LNAstateGR3) / sizeof(LNAstateGR3[0]) - 1;
    } else if (rfHz < 420e6) {
        LNAstateGainReductions = LNAstateGR0;
        maxLNAstate = sizeof(LNAstateGR0) / sizeof(LNAstateGR0[0]) - 1;
    } else if (rfHz < 1000e6) {
        LNAstateGainReductions = LNAstateGR1;
        maxLNAstate = sizeof(LNAstateGR1) / sizeof(LNAstateGR1[0]) - 1;
    } else if (rfHz <= 2000e6) {
        LNAstateGainReductions = LNAstateGR2;
        maxLNAstate = sizeof(LNAstateGR2) / sizeof(LNAstateGR2[0]) - 1;
    }
    return;
}

void SoapySDRPlay::updateRSPduoLNAstateGainReductions()
{
    static int LNAstateGR0[] = { 0, 6, 12, 18, 37, 42, 61 };
    static int LNAstateGR1[] = { 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 };
    static int LNAstateGR2[] = { 0, 7, 13, 19, 20, 27, 33, 39, 45, 64 };
    static int LNAstateGR3[] = { 0, 6, 12, 20, 26, 32, 38, 43, 62 };
    static int LNAstateGR4[] = { 0, 6, 12, 18, 37 };

    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    sdrplay_api_RspDuo_AmPortSelectT tuner1AmPortSel = chParams->rspDuoTunerParams.tuner1AmPortSel;
    if (rfHz < 60e6 && tuner1AmPortSel == sdrplay_api_RspDuo_AMPORT_1) {
        LNAstateGainReductions = LNAstateGR4;
        maxLNAstate = sizeof(LNAstateGR4) / sizeof(LNAstateGR4[0]) - 1;
    } else if (rfHz < 60e6) {
        LNAstateGainReductions = LNAstateGR0;
        maxLNAstate = sizeof(LNAstateGR0) / sizeof(LNAstateGR0[0]) - 1;
    } else if (rfHz < 420e6) {
        LNAstateGainReductions = LNAstateGR1;
        maxLNAstate = sizeof(LNAstateGR1) / sizeof(LNAstateGR1[0]) - 1;
    } else if (rfHz < 1000e6) {
        LNAstateGainReductions = LNAstateGR2;
        maxLNAstate = sizeof(LNAstateGR2) / sizeof(LNAstateGR2[0]) - 1;
    } else if (rfHz <= 2000e6) {
        LNAstateGainReductions = LNAstateGR3;
        maxLNAstate = sizeof(LNAstateGR3) / sizeof(LNAstateGR3[0]) - 1;
    }
    return;
}

void SoapySDRPlay::updateRSPdxLNAstateGainReductions()
{
    static int LNAstateGR0[] = { 0, 3,  6,  9, 12, 15, 18, 21, 24, 25, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60 };
    static int LNAstateGR1[] = { 0, 3,  6,  9, 12, 15, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60 };
    static int LNAstateGR2[] = { 0, 3,  6,  9, 12, 15, 18, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60 };
    static int LNAstateGR3[] = { 0, 3,  6,  9, 12, 15, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84 };
    static int LNAstateGR4[] = { 0, 3,  6,  9, 12, 15, 18, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84 };
    static int LNAstateGR5[] = { 0, 7, 10, 13, 16, 19, 22, 25, 31, 34, 37, 40, 43, 46, 49, 52, 55, 58, 61, 64, 67 };
    static int LNAstateGR6[] = { 0, 5,  8, 11, 14, 17, 20, 32, 35, 38, 41, 44, 47, 50, 53, 56, 59, 62, 65 };

    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    unsigned char hdrEnable = deviceParams->devParams->rspDxParams.hdrEnable;
    if (rfHz < 2e6 && hdrEnable) {
        LNAstateGainReductions = LNAstateGR0;
        maxLNAstate = sizeof(LNAstateGR0) / sizeof(LNAstateGR0[0]) - 1;
    } else if (rfHz < 12e6) {
        LNAstateGainReductions = LNAstateGR1;
        maxLNAstate = sizeof(LNAstateGR1) / sizeof(LNAstateGR1[0]) - 1;
    } else if (rfHz < 60e6) {
        LNAstateGainReductions = LNAstateGR2;
        maxLNAstate = sizeof(LNAstateGR2) / sizeof(LNAstateGR2[0]) - 1;
    } else if (rfHz < 250e6) {
        LNAstateGainReductions = LNAstateGR3;
        maxLNAstate = sizeof(LNAstateGR3) / sizeof(LNAstateGR3[0]) - 1;
    } else if (rfHz < 420e6) {
        LNAstateGainReductions = LNAstateGR4;
        maxLNAstate = sizeof(LNAstateGR4) / sizeof(LNAstateGR4[0]) - 1;
    } else if (rfHz < 1000e6) {
        LNAstateGainReductions = LNAstateGR5;
        maxLNAstate = sizeof(LNAstateGR5) / sizeof(LNAstateGR5[0]) - 1;
    } else if (rfHz <= 2000e6) {
        LNAstateGainReductions = LNAstateGR6;
        maxLNAstate = sizeof(LNAstateGR6) / sizeof(LNAstateGR6[0]) - 1;
    }
    return;
}
