/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2020 Franco Venturi

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

#include "SoapySDRPlay.hpp"

float SoapySDRPlay::sdrplay_api::ver = 0.0;

// Singleton class for SDRplay API (only one per process)
SoapySDRPlay::sdrplay_api::sdrplay_api()
{
    sdrplay_api_ErrT err;
    // Open API
    err = sdrplay_api_Open();
    if (err != sdrplay_api_Success) {
        ::SoapySDR_logf(SOAPY_SDR_ERROR, "sdrplay_api_Open() Error: %s", sdrplay_api_GetErrorString(err));
        ::SoapySDR_logf(SOAPY_SDR_ERROR, "Please check the sdrplay_api service to make sure it is up. If it is up, please restart it.");
        throw std::runtime_error("sdrplay_api_Open() failed");
    }

    // Check API versions match
    err = sdrplay_api_ApiVersion(&ver);
    if (err != sdrplay_api_Success) {
        ::SoapySDR_logf(SOAPY_SDR_ERROR, "ApiVersion Error: %s", sdrplay_api_GetErrorString(err));
        sdrplay_api_Close();
        throw std::runtime_error("ApiVersion() failed");
    }
    if (ver != SDRPLAY_API_VERSION) {
        ::SoapySDR_logf(SOAPY_SDR_WARNING, "sdrplay_api version: '%.3f' does not equal build version: '%.3f'", ver, SDRPLAY_API_VERSION);
    }
}

SoapySDRPlay::sdrplay_api::~sdrplay_api()
{
    sdrplay_api_ErrT err;
    // Close API
    err = sdrplay_api_Close();
    if (err != sdrplay_api_Success) {
        ::SoapySDR_logf(SOAPY_SDR_ERROR, "sdrplay_api_Close() failed: %s", sdrplay_api_GetErrorString(err));
    }
}
