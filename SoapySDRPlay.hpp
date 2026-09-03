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

    bool hasFrequencyCorrection(const int direction, const size_t channel) const;

    void setFrequencyCorrection(const int direction, const size_t channel, const double value);

    double getFrequencyCorrection(const int direction, const size_t channel) const;

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
                      const double frequency,
                      const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    void setFrequency(const int direction,
                      const size_t channel,
                      const std::string &name,
                      const double frequency,
                      const SoapySDR::Kwargs &args = SoapySDR::Kwargs());

    double getFrequency(const int direction, const size_t channel) const;

    double getFrequency(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::RangeList getBandwidthRange(const int direction, const size_t channel) const;

    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::ArgInfoList getFrequencyArgsInfo(const int direction, const size_t channel) const;

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate);

    double getSampleRate(const int direction, const size_t channel) const;

    std::vector<double> listSampleRates(const int direction, const size_t channel) const;

    SoapySDR::RangeList getSampleRateRange(const int direction, const size_t channel) const;

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

    // Per channel settings, which is how the two tuners of an RSPduo in Dual
    // Tuner mode are reached separately: the bias-T, the notches and the AGC
    // set point are the tuner's rather than the device's, and each tuner is a
    // channel of its own. The device wide calls above address every channel at
    // once, so an application that does not know about the second tuner still
    // configures both of them the same way.
    SoapySDR::ArgInfoList getSettingInfo(const int direction, const size_t channel) const;

    void writeSetting(const int direction, const size_t channel,
                      const std::string &key, const std::string &value);

    std::string readSetting(const int direction, const size_t channel,
                            const std::string &key) const;

    /*******************************************************************
     * Async API
     ******************************************************************/

    class SoapySDRPlayStream;
    void rx_callback(short *xi, short *xq, sdrplay_api_StreamCbParamsT *params, unsigned int numSamples, size_t channel);

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

    static double getBwValueFromEnum(sdrplay_api_Bw_MHzT bwEnum);

    void selectDevice(const std::string &serial, const std::string &mode, const std::string &antenna);

    void selectDevice();

    void selectDevice(sdrplay_api_TunerSelectT tuner,
                      sdrplay_api_RspDuoModeT rspDuoMode,
                      double rspDuoSampleFreq,
                      sdrplay_api_DeviceParamsT *thisDeviceParams);

    void releaseDevice();

    /*******************************************************************
     * Channels and tuners
     ******************************************************************/

    /// True while the device is an RSPduo running both of its tuners.
    bool isDualTuner(void) const;

    /// The parameters of one receive channel.
    ///
    /// Everywhere but in the RSPduo's Dual Tuner mode there is one channel and
    /// one tuner, and both are the selected one. In Dual Tuner mode channel 0
    /// is tuner A and channel 1 is tuner B, and each of them has a gain, an
    /// AGC and a bias-T of its own - which is what makes the two channels
    /// usable for diversity reception rather than merely present.
    sdrplay_api_RxChannelParamsT *getChannelParams(const size_t channel) const;

    /// Which tuner an update for a channel has to be sent to.
    sdrplay_api_TunerSelectT getChannelTuner(const size_t channel) const;

    /// The channels a device wide call has to reach - both of them in Dual
    /// Tuner mode, so that the two streams go on describing the same samples.
    std::vector<size_t> getAllChannels(void) const;

    /// writeSetting()/readSetting() for one channel, with the state mutex
    /// already held by the caller - both the device wide and the per channel
    /// entry points lock it before they get here.
    void writeChannelSetting(const size_t channel, const std::string &key,
                             const std::string &value);
    std::string readChannelSetting(const size_t channel, const std::string &key) const;

    /// Puts tuner B's parameters back after sdrplay_api_Init().
    ///
    /// Init() copies channel A's settings over channel B, so everything the
    /// second tuner was configured with before the stream started is lost
    /// unless it is written again - see activateStream().
    void restoreTunerB(const sdrplay_api_RxChannelParamsT &saved);

#ifdef SHOW_SERIAL_NUMBER_IN_MESSAGES
    void SoapySDR_log(const SoapySDRLogLevel logLevel, const char *message) const;
    void SoapySDR_logf(const SoapySDRLogLevel logLevel, const char *format, ...) const;
#endif


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

    // RX callback reporting changes to gain reduction, frequency, sample rate
    int gr_changed;
    int rf_changed;
    int fs_changed;
    // event callback reporting device is unavailable
    bool device_unavailable;
    const int updateTimeout = 500;   // 500ms timeout for updates

public:

   /*******************************************************************
    * Public variables
    ******************************************************************/
    
    mutable std::mutex _general_state_mutex;

    class SoapySDRPlayStream
    {
    public:
        SoapySDRPlayStream(const std::vector<size_t> &channels, size_t numBuffers,
                           unsigned long bufferLength);
        ~SoapySDRPlayStream(void);

        /// One channel's ring of buffers.
        ///
        /// Every ring is filled by its own tuner callback and advanced by the
        /// same rule, so slot n holds the same stretch of time on every channel
        /// of the stream. That is what a two channel read hands back: the two
        /// tuners of an RSPduo deliver their samples in matched pairs, and
        /// partitioning both rings identically keeps the pairing intact all the
        /// way to the caller's buffers.
        class Channel
        {
        public:
            Channel(size_t numBuffers, unsigned long bufferLength);

            std::vector<std::vector<short> > buffs;
            /// what the tuner called the first sample of each slot, which is
            /// what the two rings are checked against each other with
            std::vector<unsigned int> firstSampleNum;
            size_t tail;
            /// number of buffers filled and not yet read
            size_t count;
            /// where readStream() has got to inside the acquired buffer
            short *currentBuff;
        };

        /// Where a device channel sits in this stream, or -1 when it is not in it.
        int indexOf(size_t channel) const;

        /// How many complete buffers every channel has - the reader can only
        /// take a slot that all of them have finished with.
        size_t readyCount(void) const;

        /// Whether every channel's slot starts at the same sample number.
        bool slotsAligned(size_t slot) const;

        /// Throws away everything buffered on every channel. Only ever called
        /// from a callback that is standing at a block boundary - see
        /// drainPending.
        void drain(void);

        /// The device channels this stream carries, in the order readStream()
        /// fills the caller's buffers.
        std::vector<size_t> channels;
        std::vector<Channel> chans;

        std::mutex mutex;
        std::condition_variable cond;

        size_t      head;
        bool overflowEvent;
        std::atomic_size_t nElems;
        size_t currentHandle;
        std::atomic_bool reset;

        /// A drop asked for by the reader and carried out by the callbacks.
        ///
        /// Dropping what is buffered from the reader's side would leave the
        /// rings holding different blocks: the callbacks of one set arrive one
        /// after the other, so a drop in the middle of a set takes a block off
        /// one channel that the others have already been given. They would
        /// come back from it a whole block apart and stay there. So the reader
        /// only asks, and the drop happens where the callbacks line up again.
        bool drainPending;
        /// how many of this set of per channel callbacks have arrived; zero is
        /// the boundary between one block and the next
        size_t delivered;

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
