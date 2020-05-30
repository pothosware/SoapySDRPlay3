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
#include <SoapySDR/Registry.hpp>

#if !defined(_M_X64) && !defined(_M_IX86)
#define sprintf_s(buffer, buffer_size, stringbuffer, ...) (sprintf(buffer, stringbuffer, __VA_ARGS__))
#endif

static sdrplay_api_DeviceT rspDevs[SDRPLAY_MAX_DEVICES];
sdrplay_api_DeviceT *deviceSelected = nullptr;
SoapySDR::Stream *activeStream = nullptr;
SoapySDRPlay *activeSoapySDRPlay = nullptr;

static std::vector<SoapySDR::Kwargs> findSDRPlay(const SoapySDR::Kwargs &args)
{
   std::vector<SoapySDR::Kwargs> results;
   std::string labelHint;
   if (args.count("label") != 0) labelHint = args.at("label");

   sdrplay_api_RspDuoModeT rspDuoModeHint = sdrplay_api_RspDuoMode_Unknown;
   if (args.count("rspduo_mode") != 0)
   {
      try
      {
         rspDuoModeHint = (sdrplay_api_RspDuoModeT) stoi(args.at("rspduo_mode"));
      }
      catch (std::invalid_argument&)
      {
         rspDuoModeHint = SoapySDRPlay::stringToRSPDuoMode(args.at("rspduo_mode"));
         if (rspDuoModeHint == sdrplay_api_RspDuoMode_Unknown)
         {
            throw;
         }
      }
   }
   bool isMasterAt8MhzHint = false;
   if (args.count("rspduo_sample_freq") != 0)
   {
      isMasterAt8MhzHint = args.at("rspduo_sample_freq") == "8";
   }

   unsigned int nDevs = 0;
   char lblstr[128];

   SoapySDRPlay::sdrplay_api::get_instance();

   sdrplay_api_LockDeviceApi();

   if (activeStream)
   {
      SoapySDR_log(SOAPY_SDR_WARNING, "findSDRPlay() called while the device is streaming. Ignoring request.");
      return results;
   }

   if (deviceSelected)
   {
      sdrplay_api_ReleaseDevice(deviceSelected);
      deviceSelected = nullptr;
   }

   std::string baseLabel = "SDRplay Dev";

   // list devices by API
   sdrplay_api_GetDevices(&rspDevs[0], &nDevs, SDRPLAY_MAX_DEVICES);

   size_t posidx = labelHint.find(baseLabel);

   int labelDevIdx = -1;
   if (posidx != std::string::npos)
      labelDevIdx = labelHint.at(posidx + baseLabel.length()) - 0x30;

   int devIdx = 0;
   for (unsigned int i = 0; i < nDevs; ++i)
   {
      switch (rspDevs[i].hwVer)
      {
      case SDRPLAY_RSP1_ID:
      case SDRPLAY_RSP1A_ID:
      case SDRPLAY_RSP2_ID:
      case SDRPLAY_RSPdx_ID:
         if (labelDevIdx < 0 || devIdx == labelDevIdx)
         {
            SoapySDR::Kwargs dev;
            dev["driver"] = "sdrplay";
            sprintf_s(lblstr, 128, "%s%d %s %.*s",
                      baseLabel.c_str(), devIdx,
                      SoapySDRPlay::HWVertoString(rspDevs[i].hwVer).c_str(),
                      SDRPLAY_MAX_SER_NO_LEN, rspDevs[i].SerNo);
            dev["label"] = lblstr;
            results.push_back(dev);
         }
         ++devIdx;
         break;
      case SDRPLAY_RSPduo_ID:
         struct {
            sdrplay_api_RspDuoModeT rspDuoMode; bool isMasterAt8Mhz;
         } modes[] = {
            { sdrplay_api_RspDuoMode_Single_Tuner, false },
            { sdrplay_api_RspDuoMode_Dual_Tuner, false },
            { sdrplay_api_RspDuoMode_Master, false },
            { sdrplay_api_RspDuoMode_Master, true },
            { sdrplay_api_RspDuoMode_Slave, false }
         };
         for (auto mode : modes)
         {
            if (rspDevs[i].rspDuoMode & mode.rspDuoMode)
            {
               if ((labelDevIdx < 0 || devIdx == labelDevIdx) &&
                   (rspDuoModeHint == sdrplay_api_RspDuoMode_Unknown ||
                    mode.rspDuoMode == rspDuoModeHint) &&
                   (mode.rspDuoMode != sdrplay_api_RspDuoMode_Master ||
                    (!isMasterAt8MhzHint || mode.isMasterAt8Mhz)))
               {
                  SoapySDR::Kwargs dev;
                  dev["driver"] = "sdrplay";
                  sprintf_s(lblstr, 128, "%s%d %s %.*s - %s%s",
                            baseLabel.c_str(), devIdx,
                            SoapySDRPlay::HWVertoString(rspDevs[i].hwVer).c_str(),
                            SDRPLAY_MAX_SER_NO_LEN, rspDevs[i].SerNo,
                            SoapySDRPlay::RSPDuoModetoString(mode.rspDuoMode).c_str(),
                            mode.rspDuoMode == sdrplay_api_RspDuoMode_Master && mode.isMasterAt8Mhz ? " (RSPduo sample rate=8Mhz)" : "");
                  dev["label"] = lblstr;
                  dev["rspduo_mode"] = std::to_string(mode.rspDuoMode);
                  if (mode.isMasterAt8Mhz)
                  {
                     dev["rspduo_sample_freq"] = "8";
                  }
                  results.push_back(dev);
               }
               ++devIdx;
            }
         }
         break;
      }
   }

   sdrplay_api_UnlockDeviceApi();

   return results;
}

static SoapySDR::Device *makeSDRPlay(const SoapySDR::Kwargs &args)
{
    return new SoapySDRPlay(args);
}

static SoapySDR::Registry registerSDRPlay("sdrplay", &findSDRPlay, &makeSDRPlay, SOAPY_SDR_ABI_VERSION);
