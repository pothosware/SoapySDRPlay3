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
#pragma once

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Logger.h>
#include <SoapySDR/Types.h>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <string>
#include <cstring>
#include <algorithm>
#include <set>
#include <unordered_map>

#include <sdrplay_api.h>

#define DEFAULT_BUFFER_LENGTH     (65536)
#define DEFAULT_NUM_BUFFERS       (8)
#define DEFAULT_ELEMS_PER_SAMPLE  (2)

std::set<std::string> &SoapySDRPlay_getClaimedSerials(void);

class SoapySDRPlay: public SoapySDR::Device
{
public:
    explicit SoapySDRPlay(const SoapySDR::Kwargs &args);

    ~SoapySDRPlay(void);

    /*******************************************************************
     * Identification API
     ******************************************************************/

    std::string getDriverKey(void) const;

    std::string getHardwareKey(void) const;

    SoapySDR::Kwargs getHardwareInfo(void) const;

    /*******************************************************************
     * Channels API
     ******************************************************************/

    size_t getNumChannels(const int) const;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const;

    std::string getNativeStreamFormat(const int direction, const size_t channel, double &fullScale) const;

    SoapySDR::ArgInfoList getStreamArgsInfo(const int direction, const size_t channel) const;

    SoapySDR::Stream *setupStream(const int direction, 
                                  const std::string &format, 
                                  const std::vector<size_t> &channels = std::vector<size_t>(), 
                                  const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    void closeStream(SoapySDR::Stream *stream);

    size_t getStreamMTU(SoapySDR::Stream *stream) const;

    int activateStream(SoapySDR::Stream *stream,
                       const int flags = 0,
                       const long long timeNs = 0,
                       const size_t numElems = 0);

    int deactivateStream(SoapySDR::Stream *stream, const int flags = 0, const long long timeNs = 0);

    int readStream(SoapySDR::Stream *stream,
                   void * const *buffs,
                   const size_t numElems,
                   int &flags,
                   long long &timeNs,
                   const long timeoutUs = 200000);

    /*******************************************************************
     * Direct buffer access API
     ******************************************************************/

    size_t getNumDirectAccessBuffers(SoapySDR::Stream *stream);

    int getDirectAccessBufferAddrs(SoapySDR::Stream *stream, const size_t handle, void **buffs);

    int acquireReadBuffer(SoapySDR::Stream *stream,
                          size_t &handle,
                          const void **buffs,
                          int &flags,
                          long long &timeNs,
                          const long timeoutUs = 100000);

    void releaseReadBuffer(SoapySDR::Stream *stream, const size_t handle);

    /*******************************************************************
     * Antenna API
     ******************************************************************/

    std::vector<std::string> listAntennas(const int direction, const size_t channel) const;

    void setAntenna(const int direction, const size_t channel, const std::string &name);

    std::string getAntenna(const int direction, const size_t channel) const;

    /*******************************************************************
     * Frontend corrections API
     ******************************************************************/

    bool hasDCOffsetMode(const int direction, const size_t channel) const;

    /*******************************************************************
     * Gain API
     ******************************************************************/

    std::vector<std::string> listGains(const int direction, const size_t channel) const;

    bool hasGainMode(const int direction, const size_t channel) const;

    void setGainMode(const int direction, const size_t channel, const bool automatic);

    bool getGainMode(const int direction, const size_t channel) const;

    void setGain(const int direction, const size_t channel, const std::string &name, const double value);

    double getGain(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    void setFrequency(const int direction,
                      const size_t channel,
                      const std::string &name,
                      const double frequency,
                      const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    double getFrequency(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::RangeList getBandwidthRange(const int direction, const size_t channel) const;

    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const;

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate);

    double getSampleRate(const int direction, const size_t channel) const;

    std::vector<double> listSampleRates(const int direction, const size_t channel) const;

    /*******************************************************************
    * Bandwidth API
    ******************************************************************/

    void setBandwidth(const int direction, const size_t channel, const double bw);

    double getBandwidth(const int direction, const size_t channel) const;

    std::vector<double> listBandwidths(const int direction, const size_t channel) const;
    
    void setDCOffsetMode(const int direction, const size_t channel, const bool automatic);
    
    bool getDCOffsetMode(const int direction, const size_t channel) const;
    
    bool hasDCOffset(const int direction, const size_t channel) const;

    /*******************************************************************
     * Settings API
     ******************************************************************/

    SoapySDR::ArgInfoList getSettingInfo(void) const;

    void writeSetting(const std::string &key, const std::string &value);

    void changeRspDuoMode(const std::string &rspDuoModeString,
                          bool resetDevice);

    std::string readSetting(const std::string &key) const;

    /*******************************************************************
     * Async API
     ******************************************************************/

    class SoapySDRPlayStream;
    void rx_callback(short *xi, short *xq, unsigned int numSamples, SoapySDRPlayStream *stream);

    void ev_callback(sdrplay_api_EventT eventId, sdrplay_api_TunerSelectT tuner, sdrplay_api_EventParamsT *params);

    /*******************************************************************
     * public utility static methods
     ******************************************************************/

    static unsigned char stringToHWVer(std::string hwVer);

    static std::string HWVertoString(unsigned char hwVer);

    static sdrplay_api_RspDuoModeT stringToRSPDuoMode(std::string rspDuoMode);

    static std::string RSPDuoModetoString(sdrplay_api_RspDuoModeT rspDuoMode);

private:

    /*******************************************************************
     * Internal functions
     ******************************************************************/

    double getInputSampleRateAndDecimation(uint32_t output_sample_rate, unsigned int *decM, unsigned int *decEnable, sdrplay_api_If_kHzT *ifType) const;

    static sdrplay_api_Bw_MHzT getBwEnumForRate(double output_sample_rate);

    static  double getBwValueFromEnum(sdrplay_api_Bw_MHzT bwEnum);

    static sdrplay_api_Bw_MHzT sdrPlayGetBwMhzEnum(double bw);

    void selectDevice(const std::string &serial, const std::string &mode, const std::string &antenna);

    void selectDevice();

    void selectDevice(sdrplay_api_TunerSelectT tuner,
                      sdrplay_api_RspDuoModeT rspDuoMode,
                      double rspDuoSampleFreq,
                      sdrplay_api_DeviceParamsT *thisDeviceParams);

    void releaseDevice();


    /*******************************************************************
     * Private variables
     ******************************************************************/
    //device settings
    sdrplay_api_DeviceT device;
    sdrplay_api_DeviceParamsT *deviceParams;
    sdrplay_api_RxChannelParamsT *chParams;
    int hwVer;
    std::string serNo;
    std::string cacheKey;
    // RSP device id is used to identify the device in 'selectedRSPDevices'
    //  - serial number for RSP (except the RSPduo) and the RSPduo in non-slave mode
    //  - serial number/S for the RSPduo in slave mode
    std::string rspDeviceId;

    //cached settings
    std::atomic_ulong bufferLength;

    //numBuffers, bufferElems, elementsPerSample
    //are indeed constants
    const size_t numBuffers = DEFAULT_NUM_BUFFERS;
    const unsigned int bufferElems = DEFAULT_BUFFER_LENGTH;
    const int elementsPerSample = DEFAULT_ELEMS_PER_SAMPLE;

    std::atomic_uint shortsPerWord;
 
    std::atomic_bool streamActive;

    std::atomic_bool useShort;

    const int uninitRetryDelay = 10;   // 10 seconds before trying uninit again 

    static std::unordered_map<std::string, sdrplay_api_DeviceT*> selectedRSPDevices;

public:

   /*******************************************************************
    * Public variables
    ******************************************************************/
    
    mutable std::mutex _general_state_mutex;

    class SoapySDRPlayStream
    {
    public:
        SoapySDRPlayStream(size_t channel, size_t numBuffers, unsigned long bufferLength);
        ~SoapySDRPlayStream(void);

        size_t channel;

        std::mutex mutex;
        std::condition_variable cond;

        std::vector<std::vector<short> > buffs;
        size_t      head;
        size_t      tail;
        /// number of in-flight buffers
        size_t      count;
        short *currentBuff;
        bool overflowEvent;
        std::atomic_size_t nElems;
        size_t currentHandle;
        std::atomic_bool reset;

        // fv
        std::mutex anotherMutex;
    };

    SoapySDRPlayStream *_streams[2];
    int _streamsRefCount[2];

    constexpr static double defaultRspDuoSampleFreq = 6000000;
    constexpr static double defaultRspDuoOutputSampleRate = 2000000;

    // Singleton class for SDRplay API (only one per process)
    class sdrplay_api
    {
    public:
        static sdrplay_api& get_instance()
        {
            static sdrplay_api instance;
            return instance;
        }
        static float get_version()
        {
            return ver;
        }

    private:
        static float ver;
        sdrplay_api();

    public:
        ~sdrplay_api();
        sdrplay_api(sdrplay_api const&)    = delete;
        void operator=(sdrplay_api const&) = delete;
    };
};
