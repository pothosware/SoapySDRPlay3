/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Charles J. Cliffe
 * Copyright (c) 2020 Franco Venturi - changes for SDRplay API version 3
 *                                     and Dual Tuner for RSPduo

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
#include <iostream>

std::vector<std::string> SoapySDRPlay::getStreamFormats(const int direction, const size_t channel) const
{
    std::vector<std::string> formats;

    formats.push_back("CS16");
    formats.push_back("CF32");

    return formats;
}

std::string SoapySDRPlay::getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const
{
     fullScale = 32767;
     return "CS16";
}

SoapySDR::ArgInfoList SoapySDRPlay::getStreamArgsInfo(const int direction, const size_t channel) const
{
    SoapySDR::ArgInfoList streamArgs;

    return streamArgs;
}

/*******************************************************************
 * Async thread work
 ******************************************************************/

static void _rx_callback_A(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                           unsigned int numSamples, unsigned int reset, void *cbContext)
{
    SoapySDRPlay *self = (SoapySDRPlay *)cbContext;
    return self->rx_callback(xi, xq, params, numSamples, self->_streams[0]);
}

static void _rx_callback_B(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params,
                           unsigned int numSamples, unsigned int reset, void *cbContext)
{
    SoapySDRPlay *self = (SoapySDRPlay *)cbContext;
    return self->rx_callback(xi, xq, params, numSamples, self->_streams[1]);
}

static void _ev_callback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner,
                         sdrplay_api_EventParamsT *params, void *cbContext)
{
    SoapySDRPlay *self = (SoapySDRPlay *)cbContext;
    return self->ev_callback(eventId, tuner, params);
}

void SoapySDRPlay::rx_callback(short *xi, short *xq,
                               sdrplay_api_StreamCbParamsT *params,
                               unsigned int numSamples,
                               SoapySDRPlayStream *stream)
{
    if (stream == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(stream->mutex);

    if (params->grChanged != 0 || params->rfChanged != 0 || params->fsChanged != 0)
    {
        std::lock_guard <std::mutex> lock(_general_state_mutex);

        if (gr_changed == 0 && params->grChanged != 0)
        {
            gr_changed = params->grChanged;
        }
        if (rf_changed == 0 && params->rfChanged != 0)
        {
            rf_changed = params->rfChanged;
        }
        if (fs_changed == 0 && params->fsChanged != 0)
        {
            fs_changed = params->fsChanged;
        }
    }

    if (stream->count == numBuffers)
    {
        stream->overflowEvent = true;
        return;
    }

    int spaceReqd = numSamples * elementsPerSample * shortsPerWord;
    if ((stream->buffs[stream->tail].size() + spaceReqd) >= (bufferLength / chParams->ctrlParams.decimation.decimationFactor))
    {
       // increment the tail pointer and buffer count
       stream->tail = (stream->tail + 1) % numBuffers;
       stream->count++;

       auto &buff = stream->buffs[stream->tail];
       if (stream->count == numBuffers && (size_t) spaceReqd > buff.capacity() - buff.size())
       {
           stream->overflowEvent = true;
           return;
       }

       // notify readStream()
       stream->cond.notify_one();
    }

    // get current fill buffer
    auto &buff = stream->buffs[stream->tail];

    // we do not reallocate here, as we only resize within
    // the buffers capacity
    buff.resize(buff.size() + spaceReqd);

    // copy into the buffer queue
    unsigned int i = 0;

    if (useShort)
    {
       short *dptr = buff.data();
       dptr += (buff.size() - spaceReqd);
       for (i = 0; i < numSamples; i++)
       {
           *dptr++ = xi[i];
           *dptr++ = xq[i];
        }
    }
    else
    {
       float *dptr = (float *)buff.data();
       dptr += ((buff.size() - spaceReqd) / shortsPerWord);
       for (i = 0; i < numSamples; i++)
       {
          *dptr++ = (float)xi[i] / 32768.0f;
          *dptr++ = (float)xq[i] / 32768.0f;
       }
    }

    return;
}

void SoapySDRPlay::ev_callback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params)
{
    if (eventId == sdrplay_api_GainChange)
    {
        //Beware, lnaGRdB is really the LNA GR, NOT the LNA state !
        //sdrplay_api_GainCbParamT gainParams = params->gainParams;
        //unsigned int gRdB = gainParams.gRdB;
        //unsigned int lnaGRdB = gainParams.lnaGRdB;
        // gainParams.currGain is a calibrated gain value
        //if (gRdB < 200)
        //{
        //    current_gRdB = gRdB;
        //}
    }
    else if (eventId == sdrplay_api_PowerOverloadChange)
    {
        sdrplay_api_PowerOverloadCbEventIdT powerOverloadChangeType = params->powerOverloadParams.powerOverloadChangeType;
        if (powerOverloadChangeType == sdrplay_api_Overload_Detected)
        {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck, sdrplay_api_Update_Ext1_None);
            // OVERLOAD DETECTED
        }
        else if (powerOverloadChangeType == sdrplay_api_Overload_Corrected)
        {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Ctrl_OverloadMsgAck, sdrplay_api_Update_Ext1_None);
            // OVERLOAD CORRECTED
        }
    }
    else if (eventId == sdrplay_api_DeviceRemoved)
    {
        // Notify readStream() that the device has been removed so that
        // the application can be closed gracefully
        SoapySDR_log(SOAPY_SDR_ERROR, "Device has been removed. Stopping.");
        device_unavailable = true;
    }
    else if (eventId == sdrplay_api_RspDuoModeChange)
    {
        if (params->rspDuoModeParams.modeChangeType == sdrplay_api_MasterDllDisappeared)
        {
            // Notify readStream() that the master stream has been removed
            // so that the application can be closed gracefully
            SoapySDR_log(SOAPY_SDR_ERROR, "Master stream has been removed. Stopping.");
            device_unavailable = true;
        }
    }
}

/*******************************************************************
 * Stream API
 ******************************************************************/

SoapySDRPlay::SoapySDRPlayStream::SoapySDRPlayStream(size_t channel,
                                                     size_t numBuffers,
                                                     unsigned long bufferLength)
{
    std::lock_guard<std::mutex> lock(mutex);

    this->channel = channel;

    // clear async fifo counts
    tail = 0;
    head = 0;
    count = 0;

    // allocate buffers
    buffs.resize(numBuffers);
    for (auto &buff : buffs) buff.reserve(bufferLength);
}

SoapySDRPlay::SoapySDRPlayStream::~SoapySDRPlayStream()
{
}

SoapySDR::Stream *SoapySDRPlay::setupStream(const int direction,
                                            const std::string &format,
                                            const std::vector<size_t> &channels,
                                            const SoapySDR::Kwargs &args)
{
    size_t nchannels = device.hwVer == SDRPLAY_RSPduo_ID && device.rspDuoMode == sdrplay_api_RspDuoMode_Dual_Tuner ? 2 : 1;

    // check the channel configuration
    if (channels.size() > 1 or (channels.size() > 0 and channels.at(0) >= nchannels))
    {
       throw std::runtime_error("setupStream invalid channel selection");
    }

    // check the format
    if (format == "CS16")
    {
        useShort = true;
        shortsPerWord = 1;
        bufferLength = bufferElems * elementsPerSample * shortsPerWord;
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CS16.");
    }
    else if (format == "CF32")
    {
        useShort = false;
        shortsPerWord = sizeof(float) / sizeof(short);
        bufferLength = bufferElems * elementsPerSample * shortsPerWord;  // allocate enough space for floats instead of shorts
        SoapySDR_log(SOAPY_SDR_INFO, "Using format CF32.");
    }
    else
    {
        throw std::runtime_error( "setupStream invalid format '" + format +
                                  "' -- Only CS16 or CF32 are supported by the SoapySDRPlay module.");
    }

    // default is channel 0
    size_t channel = channels.size() == 0 ? 0 : channels.at(0);
    SoapySDRPlayStream *sdrplay_stream = _streams[channel];
    if (sdrplay_stream == 0)
    {
        sdrplay_stream = new SoapySDRPlayStream(channel, numBuffers, bufferLength);
    }
    return reinterpret_cast<SoapySDR::Stream *>(sdrplay_stream);
}

void SoapySDRPlay::closeStream(SoapySDR::Stream *stream)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    SoapySDRPlayStream *sdrplay_stream = reinterpret_cast<SoapySDRPlayStream *>(stream);

    bool deleteStream = false;
    int activeStreams = 0;
    for (int i = 0; i < 2; ++i)
    {
        if (_streams[i] == sdrplay_stream)
        {
            _streamsRefCount[i]--;
            if (_streamsRefCount[i] == 0)
            {
                _streams[i] = 0;
                deleteStream = true;
            }
        }
        activeStreams += _streamsRefCount[i];
    }

    if (deleteStream)
    {
        // notify readStream()
        sdrplay_stream->cond.notify_one();
        delete sdrplay_stream;
    }
    if (activeStreams == 0)
    {
        while (true)
        {
            sdrplay_api_ErrT err;
            err = sdrplay_api_Uninit(device.dev);
            if (err != sdrplay_api_StopPending)
            {
                break;
            }
            SoapySDR_logf(SOAPY_SDR_WARNING, "Please close RSPduo slave device first. Trying again in %d seconds", uninitRetryDelay);
            waitForDevice(uninitRetryDelay*1000);
        }
        streamActive = false;
    }
}

size_t SoapySDRPlay::getStreamMTU(SoapySDR::Stream *stream) const
{
    // is a constant in practice
    return bufferElems;
}

int SoapySDRPlay::activateStream(SoapySDR::Stream *stream,
                                 const int flags,
                                 const long long timeNs,
                                 const size_t numElems)
{
    if (flags != 0)
    {
        SoapySDR_log(SOAPY_SDR_ERROR, "error in activateStream() - flags != 0");
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    SoapySDRPlayStream *sdrplay_stream = reinterpret_cast<SoapySDRPlayStream *>(stream);

    sdrplay_stream->reset = true;
    sdrplay_stream->nElems = 0;
    _streams[sdrplay_stream->channel] = sdrplay_stream;
    _streamsRefCount[sdrplay_stream->channel]++;

    if (streamActive)
    {
        return 0;
    }

    sdrplay_api_ErrT err;

    std::lock_guard <std::mutex> lock(_general_state_mutex);

    // Enable (= sdrplay_api_DbgLvl_Verbose) API calls tracing,
    // but only for debug purposes due to its performance impact.
    sdrplay_api_DebugEnable(device.dev, sdrplay_api_DbgLvl_Disable);
    //sdrplay_api_DebugEnable(device.dev, sdrplay_api_DbgLvl_Verbose);

    chParams->tunerParams.dcOffsetTuner.dcCal = 4;
    chParams->tunerParams.dcOffsetTuner.speedUp = 0;
    chParams->tunerParams.dcOffsetTuner.trackTime = 63;

    sdrplay_api_CallbackFnsT cbFns;
    cbFns.StreamACbFn = _rx_callback_A;
    cbFns.StreamBCbFn = _rx_callback_B;
    cbFns.EventCbFn = _ev_callback;

#ifdef STREAMING_USB_MODE_BULK
    SoapySDR_log(SOAPY_SDR_INFO, "Using streaming USB mode bulk.");
    deviceParams->devParams->mode = sdrplay_api_BULK;
#endif

    err = sdrplay_api_Init(device.dev, &cbFns, (void *)this);
    if (err != sdrplay_api_Success)
    {
        SoapySDR_logf(SOAPY_SDR_ERROR, "error in activateStream() - Init() failed: %s", sdrplay_api_GetErrorString(err));
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    streamActive = true;

    return 0;
}

int SoapySDRPlay::deactivateStream(SoapySDR::Stream *stream, const int flags, const long long timeNs)
{
    if (flags != 0)
    {
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    // do nothing because deactivateStream() can be called multiple times
    return 0;
}

int SoapySDRPlay::readStream(SoapySDR::Stream *stream,
                             void * const *buffs,
                             const size_t numElems,
                             int &flags,
                             long long &timeNs,
                             const long timeoutUs)
{
    // the API requests us to wait until either the
    // timeout is reached or the stream is activated
    if (!streamActive)
    {
        using us = std::chrono::microseconds;
        std::this_thread::sleep_for(us(timeoutUs));
        if(!streamActive){
            return SOAPY_SDR_TIMEOUT;
        }
    }

    SoapySDRPlayStream *sdrplay_stream = reinterpret_cast<SoapySDRPlayStream *>(stream);
    if (_streams[sdrplay_stream->channel] == 0)
    {
        //throw std::runtime_error("readStream stream not activated");
        return SOAPY_SDR_NOT_SUPPORTED;
    }

    // fv
    std::lock_guard <std::mutex> lock(sdrplay_stream->anotherMutex);

    // are elements left in the buffer? if not, do a new read.
    if (sdrplay_stream->nElems == 0)
    {
        int ret = this->acquireReadBuffer(stream, sdrplay_stream->currentHandle, (const void **)&sdrplay_stream->currentBuff, flags, timeNs, timeoutUs);

        if (ret < 0)
        {
            // Do not generate logs here, as interleaving with stream indicators
            //SoapySDR_logf(SOAPY_SDR_WARNING, "readStream() failed: %s", SoapySDR_errToStr(ret));
            return ret;
        }
        sdrplay_stream->nElems = ret;
    }

    size_t returnedElems = std::min(sdrplay_stream->nElems.load(), numElems);

    // copy into user's buff - always write to buffs[0] since each stream
    // can have only one rx/channel
    if (useShort)
    {
        std::memcpy(buffs[0], sdrplay_stream->currentBuff, returnedElems * 2 * sizeof(short));
    }
    else
    {
        std::memcpy(buffs[0], (float *)(void*)sdrplay_stream->currentBuff, returnedElems * 2 * sizeof(float));
    }

    // bump variables for next call into readStream
    sdrplay_stream->nElems -= returnedElems;

    // scope lock here to update stream->currentBuff position
    {
        std::lock_guard <std::mutex> lock(sdrplay_stream->mutex);
        sdrplay_stream->currentBuff += returnedElems * elementsPerSample * shortsPerWord;
    }

    // return number of elements written to buff
    if (sdrplay_stream->nElems != 0)
    {
        flags |= SOAPY_SDR_MORE_FRAGMENTS;
    }
    else
    {
        this->releaseReadBuffer(stream, sdrplay_stream->currentHandle);
    }
    return (int)returnedElems;
}

/*******************************************************************
 * Direct buffer access API
 ******************************************************************/

size_t SoapySDRPlay::getNumDirectAccessBuffers(SoapySDR::Stream *stream)
{
    SoapySDRPlayStream *sdrplay_stream = reinterpret_cast<SoapySDRPlayStream *>(stream);
    std::lock_guard <std::mutex> lockA(sdrplay_stream->mutex);
    return sdrplay_stream->buffs.size();
}

int SoapySDRPlay::getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs)
{
    SoapySDRPlayStream *sdrplay_stream = reinterpret_cast<SoapySDRPlayStream *>(stream);
    std::lock_guard <std::mutex> lockA(sdrplay_stream->mutex);
    // always write to buffs[0] since each stream can have only one rx/channel
    buffs[0] = (void *)sdrplay_stream->buffs[handle].data();
    return 0;
}

int SoapySDRPlay::acquireReadBuffer(SoapySDR::Stream *stream,
                                    size_t &handle,
                                    const void **buffs,
                                    int &flags,
                                    long long &timeNs,
                                    const long timeoutUs)
{
    SoapySDRPlayStream *sdrplay_stream = reinterpret_cast<SoapySDRPlayStream *>(stream);

    std::unique_lock <std::mutex> lock(sdrplay_stream->mutex);

    // reset is issued by various settings
    // overflow set in the rx callback thread
    if (sdrplay_stream->reset || sdrplay_stream->overflowEvent)
    {
        // drain all buffers from the fifo
        sdrplay_stream->tail = 0;
        sdrplay_stream->head = 0;
        sdrplay_stream->count = 0;
        for (auto &buff : sdrplay_stream->buffs) buff.clear();
        sdrplay_stream->overflowEvent = false;
        if (sdrplay_stream->reset)
        {
           sdrplay_stream->reset = false;
        }
        else
        {
           SoapySDR_log(SOAPY_SDR_SSI, "O");
           return SOAPY_SDR_OVERFLOW;
        }
    }

    // wait for a buffer to become available
    if (sdrplay_stream->count == 0)
    {
        sdrplay_stream->cond.wait_for(lock, std::chrono::microseconds(timeoutUs));
        if (sdrplay_stream->count == 0)
        {
           return SOAPY_SDR_TIMEOUT;
        }
    }

    if (device_unavailable)
    {
       SoapySDR_log(SOAPY_SDR_ERROR, "Device is unavailable");
       return SOAPY_SDR_NOT_SUPPORTED;
    }

    // extract handle and buffer
    handle = sdrplay_stream->head;
    // always write to buffs[0] since each stream can have only one rx/channel
    buffs[0] = (void *)sdrplay_stream->buffs[handle].data();
    flags = 0;

    sdrplay_stream->head = (sdrplay_stream->head + 1) % numBuffers;

    // return number available
    return (int)(sdrplay_stream->buffs[handle].size() / (elementsPerSample * shortsPerWord));
}

void SoapySDRPlay::releaseReadBuffer(SoapySDR::Stream *stream, const size_t handle)
{
    SoapySDRPlayStream *sdrplay_stream = reinterpret_cast<SoapySDRPlayStream *>(stream);
    std::lock_guard <std::mutex> lockA(sdrplay_stream->mutex);
    sdrplay_stream->buffs[handle].clear();
    sdrplay_stream->count--;
}
