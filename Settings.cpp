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

#if defined(_M_X64) || defined(_M_IX86)
#define strcasecmp _stricmp
#elif defined (__GNUC__)
#include <strings.h>
#endif

std::unordered_map<std::string, sdrplay_api_DeviceT*> SoapySDRPlay::selectedRSPDevices;


std::set<std::string> &SoapySDRPlay_getClaimedSerials(void)
{
   static std::set<std::string> serials;
   return serials;
}

SoapySDRPlay::SoapySDRPlay(const SoapySDR::Kwargs &args)
{
    if (args.count("serial") == 0) throw std::runtime_error("no sdrplay device found");

    selectDevice(args.at("serial"),
                 args.count("mode") ? args.at("mode") : "",
                 args.count("antenna") ? args.at("antenna") : "");

    // keep all the default settings:
    // - rf: 200MHz
    // - fs: 2MHz
    // - decimation: off
    // - IF: 0kHz (zero IF)
    // - bw: 200kHz
    // - attenuation: 50dB
    // - LNA state: 0
    // - AGC: 50Hz
    // - DC correction: on
    // - IQ balance: on

    // process additional device string arguments
    for (std::pair<std::string, std::string> arg : args) {
        // ignore 'driver', 'label', 'mode', 'serial', and 'soapy'
        if (arg.first == "driver" || arg.first == "label" ||
            arg.first == "mode" || arg.first == "serial" ||
            arg.first == "soapy") {
            continue;
        }
        writeSetting(arg.first, arg.second);
    }

    // streaming settings
    // this may change later according to format
    shortsPerWord = 1;
    bufferLength = bufferElems * elementsPerSample * shortsPerWord;

    _streams[0] = 0;
    _streams[1] = 0;
    _streamsRefCount[0] = 0;
    _streamsRefCount[1] = 0;
    useShort = true;

    streamActive = false;

    cacheKey = serNo;
    if (hwVer == SDRPLAY_RSPduo_ID) cacheKey += "@" + args.at("mode");
    SoapySDRPlay_getClaimedSerials().insert(cacheKey);
}

SoapySDRPlay::~SoapySDRPlay(void)
{
    SoapySDRPlay_getClaimedSerials().erase(cacheKey);
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    releaseDevice();

    _streams[0] = 0;
    _streams[1] = 0;
    _streamsRefCount[0] = 0;
    _streamsRefCount[1] = 0;
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapySDRPlay::getDriverKey(void) const
{
    return "SDRplay";
}

std::string SoapySDRPlay::getHardwareKey(void) const
{
    if (hwVer == SDRPLAY_RSP1_ID) return "RSP1";
    if (hwVer == SDRPLAY_RSP1A_ID) return "RSP1A";
    if (hwVer == SDRPLAY_RSP2_ID) return "RSP2";
    if (hwVer == SDRPLAY_RSPduo_ID) return "RSPduo";
    if (hwVer == SDRPLAY_RSPdx_ID) return "RSPdx";
    return "UNKNOWN";
}

SoapySDR::Kwargs SoapySDRPlay::getHardwareInfo(void) const
{
    // key/value pairs for any useful information
    // this also gets printed in --probe
    SoapySDR::Kwargs hwArgs;

    float ver = SoapySDRPlay::sdrplay_api::get_version();
    hwArgs["sdrplay_api_api_version"] = std::to_string(ver);
    hwArgs["sdrplay_api_hw_version"] = std::to_string(device.hwVer);

    return hwArgs;
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapySDRPlay::getNumChannels(const int dir) const
{
    if (device.hwVer == SDRPLAY_RSPduo_ID && device.rspDuoMode == sdrplay_api_RspDuoMode_Dual_Tuner) {
        return (dir == SOAPY_SDR_RX) ? 2 : 0;
    }
    return (dir == SOAPY_SDR_RX) ? 1 : 0;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapySDRPlay::listAntennas(const int direction, const size_t channel) const
{
    std::vector<std::string> antennas;

    if (direction == SOAPY_SDR_TX) {
        return antennas;
    }

    if (device.hwVer == SDRPLAY_RSP1_ID || device.hwVer == SDRPLAY_RSP1A_ID) {
        antennas.push_back("RX");
    }
    else if (device.hwVer == SDRPLAY_RSP2_ID) {
        antennas.push_back("Antenna A");
        antennas.push_back("Antenna B");
        antennas.push_back("Hi-Z");
    }
    else if (device.hwVer == SDRPLAY_RSPdx_ID) {
        antennas.push_back("Antenna A");
        antennas.push_back("Antenna B");
        antennas.push_back("Antenna C");
    }
    else if (device.hwVer == SDRPLAY_RSPduo_ID) {
        if (device.rspDuoMode == sdrplay_api_RspDuoMode_Single_Tuner ||
            device.rspDuoMode == sdrplay_api_RspDuoMode_Master) {
            antennas.push_back("Tuner 1 50 ohm");
            antennas.push_back("Tuner 1 Hi-Z");
            antennas.push_back("Tuner 2 50 ohm");
        }
        else if (device.rspDuoMode == sdrplay_api_RspDuoMode_Dual_Tuner) {
            if (channel == 0) {
                // No Hi-Z antenna in Dual Tuner mode
                // For diversity reception you would want the two tuner inputs
                // to be the same otherwise there is a mismatch in the gain
                // control.
                antennas.push_back("Tuner 1 50 ohm");
            }
            else if (channel == 1) {
                antennas.push_back("Tuner 2 50 ohm");
            }
        }
        else if (device.rspDuoMode == sdrplay_api_RspDuoMode_Slave) {
            if (device.tuner == sdrplay_api_Tuner_A) {
                antennas.push_back("Tuner 1 50 ohm");
                antennas.push_back("Tuner 1 Hi-Z");
            }
            else if (device.tuner == sdrplay_api_Tuner_B) {
                antennas.push_back("Tuner 2 50 ohm");
            }
        }
    }
    return antennas;
}

void SoapySDRPlay::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    // Check direction
    if ((direction != SOAPY_SDR_RX) || (device.hwVer == SDRPLAY_RSP1_ID) || (device.hwVer == SDRPLAY_RSP1A_ID)) {
        return;       
    }

    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (device.hwVer == SDRPLAY_RSP2_ID)
    {
        bool changeToAntennaA_B = false;

        if (name == "Antenna A")
        {
            chParams->rsp2TunerParams.antennaSel = sdrplay_api_Rsp2_ANTENNA_A;
            changeToAntennaA_B = true;
        }
        else if (name == "Antenna B")
        {
            chParams->rsp2TunerParams.antennaSel = sdrplay_api_Rsp2_ANTENNA_B;
            changeToAntennaA_B = true;
        }
        else if (name == "Hi-Z")
        {
            chParams->rsp2TunerParams.amPortSel = sdrplay_api_Rsp2_AMPORT_1;

            if (streamActive)
            {
                sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Rsp2_AmPortSelect, sdrplay_api_Update_Ext1_None);
            }
        }

        if (changeToAntennaA_B)
        {
        
            //if we are currently High_Z, make the switch first.
            if (chParams->rsp2TunerParams.amPortSel == sdrplay_api_Rsp2_AMPORT_1)
            {
                chParams->rsp2TunerParams.amPortSel = sdrplay_api_Rsp2_AMPORT_2;

                if (streamActive)
                {
                    sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Rsp2_AmPortSelect, sdrplay_api_Update_Ext1_None);
                }
            }
            else
            {
                if (streamActive)
                {
                    sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Rsp2_AntennaControl, sdrplay_api_Update_Ext1_None);
                }
            }
        }
    }
    else if (device.hwVer == SDRPLAY_RSPdx_ID)
    {
        if (name == "Antenna A")
        {
            deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_A;
        }
        else if (name == "Antenna B")
        {
            deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_B;
        }
        else if (name == "Antenna C")
        {
            deviceParams->devParams->rspDxParams.antennaSel = sdrplay_api_RspDx_ANTENNA_C;
        }

        if (streamActive)
        {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_AntennaControl);
        }
    }
    else if (device.hwVer == SDRPLAY_RSPduo_ID)
    {
        bool changeToTunerA_B = false;
        bool changeAmPort = false;
        bool isTunerChangeAllowed = device.rspDuoMode == sdrplay_api_RspDuoMode_Single_Tuner || device.rspDuoMode == sdrplay_api_RspDuoMode_Master;

        if (name == "Tuner 1 50 ohm")
        {
            changeAmPort = chParams->rspDuoTunerParams.tuner1AmPortSel != sdrplay_api_RspDuo_AMPORT_2;
            chParams->rspDuoTunerParams.tuner1AmPortSel = sdrplay_api_RspDuo_AMPORT_2;
            changeToTunerA_B = isTunerChangeAllowed && device.tuner != sdrplay_api_Tuner_A;
        }
        else if (name == "Tuner 2 50 ohm")
        {
            changeAmPort = chParams->rspDuoTunerParams.tuner1AmPortSel != sdrplay_api_RspDuo_AMPORT_2;
            changeToTunerA_B = isTunerChangeAllowed && device.tuner != sdrplay_api_Tuner_B;
        }
        else if (name == "Tuner 1 Hi-Z")
        {
            changeAmPort = chParams->rspDuoTunerParams.tuner1AmPortSel != sdrplay_api_RspDuo_AMPORT_1;
            chParams->rspDuoTunerParams.tuner1AmPortSel = sdrplay_api_RspDuo_AMPORT_1;
            changeToTunerA_B = isTunerChangeAllowed && device.tuner != sdrplay_api_Tuner_A;
        }

        if (!changeToTunerA_B)
        {
            if (changeAmPort)
            {
                //if we are currently High_Z, make the switch first.
                if (streamActive)
                {
                    sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_RspDuo_AmPortSelect, sdrplay_api_Update_Ext1_None);
                }
            }
        }
        else
        {
            if (streamActive)
            {
                if (device.rspDuoMode == sdrplay_api_RspDuoMode_Single_Tuner)
                {
                    sdrplay_api_ErrT err;
                    err = sdrplay_api_SwapRspDuoActiveTuner(device.dev,
                               &device.tuner, chParams->rspDuoTunerParams.tuner1AmPortSel);
                    if (err != sdrplay_api_Success)
                    {
                        SoapySDR_logf(SOAPY_SDR_WARNING, "SwapRspDuoActiveTuner Error: %s", sdrplay_api_GetErrorString(err));
                    }
                    chParams = device.tuner == sdrplay_api_Tuner_B ?
                               deviceParams->rxChannelB : deviceParams->rxChannelA;
                }
                else if (device.rspDuoMode == sdrplay_api_RspDuoMode_Master)
                {
                    // not sure what is the best way to handle this case - fv
                    SoapySDR_log(SOAPY_SDR_WARNING, "tuner change not allowed in RSPduo Master mode while the device is streaming");
                }
            }
            else
            {
                // preserve biasT setting when changing tuner/antenna
                unsigned char biasTen = chParams->rspDuoTunerParams.biasTEnable;
                sdrplay_api_TunerSelectT other_tuner = (device.tuner == sdrplay_api_Tuner_A) ? sdrplay_api_Tuner_B : sdrplay_api_Tuner_A;
                selectDevice(other_tuner, device.rspDuoMode,
                             device.rspDuoSampleFreq, nullptr);
                chParams->rspDuoTunerParams.biasTEnable = biasTen;
            }
        }
    }
}

std::string SoapySDRPlay::getAntenna(const int direction, const size_t channel) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (direction == SOAPY_SDR_TX)
    {
        return "";
    }

    if (device.hwVer == SDRPLAY_RSP2_ID)
    {
        if (chParams->rsp2TunerParams.amPortSel == sdrplay_api_Rsp2_AMPORT_1) {
            return "Hi-Z";
        }
        else if (chParams->rsp2TunerParams.antennaSel == sdrplay_api_Rsp2_ANTENNA_A) {
            return "Antenna A";
        }
        else {
            return "Antenna B";  
        }
    }
    else if (device.hwVer == SDRPLAY_RSPduo_ID)
    {
        if (device.tuner == sdrplay_api_Tuner_A ||
                (device.tuner == sdrplay_api_Tuner_Both && channel == 0)) {
            if (chParams->rspDuoTunerParams.tuner1AmPortSel == sdrplay_api_RspDuo_AMPORT_1) {
                return "Tuner 1 Hi-Z";
            } else {
                return "Tuner 1 50 ohm";
            }
        } else if (device.tuner == sdrplay_api_Tuner_B ||
                  (device.tuner == sdrplay_api_Tuner_Both && channel == 1)) {
                return "Tuner 2 50 ohm";
        }
    }
    else if (device.hwVer == SDRPLAY_RSPdx_ID)
    {
        if (deviceParams->devParams->rspDxParams.antennaSel == sdrplay_api_RspDx_ANTENNA_A) {
            return "Antenna A";
        }
        else if (deviceParams->devParams->rspDxParams.antennaSel == sdrplay_api_RspDx_ANTENNA_B) {
            return "Antenna B";
        }
        else if (deviceParams->devParams->rspDxParams.antennaSel == sdrplay_api_RspDx_ANTENNA_C) {
            return "Antenna C";
        }
    }

    return "RX";
}

/*******************************************************************
 * Frontend corrections API
 ******************************************************************/

bool SoapySDRPlay::hasDCOffsetMode(const int direction, const size_t channel) const
{
    return true;
}

void SoapySDRPlay::setDCOffsetMode(const int direction, const size_t channel, const bool automatic)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    //enable/disable automatic DC removal
    chParams->ctrlParams.dcOffset.DCenable = (unsigned char)automatic;
    chParams->ctrlParams.dcOffset.IQenable = (unsigned char)automatic;
}

bool SoapySDRPlay::getDCOffsetMode(const int direction, const size_t channel) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    return (bool)chParams->ctrlParams.dcOffset.DCenable;
}

bool SoapySDRPlay::hasDCOffset(const int direction, const size_t channel) const
{
    //is a specific DC removal value configurable?
    return false;
}

/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapySDRPlay::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("IFGR");
    results.push_back("RFGR");

    return results;
}

bool SoapySDRPlay::hasGainMode(const int direction, const size_t channel) const
{
    return true;
}

void SoapySDRPlay::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    sdrplay_api_AgcControlT agc_control = automatic ? sdrplay_api_AGC_CTRL_EN : sdrplay_api_AGC_DISABLE;
    if (chParams->ctrlParams.agc.enable != agc_control)
    {
        chParams->ctrlParams.agc.enable = agc_control;
        if (streamActive)
        {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Ctrl_Agc, sdrplay_api_Update_Ext1_None);
        }
    }
}

bool SoapySDRPlay::getGainMode(const int direction, const size_t channel) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    return chParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE;
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   bool doUpdate = false;

   if (name == "IFGR" && chParams->ctrlParams.agc.enable == sdrplay_api_AGC_DISABLE)
   {
      //apply the change if the required value is different from gRdB 
      if (chParams->tunerParams.gain.gRdB != (int)value)
      {
         chParams->tunerParams.gain.gRdB = (int)value;
         doUpdate = true;
      }
   }
   else if (name == "RFGR")
   {
      if (chParams->tunerParams.gain.LNAstate != (int)value) {

          chParams->tunerParams.gain.LNAstate = (int)value;
          doUpdate = true;
      }
   }
   if ((doUpdate == true) && (streamActive))
   {
      sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
   }
}

double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (name == "IFGR")
   {
       return chParams->tunerParams.gain.gRdB;
   }
   else if (name == "RFGR")
   {
      return chParams->tunerParams.gain.LNAstate;
   }

   return 0;
}

SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
   if (name == "IFGR")
   {
      return SoapySDR::Range(20, 59);
   }
   else if ((name == "RFGR") && (device.hwVer == SDRPLAY_RSP1_ID))
   {
      return SoapySDR::Range(0, 3);
   }
   else if ((name == "RFGR") && (device.hwVer == SDRPLAY_RSP2_ID))
   {
      return SoapySDR::Range(0, 8);
   }
   else if ((name == "RFGR") && (device.hwVer == SDRPLAY_RSPduo_ID))
   {
      return SoapySDR::Range(0, 9);
   }
   else if ((name == "RFGR") && (device.hwVer == SDRPLAY_RSP1A_ID))
   {
      return SoapySDR::Range(0, 9);
   }
   else if ((name == "RFGR") && (device.hwVer == SDRPLAY_RSPdx_ID))
   {
      return SoapySDR::Range(0, 27);
   }
    return SoapySDR::Range(20, 59);
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapySDRPlay::setFrequency(const int direction,
                                 const size_t channel,
                                 const std::string &name,
                                 const double frequency,
                                 const SoapySDR::Kwargs &args)
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (direction == SOAPY_SDR_RX)
   {
      if ((name == "RF") && (chParams->tunerParams.rfFreq.rfHz != (uint32_t)frequency))
      {
         chParams->tunerParams.rfFreq.rfHz = (uint32_t)frequency;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Tuner_Frf, sdrplay_api_Update_Ext1_None);
         }
      }
      // can't set ppm for RSPduo slaves
      else if ((name == "CORR") && deviceParams->devParams &&
              (deviceParams->devParams->ppm != frequency))
      {
         deviceParams->devParams->ppm = frequency;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Dev_Ppm, sdrplay_api_Update_Ext1_None);
         }
      }
   }
}

double SoapySDRPlay::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    if (name == "RF")
    {
        return (double)chParams->tunerParams.rfFreq.rfHz;
    }
    else if (name == "CORR")
    {
        if (deviceParams->devParams)
        {
            return deviceParams->devParams->ppm;
        } else {
            return 0;
        }
    }

    return 0;
}

std::vector<std::string> SoapySDRPlay::listFrequencies(const int direction, const size_t channel) const
{
    std::vector<std::string> names;
    names.push_back("RF");
    names.push_back("CORR");
    return names;
}

SoapySDR::RangeList SoapySDRPlay::getFrequencyRange(const int direction, const size_t channel,  const std::string &name) const
{
    SoapySDR::RangeList results;
    if (name == "RF")
    {
        if(device.hwVer == SDRPLAY_RSP1_ID)
        {
            results.push_back(SoapySDR::Range(10000, 2000000000));
        }
        else
        {
            results.push_back(SoapySDR::Range(1000, 2000000000));
        }
    }
    return results;
}

SoapySDR::ArgInfoList SoapySDRPlay::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
    SoapySDR::ArgInfoList freqArgs;

    return freqArgs;
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

/* input_sample_rate:  sample rate used by the SDR
 * output_sample_rate: sample rate as seen by the client app
 *                     (<= input_sample_rate because of decimation)
 */

void SoapySDRPlay::setSampleRate(const int direction, const size_t channel, const double output_sample_rate)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

    SoapySDR_logf(SOAPY_SDR_DEBUG, "Requested output sample rate: %lf", output_sample_rate);

    if (direction == SOAPY_SDR_RX)
    {
       unsigned int decM;
       unsigned int decEnable;
       sdrplay_api_If_kHzT ifType;
       double input_sample_rate = getInputSampleRateAndDecimation(output_sample_rate, &decM, &decEnable, &ifType);
       if (input_sample_rate < 0) {
           SoapySDR_logf(SOAPY_SDR_WARNING, "invalid sample rate. Sample rate unchanged.");
           return;
       }

       sdrplay_api_Bw_MHzT bwType = getBwEnumForRate(output_sample_rate);

       sdrplay_api_ReasonForUpdateT reasonForUpdate = sdrplay_api_Update_None;
       if (deviceParams->devParams && input_sample_rate != deviceParams->devParams->fsFreq.fsHz)
       {
          deviceParams->devParams->fsFreq.fsHz = input_sample_rate;
          reasonForUpdate = (sdrplay_api_ReasonForUpdateT)(reasonForUpdate | sdrplay_api_Update_Dev_Fs);
       }
       if (ifType != chParams->tunerParams.ifType)
       {
          chParams->tunerParams.ifType = ifType;
          reasonForUpdate = (sdrplay_api_ReasonForUpdateT)(reasonForUpdate | sdrplay_api_Update_Tuner_IfType);
       }
       if (decM != chParams->ctrlParams.decimation.decimationFactor)
       {
          chParams->ctrlParams.decimation.enable = decEnable;
          chParams->ctrlParams.decimation.decimationFactor = decM;
          if (ifType == sdrplay_api_IF_Zero) {
              chParams->ctrlParams.decimation.wideBandSignal = 1;
          }
          else {
              chParams->ctrlParams.decimation.wideBandSignal = 0;
          }
          reasonForUpdate = (sdrplay_api_ReasonForUpdateT)(reasonForUpdate | sdrplay_api_Update_Ctrl_Decimation);
       }
       if (bwType != chParams->tunerParams.bwType)
       {
          chParams->tunerParams.bwType = bwType;
          reasonForUpdate = (sdrplay_api_ReasonForUpdateT)(reasonForUpdate | sdrplay_api_Update_Tuner_BwType);
       }
       if (reasonForUpdate != sdrplay_api_Update_None)
       {
          if (_streams[0]) { _streams[0]->reset = true; }
          if (_streams[1]) { _streams[1]->reset = true; }
          if (streamActive)
          {
             // beware that when the fs change crosses the boundary between
             // 2,685,312 and 2,685,313 the rx_callbacks stop for some
             // reason
             sdrplay_api_Update(device.dev, device.tuner, reasonForUpdate, sdrplay_api_Update_Ext1_None);
          }
       }
    }
}

double SoapySDRPlay::getSampleRate(const int direction, const size_t channel) const
{
   double fsHz = deviceParams->devParams ? deviceParams->devParams->fsFreq.fsHz : device.rspDuoSampleFreq;
   if ((fsHz == 6.0e6 && chParams->tunerParams.ifType == sdrplay_api_IF_1_620) ||
       (fsHz == 8.0e6 && chParams->tunerParams.ifType == sdrplay_api_IF_2_048))
   {
      fsHz = 2.0e6;
   }
   else if (!(fsHz >= 2.0e6 &&
              chParams->tunerParams.ifType == sdrplay_api_IF_Zero &&
              (device.hwVer != SDRPLAY_RSPduo_ID || device.rspDuoMode == sdrplay_api_RspDuoMode_Single_Tuner)
           ))
   {
      SoapySDR_logf(SOAPY_SDR_ERROR, "Invalid sample rate and/or IF setting - fsHz=%lf ifType=%d hwVer=%d rspDuoMode=%d rspDuoSampleFreq=%lf", fsHz, chParams->tunerParams.ifType, device.hwVer, device.rspDuoMode, device.rspDuoSampleFreq);
      throw std::runtime_error("Invalid sample rate and/or IF setting");
   }

   if (!chParams->ctrlParams.decimation.enable)
   {
      return fsHz;
   }
   else
   {
      return fsHz / chParams->ctrlParams.decimation.decimationFactor;
   }
}

std::vector<double> SoapySDRPlay::listSampleRates(const int direction, const size_t channel) const
{
    std::vector<double> output_sample_rates;

    if (device.hwVer == SDRPLAY_RSPduo_ID && device.rspDuoMode != sdrplay_api_RspDuoMode_Single_Tuner)
    {
        output_sample_rates.push_back(62500);
        output_sample_rates.push_back(125000);
        output_sample_rates.push_back(250000);
        output_sample_rates.push_back(500000);
        output_sample_rates.push_back(1000000);
        output_sample_rates.push_back(2000000);
        return output_sample_rates;
    }

    output_sample_rates.push_back(62500);
    output_sample_rates.push_back(96000);
    output_sample_rates.push_back(125000);
    output_sample_rates.push_back(192000);
    output_sample_rates.push_back(250000);
    output_sample_rates.push_back(384000);
    output_sample_rates.push_back(500000);
    output_sample_rates.push_back(768000);
    output_sample_rates.push_back(1000000);
    output_sample_rates.push_back(2000000);
    output_sample_rates.push_back(2048000);
    output_sample_rates.push_back(3000000);
    output_sample_rates.push_back(4000000);
    output_sample_rates.push_back(5000000);
    output_sample_rates.push_back(6000000);
    output_sample_rates.push_back(7000000);
    output_sample_rates.push_back(8000000);
    output_sample_rates.push_back(9000000);
    output_sample_rates.push_back(10000000);
    return output_sample_rates;
}

double SoapySDRPlay::getInputSampleRateAndDecimation(uint32_t output_sample_rate, unsigned int *decM, unsigned int *decEnable, sdrplay_api_If_kHzT *ifType) const
{
    sdrplay_api_If_kHzT lif = sdrplay_api_IF_1_620;
    double lif_input_sample_rate = 6000000;
    if (device.hwVer == SDRPLAY_RSPduo_ID && device.rspDuoSampleFreq == 8000000)
    {
        lif = sdrplay_api_IF_2_048;
        lif_input_sample_rate = 8000000;
    }

    // all RSPs should support these sample rates
    switch (output_sample_rate) {
        case 62500:
            *ifType = lif; *decM = 32; *decEnable = 1;
            return lif_input_sample_rate;
        case 125000:
            *ifType = lif; *decM = 16; *decEnable = 1;
            return lif_input_sample_rate;
        case 250000:
            *ifType = lif; *decM =  8; *decEnable = 1;
            return lif_input_sample_rate;
        case 500000:
            *ifType = lif; *decM =  4; *decEnable = 1;
            return lif_input_sample_rate;
        case 1000000:
            *ifType = lif; *decM =  2; *decEnable = 1;
            return lif_input_sample_rate;
        case 2000000:
            *ifType = lif; *decM =  1; *decEnable = 0;
            return lif_input_sample_rate;
    }

    if (device.hwVer == SDRPLAY_RSPduo_ID && device.rspDuoMode != sdrplay_api_RspDuoMode_Single_Tuner)
    {
        return -1;
    }
  
    if (output_sample_rate <= 2000000)
    {
        switch (output_sample_rate) {
            case 96000:
                *ifType = sdrplay_api_IF_Zero; *decM = 32; *decEnable = 1;
                return output_sample_rate * *decM;
            case 192000:
                *ifType = sdrplay_api_IF_Zero; *decM = 16; *decEnable = 1;
                return output_sample_rate * *decM;
            case 384000:
                *ifType = sdrplay_api_IF_Zero; *decM =  8; *decEnable = 1;
                return output_sample_rate * *decM;
            case 768000:
                *ifType = sdrplay_api_IF_Zero; *decM =  4; *decEnable = 1;
                return output_sample_rate * *decM;
            default:
                return -1;
        }
    }

    // rate should be > 2 MHz so just return output_sample_rate
    *decM = 1; *decEnable = 0;
    *ifType = sdrplay_api_IF_Zero;
    return output_sample_rate;
}

/*******************************************************************
* Bandwidth API
******************************************************************/

void SoapySDRPlay::setBandwidth(const int direction, const size_t channel, const double bw_in)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (direction == SOAPY_SDR_RX) 
   {
      if (getBwValueFromEnum(chParams->tunerParams.bwType) != bw_in)
      {
         chParams->tunerParams.bwType = sdrPlayGetBwMhzEnum(bw_in);
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Tuner_BwType, sdrplay_api_Update_Ext1_None);
         }
      }
   }
}

double SoapySDRPlay::getBandwidth(const int direction, const size_t channel) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (direction == SOAPY_SDR_RX)
   {
      return getBwValueFromEnum(chParams->tunerParams.bwType);
   }
   return 0;
}

std::vector<double> SoapySDRPlay::listBandwidths(const int direction, const size_t channel) const
{
   std::vector<double> bandwidths;
   bandwidths.push_back(200000);
   bandwidths.push_back(300000);
   bandwidths.push_back(600000);
   bandwidths.push_back(1536000);
   if (!(device.hwVer == SDRPLAY_RSPduo_ID &&
         (device.rspDuoMode == sdrplay_api_RspDuoMode_Dual_Tuner ||
          device.rspDuoMode == sdrplay_api_RspDuoMode_Master ||
          device.rspDuoMode == sdrplay_api_RspDuoMode_Slave))) {
       bandwidths.push_back(5000000);
       bandwidths.push_back(6000000);
       bandwidths.push_back(7000000);
       bandwidths.push_back(8000000);
   }
   return bandwidths;
}

SoapySDR::RangeList SoapySDRPlay::getBandwidthRange(const int direction, const size_t channel) const
{
   SoapySDR::RangeList results;
   //call into the older deprecated listBandwidths() call
   for (auto &bw : this->listBandwidths(direction, channel))
   {
     results.push_back(SoapySDR::Range(bw, bw));
   }
   return results;
}


sdrplay_api_Bw_MHzT SoapySDRPlay::getBwEnumForRate(double output_sample_rate)
{
   if      (output_sample_rate <  300000) return sdrplay_api_BW_0_200;
   else if (output_sample_rate <  600000) return sdrplay_api_BW_0_300;
   else if (output_sample_rate < 1536000) return sdrplay_api_BW_0_600;
   else if (output_sample_rate < 5000000) return sdrplay_api_BW_1_536;
   else if (output_sample_rate < 6000000) return sdrplay_api_BW_5_000;
   else if (output_sample_rate < 7000000) return sdrplay_api_BW_6_000;
   else if (output_sample_rate < 8000000) return sdrplay_api_BW_7_000;
   else                                   return sdrplay_api_BW_8_000;
}


double SoapySDRPlay::getBwValueFromEnum(sdrplay_api_Bw_MHzT bwEnum)
{
   if      (bwEnum == sdrplay_api_BW_0_200) return 200000;
   else if (bwEnum == sdrplay_api_BW_0_300) return 300000;
   else if (bwEnum == sdrplay_api_BW_0_600) return 600000;
   else if (bwEnum == sdrplay_api_BW_1_536) return 1536000;
   else if (bwEnum == sdrplay_api_BW_5_000) return 5000000;
   else if (bwEnum == sdrplay_api_BW_6_000) return 6000000;
   else if (bwEnum == sdrplay_api_BW_7_000) return 7000000;
   else if (bwEnum == sdrplay_api_BW_8_000) return 8000000;
   else return 0;
}


sdrplay_api_Bw_MHzT SoapySDRPlay::sdrPlayGetBwMhzEnum(double bw)
{
   if      (bw == 200000) return sdrplay_api_BW_0_200;
   else if (bw == 300000) return sdrplay_api_BW_0_300;
   else if (bw == 600000) return sdrplay_api_BW_0_600;
   else if (bw == 1536000) return sdrplay_api_BW_1_536;
   else if (bw == 5000000) return sdrplay_api_BW_5_000;
   else if (bw == 6000000) return sdrplay_api_BW_6_000;
   else if (bw == 7000000) return sdrplay_api_BW_7_000;
   else if (bw == 8000000) return sdrplay_api_BW_8_000;
   else return sdrplay_api_BW_0_200;
}

/*******************************************************************
* Settings API
******************************************************************/

unsigned char SoapySDRPlay::stringToHWVer(std::string hwVer)
{
   if (strcasecmp(hwVer.c_str(), "RSP1") == 0)
   {
      return SDRPLAY_RSP1_ID;
   }
   else if (strcasecmp(hwVer.c_str(), "RSP1A") == 0)
   {
      return SDRPLAY_RSP1A_ID;
   }
   else if (strcasecmp(hwVer.c_str(), "RSP2") == 0)
   {
      return SDRPLAY_RSP2_ID;
   }
   else if (strcasecmp(hwVer.c_str(), "RSPduo") == 0)
   {
      return SDRPLAY_RSPduo_ID;
   }
   else if (strcasecmp(hwVer.c_str(), "RSPdx") == 0)
   {
      return SDRPLAY_RSPdx_ID;
   }
   return 0;
}

std::string SoapySDRPlay::HWVertoString(unsigned char hwVer)
{
   switch (hwVer)
   {
   case SDRPLAY_RSP1_ID:
      return "RSP1";
      break;
   case SDRPLAY_RSP1A_ID:
      return "RSP1A";
      break;
   case SDRPLAY_RSP2_ID:
      return "RSP2";
      break;
   case SDRPLAY_RSPduo_ID:
      return "RSPduo";
      break;
   case SDRPLAY_RSPdx_ID:
      return "RSPdx";
      break;
   }
   return "";
}

sdrplay_api_RspDuoModeT SoapySDRPlay::stringToRSPDuoMode(std::string rspDuoMode)
{
   if (strcasecmp(rspDuoMode.c_str(), "Single Tuner") == 0)
   {
      return sdrplay_api_RspDuoMode_Single_Tuner;
   }
   else if (strcasecmp(rspDuoMode.c_str(), "Dual Tuner") == 0)
   {
      return sdrplay_api_RspDuoMode_Dual_Tuner;
   }
   else if (strcasecmp(rspDuoMode.c_str(), "Master") == 0)
   {
      return sdrplay_api_RspDuoMode_Master;
   }
   else if (strcasecmp(rspDuoMode.c_str(), "Slave") == 0)
   {
      return sdrplay_api_RspDuoMode_Slave;
   }
   return sdrplay_api_RspDuoMode_Unknown;
}

std::string SoapySDRPlay::RSPDuoModetoString(sdrplay_api_RspDuoModeT rspDuoMode)
{
   switch (rspDuoMode)
   {
   case sdrplay_api_RspDuoMode_Unknown:
      return "";
      break;
   case sdrplay_api_RspDuoMode_Single_Tuner:
      return "Single Tuner";
      break;
   case sdrplay_api_RspDuoMode_Dual_Tuner:
      return "Dual Tuner";
      break;
   case sdrplay_api_RspDuoMode_Master:
      return "Master";
      break;
   case sdrplay_api_RspDuoMode_Slave:
      return "Slave";
      break;
   }
   return "";
}

SoapySDR::ArgInfoList SoapySDRPlay::getSettingInfo(void) const
{
    SoapySDR::ArgInfoList setArgs;

    // call selectDevice() because CubicSDR may think the device is
    // already selected - fv
    // here we need to cast away the constness of this, since selectDevice()
    // makes changes to its members
    SoapySDRPlay *non_const_this = const_cast<SoapySDRPlay*>(this);
    non_const_this->selectDevice();

#ifdef RF_GAIN_IN_MENU
    if (device.hwVer == SDRPLAY_RSP2_ID)
    {
       SoapySDR::ArgInfo RfGainArg;
       RfGainArg.key = "rfgain_sel";
       RfGainArg.value = "4";
       RfGainArg.name = "RF Gain Select";
       RfGainArg.description = "RF Gain Select";
       RfGainArg.type = SoapySDR::ArgInfo::STRING;
       RfGainArg.options.push_back("0");
       RfGainArg.options.push_back("1");
       RfGainArg.options.push_back("2");
       RfGainArg.options.push_back("3");
       RfGainArg.options.push_back("4");
       RfGainArg.options.push_back("5");
       RfGainArg.options.push_back("6");
       RfGainArg.options.push_back("7");
       RfGainArg.options.push_back("8");
       setArgs.push_back(RfGainArg);
    }
    else if (device.hwVer == SDRPLAY_RSPduo_ID)
    {
       SoapySDR::ArgInfo RfGainArg;
       RfGainArg.key = "rfgain_sel";
       RfGainArg.value = "4";
       RfGainArg.name = "RF Gain Select";
       RfGainArg.description = "RF Gain Select";
       RfGainArg.type = SoapySDR::ArgInfo::STRING;
       RfGainArg.options.push_back("0");
       RfGainArg.options.push_back("1");
       RfGainArg.options.push_back("2");
       RfGainArg.options.push_back("3");
       RfGainArg.options.push_back("4");
       RfGainArg.options.push_back("5");
       RfGainArg.options.push_back("6");
       RfGainArg.options.push_back("7");
       RfGainArg.options.push_back("8");
       RfGainArg.options.push_back("9");
       setArgs.push_back(RfGainArg);
    }
    else if (device.hwVer == SDRPLAY_RSP1A_ID)
    {
       SoapySDR::ArgInfo RfGainArg;
       RfGainArg.key = "rfgain_sel";
       RfGainArg.value = "4";
       RfGainArg.name = "RF Gain Select";
       RfGainArg.description = "RF Gain Select";
       RfGainArg.type = SoapySDR::ArgInfo::STRING;
       RfGainArg.options.push_back("0");
       RfGainArg.options.push_back("1");
       RfGainArg.options.push_back("2");
       RfGainArg.options.push_back("3");
       RfGainArg.options.push_back("4");
       RfGainArg.options.push_back("5");
       RfGainArg.options.push_back("6");
       RfGainArg.options.push_back("7");
       RfGainArg.options.push_back("8");
       RfGainArg.options.push_back("9");
       setArgs.push_back(RfGainArg);
    }
    else if (device.hwVer == SDRPLAY_RSPdx_ID)
    {
       SoapySDR::ArgInfo RfGainArg;
       RfGainArg.key = "rfgain_sel";
       RfGainArg.value = "4";
       RfGainArg.name = "RF Gain Select";
       RfGainArg.description = "RF Gain Select";
       RfGainArg.type = SoapySDR::ArgInfo::STRING;
       RfGainArg.options.push_back("0");
       RfGainArg.options.push_back("1");
       RfGainArg.options.push_back("2");
       RfGainArg.options.push_back("3");
       RfGainArg.options.push_back("4");
       RfGainArg.options.push_back("5");
       RfGainArg.options.push_back("6");
       RfGainArg.options.push_back("7");
       RfGainArg.options.push_back("8");
       RfGainArg.options.push_back("9");
       RfGainArg.options.push_back("10");
       RfGainArg.options.push_back("11");
       RfGainArg.options.push_back("12");
       RfGainArg.options.push_back("13");
       RfGainArg.options.push_back("14");
       RfGainArg.options.push_back("15");
       RfGainArg.options.push_back("16");
       RfGainArg.options.push_back("17");
       RfGainArg.options.push_back("18");
       RfGainArg.options.push_back("19");
       RfGainArg.options.push_back("20");
       RfGainArg.options.push_back("21");
       RfGainArg.options.push_back("22");
       RfGainArg.options.push_back("23");
       RfGainArg.options.push_back("24");
       RfGainArg.options.push_back("25");
       RfGainArg.options.push_back("26");
       RfGainArg.options.push_back("27");
       setArgs.push_back(RfGainArg);
    }
    else
    {
       SoapySDR::ArgInfo RfGainArg;
       RfGainArg.key = "rfgain_sel";
       RfGainArg.value = "1";
       RfGainArg.name = "RF Gain Select";
       RfGainArg.description = "RF Gain Select";
       RfGainArg.type = SoapySDR::ArgInfo::STRING;
       RfGainArg.options.push_back("0");
       RfGainArg.options.push_back("1");
       RfGainArg.options.push_back("2");
       RfGainArg.options.push_back("3");
       setArgs.push_back(RfGainArg);
    }
#endif

    SoapySDR::ArgInfo IQcorrArg;
    IQcorrArg.key = "iqcorr_ctrl";
    IQcorrArg.value = "true";
    IQcorrArg.name = "IQ Correction";
    IQcorrArg.description = "IQ Correction Control";
    IQcorrArg.type = SoapySDR::ArgInfo::BOOL;
    setArgs.push_back(IQcorrArg);

    SoapySDR::ArgInfo SetPointArg;
    SetPointArg.key = "agc_setpoint";
    SetPointArg.value = "-30";
    SetPointArg.name = "AGC Setpoint";
    SetPointArg.description = "AGC Setpoint (dBfs)";
    SetPointArg.type = SoapySDR::ArgInfo::INT;
    SetPointArg.range = SoapySDR::Range(-60, 0);
    setArgs.push_back(SetPointArg);

    if (device.hwVer == SDRPLAY_RSP2_ID) // RSP2/RSP2pro
    {
       SoapySDR::ArgInfo ExtRefArg;
       ExtRefArg.key = "extref_ctrl";
       ExtRefArg.value = "true";
       ExtRefArg.name = "ExtRef Enable";
       ExtRefArg.description = "External Reference Control";
       ExtRefArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(ExtRefArg);

       SoapySDR::ArgInfo BiasTArg;
       BiasTArg.key = "biasT_ctrl";
       BiasTArg.value = "true";
       BiasTArg.name = "BiasT Enable";
       BiasTArg.description = "BiasT Control";
       BiasTArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(BiasTArg);

       SoapySDR::ArgInfo RfNotchArg;
       RfNotchArg.key = "rfnotch_ctrl";
       RfNotchArg.value = "true";
       RfNotchArg.name = "RfNotch Enable";
       RfNotchArg.description = "RF Notch Filter Control";
       RfNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(RfNotchArg);
    }
    else if (device.hwVer == SDRPLAY_RSPduo_ID) // RSPduo
    {
       SoapySDR::ArgInfo ExtRefArg;
       ExtRefArg.key = "extref_ctrl";
       ExtRefArg.value = "true";
       ExtRefArg.name = "ExtRef Enable";
       ExtRefArg.description = "External Reference Control";
       ExtRefArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(ExtRefArg);

       SoapySDR::ArgInfo BiasTArg;
       BiasTArg.key = "biasT_ctrl";
       BiasTArg.value = "true";
       BiasTArg.name = "BiasT Enable";
       BiasTArg.description = "BiasT Control";
       BiasTArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(BiasTArg);

       SoapySDR::ArgInfo RfNotchArg;
       RfNotchArg.key = "rfnotch_ctrl";
       RfNotchArg.value = "true";
       RfNotchArg.name = "RfNotch Enable";
       RfNotchArg.description = "RF Notch Filter Control";
       RfNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(RfNotchArg);

       SoapySDR::ArgInfo DabNotchArg;
       DabNotchArg.key = "dabnotch_ctrl";
       DabNotchArg.value = "true";
       DabNotchArg.name = "DabNotch Enable";
       DabNotchArg.description = "DAB Notch Filter Control";
       DabNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(DabNotchArg);
    }
    else if (device.hwVer == SDRPLAY_RSP1A_ID) // RSP1A
    {
       SoapySDR::ArgInfo BiasTArg;
       BiasTArg.key = "biasT_ctrl";
       BiasTArg.value = "true";
       BiasTArg.name = "BiasT Enable";
       BiasTArg.description = "BiasT Control";
       BiasTArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(BiasTArg);

       SoapySDR::ArgInfo RfNotchArg;
       RfNotchArg.key = "rfnotch_ctrl";
       RfNotchArg.value = "true";
       RfNotchArg.name = "RfNotch Enable";
       RfNotchArg.description = "RF Notch Filter Control";
       RfNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(RfNotchArg);

       SoapySDR::ArgInfo DabNotchArg;
       DabNotchArg.key = "dabnotch_ctrl";
       DabNotchArg.value = "true";
       DabNotchArg.name = "DabNotch Enable";
       DabNotchArg.description = "DAB Notch Filter Control";
       DabNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(DabNotchArg);
    }
    else if (device.hwVer == SDRPLAY_RSPdx_ID) // RSPdx
    {
       SoapySDR::ArgInfo BiasTArg;
       BiasTArg.key = "biasT_ctrl";
       BiasTArg.value = "true";
       BiasTArg.name = "BiasT Enable";
       BiasTArg.description = "BiasT Control";
       BiasTArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(BiasTArg);

       SoapySDR::ArgInfo RfNotchArg;
       RfNotchArg.key = "rfnotch_ctrl";
       RfNotchArg.value = "true";
       RfNotchArg.name = "RfNotch Enable";
       RfNotchArg.description = "RF Notch Filter Control";
       RfNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(RfNotchArg);

       SoapySDR::ArgInfo DabNotchArg;
       DabNotchArg.key = "dabnotch_ctrl";
       DabNotchArg.value = "true";
       DabNotchArg.name = "DabNotch Enable";
       DabNotchArg.description = "DAB Notch Filter Control";
       DabNotchArg.type = SoapySDR::ArgInfo::BOOL;
       setArgs.push_back(DabNotchArg);
    }

    return setArgs;
}

void SoapySDRPlay::writeSetting(const std::string &key, const std::string &value)
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);

#ifdef RF_GAIN_IN_MENU
   if (key == "rfgain_sel")
   {
      chParams->tunerParams.gain.LNAstate = static_cast<unsigned char>(stoul(value));
      sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
   }
   else
#endif
   if (key == "iqcorr_ctrl")
   {
      if (value == "false") chParams->ctrlParams.dcOffset.IQenable = 0;
      else                  chParams->ctrlParams.dcOffset.IQenable = 1;
      chParams->ctrlParams.dcOffset.DCenable = 1;
      if (streamActive)
      {
         sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Ctrl_DCoffsetIQimbalance, sdrplay_api_Update_Ext1_None);
      }
   }
   else if (key == "agc_setpoint")
   {
      chParams->ctrlParams.agc.setPoint_dBfs = stoi(value);
      if (streamActive)
      {
         sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Ctrl_Agc, sdrplay_api_Update_Ext1_None);
      }
   }
   else if (key == "extref_ctrl")
   {
      unsigned char extRef;
      if (value == "false") extRef = 0;
      else                  extRef = 1;
      if (device.hwVer == SDRPLAY_RSP2_ID)
      {
         deviceParams->devParams->rsp2Params.extRefOutputEn = extRef;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Rsp2_ExtRefControl, sdrplay_api_Update_Ext1_None);
         }
      }
      if (device.hwVer == SDRPLAY_RSPduo_ID)
      {
         // can't get extRefOutputEn for RSPduo slaves
         if (deviceParams->devParams)
         {
           deviceParams->devParams->rspDuoParams.extRefOutputEn = extRef;
           if (streamActive)
           {
             sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_RspDuo_ExtRefControl, sdrplay_api_Update_Ext1_None);
           }
         }
      }
   }
   else if (key == "biasT_ctrl")
   {
      unsigned char biasTen;
      if (value == "false") biasTen = 0;
      else                  biasTen = 1;
      if (device.hwVer == SDRPLAY_RSP2_ID)
      {
         chParams->rsp2TunerParams.biasTEnable = biasTen;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Rsp2_BiasTControl, sdrplay_api_Update_Ext1_None);
         }
      }
      else if (device.hwVer == SDRPLAY_RSPduo_ID)
      {
         chParams->rspDuoTunerParams.biasTEnable = biasTen;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_RspDuo_BiasTControl, sdrplay_api_Update_Ext1_None);
         }
      }
      else if (device.hwVer == SDRPLAY_RSP1A_ID)
      {
         chParams->rsp1aTunerParams.biasTEnable = biasTen;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Rsp1a_BiasTControl, sdrplay_api_Update_Ext1_None);
         }
      }
      else if (device.hwVer == SDRPLAY_RSPdx_ID)
      {
         deviceParams->devParams->rspDxParams.biasTEnable = biasTen;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_BiasTControl);
         }
      }
   }
   else if (key == "rfnotch_ctrl")
   {
      unsigned char notchEn;
      if (value == "false") notchEn = 0;
      else                  notchEn = 1;
      if (device.hwVer == SDRPLAY_RSP2_ID)
      {
         chParams->rsp2TunerParams.rfNotchEnable = notchEn;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Rsp2_RfNotchControl, sdrplay_api_Update_Ext1_None);
         }
      }
      else if (device.hwVer == SDRPLAY_RSPduo_ID)
      {
        if (device.tuner == sdrplay_api_Tuner_A && chParams->rspDuoTunerParams.tuner1AmPortSel == sdrplay_api_RspDuo_AMPORT_1)
        {
          chParams->rspDuoTunerParams.tuner1AmNotchEnable = notchEn;
          if (streamActive)
          {
             sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_RspDuo_Tuner1AmNotchControl, sdrplay_api_Update_Ext1_None);
          }
        }
        if (chParams->rspDuoTunerParams.tuner1AmPortSel == sdrplay_api_RspDuo_AMPORT_2)
        {
          chParams->rspDuoTunerParams.rfNotchEnable = notchEn;
          if (streamActive)
          {
             sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_RspDuo_RfNotchControl, sdrplay_api_Update_Ext1_None);
          }
        }
      }
      else if (device.hwVer == SDRPLAY_RSP1A_ID)
      {
         deviceParams->devParams->rsp1aParams.rfNotchEnable = notchEn;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Rsp1a_RfNotchControl, sdrplay_api_Update_Ext1_None);
         }
      }
      else if (device.hwVer == SDRPLAY_RSPdx_ID)
      {
         deviceParams->devParams->rspDxParams.rfNotchEnable = notchEn;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfNotchControl);
         }
      }
   }
   else if (key == "dabnotch_ctrl")
   {
      unsigned char dabNotchEn;
      if (value == "false") dabNotchEn = 0;
      else                  dabNotchEn = 1;
      if (device.hwVer == SDRPLAY_RSPduo_ID)
      {
         chParams->rspDuoTunerParams.rfDabNotchEnable = dabNotchEn;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_RspDuo_RfDabNotchControl, sdrplay_api_Update_Ext1_None);
         }
      }
      if (device.hwVer == SDRPLAY_RSP1A_ID)
      {
         deviceParams->devParams->rsp1aParams.rfDabNotchEnable = dabNotchEn;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Rsp1a_RfDabNotchControl, sdrplay_api_Update_Ext1_None);
         }
      }
      else if (device.hwVer == SDRPLAY_RSPdx_ID)
      {
         deviceParams->devParams->rspDxParams.rfDabNotchEnable = dabNotchEn;
         if (streamActive)
         {
            sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_None, sdrplay_api_Update_RspDx_RfDabNotchControl);
         }
      }
   }
}

std::string SoapySDRPlay::readSetting(const std::string &key) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

#ifdef RF_GAIN_IN_MENU
    if (key == "rfgain_sel")
    {
       return std::to_string(static_cast<unsigned int>(chParams->tunerParams.gain.LNAstate));
    }
    else
#endif
    if (key == "iqcorr_ctrl")
    {
       if (chParams->ctrlParams.dcOffset.IQenable == 0) return "false";
       else                                             return "true";
    }
    else if (key == "agc_setpoint")
    {
       return std::to_string(chParams->ctrlParams.agc.setPoint_dBfs);
    }
    else if (key == "extref_ctrl")
    {
       unsigned char extRef = 0;
       if (device.hwVer == SDRPLAY_RSP2_ID) extRef = deviceParams->devParams->rsp2Params.extRefOutputEn;
      if (device.hwVer == SDRPLAY_RSPduo_ID) {
         // can't get extRefOutputEn for RSPduo slaves
         if (!deviceParams->devParams) {
            return "unknown";
         }
         extRef = deviceParams->devParams->rspDuoParams.extRefOutputEn;
       }
       if (extRef == 0) return "false";
       else             return "true";
    }
    else if (key == "biasT_ctrl")
    {
       unsigned char biasTen = 0;
       if (device.hwVer == SDRPLAY_RSP2_ID) biasTen = chParams->rsp2TunerParams.biasTEnable;
       else if (device.hwVer == SDRPLAY_RSPduo_ID) biasTen = chParams->rspDuoTunerParams.biasTEnable;
       else if (device.hwVer == SDRPLAY_RSP1A_ID) biasTen = chParams->rsp1aTunerParams.biasTEnable;
       else if (device.hwVer == SDRPLAY_RSPdx_ID) biasTen = deviceParams->devParams->rspDxParams.biasTEnable;
       if (biasTen == 0) return "false";
       else              return "true";
    }
    else if (key == "rfnotch_ctrl")
    {
       unsigned char notchEn = 0;
       if (device.hwVer == SDRPLAY_RSP2_ID) notchEn = chParams->rsp2TunerParams.rfNotchEnable;
       else if (device.hwVer == SDRPLAY_RSPduo_ID)
       {
          if (device.tuner == sdrplay_api_Tuner_A && chParams->rspDuoTunerParams.tuner1AmPortSel == sdrplay_api_RspDuo_AMPORT_1)
          {
             notchEn = chParams->rspDuoTunerParams.tuner1AmNotchEnable;
          }
          if (chParams->rspDuoTunerParams.tuner1AmPortSel == sdrplay_api_RspDuo_AMPORT_2)
          {
             notchEn = chParams->rspDuoTunerParams.rfNotchEnable;
          }
       }
       else if (device.hwVer == SDRPLAY_RSP1A_ID) notchEn = deviceParams->devParams->rsp1aParams.rfNotchEnable;
       else if (device.hwVer == SDRPLAY_RSPdx_ID) notchEn = deviceParams->devParams->rspDxParams.rfNotchEnable;
       if (notchEn == 0) return "false";
       else              return "true";
    }
    else if (key == "dabnotch_ctrl")
    {
       unsigned char dabNotchEn = 0;
       if (device.hwVer == SDRPLAY_RSPduo_ID) dabNotchEn = chParams->rspDuoTunerParams.rfDabNotchEnable;
       else if (device.hwVer == SDRPLAY_RSP1A_ID) dabNotchEn = deviceParams->devParams->rsp1aParams.rfDabNotchEnable;
       else if (device.hwVer == SDRPLAY_RSPdx_ID) dabNotchEn = deviceParams->devParams->rspDxParams.rfDabNotchEnable;
       if (dabNotchEn == 0) return "false";
       else                 return "true";
    }

    // SoapySDR_logf(SOAPY_SDR_WARNING, "Unknown setting '%s'", key.c_str());
    return "";
}

void SoapySDRPlay::selectDevice(const std::string &serial,
                                const std::string &mode,
                                const std::string &antenna)
{
    serNo = serial;
    rspDeviceId = serial;
    if (mode == "SL") {
        rspDeviceId += "/S";
    }

    sdrplay_api_TunerSelectT tuner;
    sdrplay_api_RspDuoModeT rspDuoMode;
    double rspDuoSampleFreq = 0.0;
    if (mode.empty())
    {
        tuner = sdrplay_api_Tuner_Neither;
        rspDuoMode = sdrplay_api_RspDuoMode_Unknown;
        rspDuoSampleFreq = 0.0;
    }
    else if (mode == "ST")
    {
        tuner = sdrplay_api_Tuner_A;
        rspDuoMode = sdrplay_api_RspDuoMode_Single_Tuner;
        rspDuoSampleFreq = 0.0;
    }
    else if (mode == "DT")
    {
        tuner = sdrplay_api_Tuner_Both;
        rspDuoMode = sdrplay_api_RspDuoMode_Dual_Tuner;
        rspDuoSampleFreq = 6000000;
    }
    else if (mode == "MA")
    {
        tuner = sdrplay_api_Tuner_A;
        rspDuoMode = sdrplay_api_RspDuoMode_Master;
        rspDuoSampleFreq = 6000000;
    }
    else if (mode == "MA8")
    {
        tuner = sdrplay_api_Tuner_A;
        rspDuoMode = sdrplay_api_RspDuoMode_Master;
        rspDuoSampleFreq = 8000000;
    }
    else if (mode == "SL")
    {
        tuner = sdrplay_api_Tuner_Neither;
        rspDuoMode = sdrplay_api_RspDuoMode_Slave;
    }
    else
    {
        throw std::runtime_error("sdrplay RSPduo mode is invalid");
    }

    // if an antenna is specified, select the RSPduo tuner based on it
    if (!(rspDuoMode == sdrplay_api_RspDuoMode_Unknown ||
          rspDuoMode == sdrplay_api_RspDuoMode_Dual_Tuner))
    {
        if (!antenna.empty())
        {
            if (antenna == "Tuner 1 50 ohm") {
                tuner = sdrplay_api_Tuner_A;
            } else if (antenna == "Tuner 1 Hi-Z") {
                tuner = sdrplay_api_Tuner_A;
            } else if (antenna == "Tuner 2 50 ohm") {
                tuner = sdrplay_api_Tuner_B;
            } else {
                throw std::runtime_error("invalid RSPduo antenna selected");
            }
        }
    }

    selectDevice(tuner, rspDuoMode, rspDuoSampleFreq, nullptr);

    return;
}

void SoapySDRPlay::selectDevice()
{
    if (selectedRSPDevices.count(rspDeviceId))
    {
        sdrplay_api_DeviceT *currDevice = selectedRSPDevices.at(rspDeviceId);
        if (currDevice == &device) {
            // nothing to do - we are good
            return;
        }
    }

    selectDevice(device.tuner, device.rspDuoMode, device.rspDuoSampleFreq,
                 deviceParams);

    return;
}

void SoapySDRPlay::selectDevice(sdrplay_api_TunerSelectT tuner,
                                sdrplay_api_RspDuoModeT rspDuoMode,
                                double rspDuoSampleFreq,
                                sdrplay_api_DeviceParamsT *thisDeviceParams)
{
    sdrplay_api_ErrT err;
    if (selectedRSPDevices.count(rspDeviceId)) {
        sdrplay_api_DeviceT *currDevice = selectedRSPDevices.at(rspDeviceId);
        selectedRSPDevices.erase(rspDeviceId);
        sdrplay_api_LockDeviceApi();
        err = sdrplay_api_ReleaseDevice(currDevice);
        if (err != sdrplay_api_Success)
        {
            sdrplay_api_UnlockDeviceApi();
            SoapySDR_logf(SOAPY_SDR_ERROR, "ReleaseDevice Error: %s", sdrplay_api_GetErrorString(err));
            throw std::runtime_error("ReleaseDevice() failed");
        }
        sdrplay_api_UnlockDeviceApi();
    }

    // save all the device configuration so we can put it back later on
    bool hasDevParams = false;
    bool hasRxChannelA = false;
    bool hasRxChannelB = false;
    sdrplay_api_DevParamsT devParams;
    sdrplay_api_RxChannelParamsT rxChannelA;
    sdrplay_api_RxChannelParamsT rxChannelB;
    if (thisDeviceParams)
    {
        hasDevParams = thisDeviceParams->devParams;
        hasRxChannelA = thisDeviceParams->rxChannelA;
        hasRxChannelB = thisDeviceParams->rxChannelB;
        if (hasDevParams) devParams = *thisDeviceParams->devParams;
        if (hasRxChannelA) rxChannelA = *thisDeviceParams->rxChannelA;
        if (hasRxChannelB) rxChannelB = *thisDeviceParams->rxChannelB;
    }

    // retrieve hwVer and serNo by API
    unsigned int nDevs = 0;

    sdrplay_api_LockDeviceApi();
    sdrplay_api_DeviceT rspDevs[SDRPLAY_MAX_DEVICES];
    sdrplay_api_GetDevices(&rspDevs[0], &nDevs, SDRPLAY_MAX_DEVICES);

    unsigned devIdx = SDRPLAY_MAX_DEVICES;
    for (unsigned int i = 0; i < nDevs; i++)
    {
        if (rspDevs[i].SerNo == serNo) devIdx = i;
    }
    if (devIdx == SDRPLAY_MAX_DEVICES) throw std::runtime_error("no sdrplay device matches");

    device = rspDevs[devIdx];
    hwVer = device.hwVer;

    SoapySDR_logf(SOAPY_SDR_INFO, "devIdx: %d", devIdx);
    SoapySDR_logf(SOAPY_SDR_INFO, "hwVer: %d", device.hwVer);

    if (hwVer == SDRPLAY_RSPduo_ID && rspDuoMode != sdrplay_api_RspDuoMode_Slave)
    {
        if ((rspDuoMode & device.rspDuoMode) != rspDuoMode)
        {
            throw std::runtime_error("sdrplay RSPduo mode not available");
        }
        else
        {
            device.rspDuoMode = rspDuoMode;
        }
        if ((tuner & device.tuner) != tuner)
        {
            throw std::runtime_error("sdrplay RSPduo tuner not available");
        }
        else
        {
            device.tuner = tuner;
        }
        if (rspDuoSampleFreq != 0)
        {
            device.rspDuoSampleFreq = rspDuoSampleFreq;
        }
    }
    else if (hwVer == SDRPLAY_RSPduo_ID && rspDuoMode == sdrplay_api_RspDuoMode_Slave)
    {
        if (rspDuoMode != device.rspDuoMode)
        {
            throw std::runtime_error("sdrplay RSPduo slave mode not available");
        }
        if (tuner != sdrplay_api_Tuner_Neither && tuner != device.tuner)
        {
            throw std::runtime_error("sdrplay RSPduo tuner not available in slave mode");
        }
        if (rspDuoSampleFreq != 0 && rspDuoSampleFreq != device.rspDuoSampleFreq)
        {
            throw std::runtime_error("sdrplay RSPduo sample rate not available in slace mode");
        }
    }
    else
    {
        if (rspDuoMode != sdrplay_api_RspDuoMode_Unknown || tuner != sdrplay_api_Tuner_Neither)
        {
            throw std::runtime_error("sdrplay RSP does not support RSPduo mode or tuner");
        }
    }

    SoapySDR_logf(SOAPY_SDR_INFO, "rspDuoMode: %d", device.rspDuoMode);
    SoapySDR_logf(SOAPY_SDR_INFO, "tuner: %d", device.tuner);
    SoapySDR_logf(SOAPY_SDR_INFO, "rspDuoSampleFreq: %lf", device.rspDuoSampleFreq);

    err = sdrplay_api_SelectDevice(&device);
    if (err != sdrplay_api_Success)
    {
        sdrplay_api_UnlockDeviceApi();
        SoapySDR_logf(SOAPY_SDR_ERROR, "SelectDevice Error: %s", sdrplay_api_GetErrorString(err));
        throw std::runtime_error("SelectDevice() failed");
    }
    selectedRSPDevices[rspDeviceId] = &device;

    sdrplay_api_UnlockDeviceApi();

    // Enable (= sdrplay_api_DbgLvl_Verbose) API calls tracing,
    // but only for debug purposes due to its performance impact.
    sdrplay_api_DebugEnable(device.dev, sdrplay_api_DbgLvl_Disable);
    //sdrplay_api_DebugEnable(device.dev, sdrplay_api_DbgLvl_Verbose);

    err = sdrplay_api_GetDeviceParams(device.dev, &deviceParams);
    if (err != sdrplay_api_Success)
    {
        SoapySDR_logf(SOAPY_SDR_ERROR, "GetDeviceParams Error: %s", sdrplay_api_GetErrorString(err));
        throw std::runtime_error("GetDeviceParams() failed");
    }

    if (thisDeviceParams)
    {
        if (hasDevParams) *deviceParams->devParams = devParams;
        if (hasRxChannelA) *deviceParams->rxChannelA = rxChannelA;
        if (hasRxChannelB) *deviceParams->rxChannelB = rxChannelB;
    }

    chParams = device.tuner == sdrplay_api_Tuner_B ? deviceParams->rxChannelB : deviceParams->rxChannelA;

    return;
}

void SoapySDRPlay::releaseDevice()
{
    sdrplay_api_ErrT err;
    if (selectedRSPDevices.count(rspDeviceId)) {
        sdrplay_api_DeviceT *currDevice = selectedRSPDevices.at(rspDeviceId);
        if (currDevice != &device) {
            // nothing to do - we are good
            return;
        }
        selectedRSPDevices.erase(rspDeviceId);
        sdrplay_api_LockDeviceApi();
        err = sdrplay_api_ReleaseDevice(currDevice);
        if (err != sdrplay_api_Success)
        {
            sdrplay_api_UnlockDeviceApi();
            SoapySDR_logf(SOAPY_SDR_ERROR, "ReleaseDevice Error: %s", sdrplay_api_GetErrorString(err));
            throw std::runtime_error("ReleaseDevice() failed");
        }
        sdrplay_api_UnlockDeviceApi();
    }

    return;
}
