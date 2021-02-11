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

static std::map<std::string, SoapySDR::Kwargs> _cachedResults;

static std::vector<SoapySDR::Kwargs> findSDRPlay(const SoapySDR::Kwargs &args)
{
   std::vector<SoapySDR::Kwargs> results;
   unsigned int nDevs = 0;
   char lblstr[128];

   std::string baseLabel = "SDRplay Dev";

   // list devices by API
   SoapySDRPlay::sdrplay_api::get_instance();
   sdrplay_api_LockDeviceApi();
   sdrplay_api_DeviceT rspDevs[SDRPLAY_MAX_DEVICES];
   sdrplay_api_GetDevices(&rspDevs[0], &nDevs, SDRPLAY_MAX_DEVICES);

   for (unsigned int i = 0; i < nDevs; i++)
   {
      SoapySDR::Kwargs dev;
      dev["serial"] = rspDevs[i].SerNo;
      const bool serialMatch = args.count("serial") == 0 or args.at("serial") == dev["serial"];
      if (not serialMatch) continue;
      std::string modelName;
      if (rspDevs[i].hwVer == SDRPLAY_RSP1_ID)
      {
         modelName = "RSP1";
      }
      else if (rspDevs[i].hwVer == SDRPLAY_RSP1A_ID)
      {
         modelName = "RSP1A";
      }
      else if (rspDevs[i].hwVer == SDRPLAY_RSP2_ID)
      {
         modelName = "RSP2";
      }
      else if (rspDevs[i].hwVer == SDRPLAY_RSPdx_ID)
      {
         modelName = "RSPdx";
      }
      else
      {
         modelName = "UNKNOWN";
      }
      if (rspDevs[i].hwVer != SDRPLAY_RSPduo_ID)
      {
         sprintf_s(lblstr, sizeof(lblstr), "SDRplay Dev%ld %s %s", results.size(), modelName.c_str(), rspDevs[i].SerNo);
         dev["label"] = lblstr;
         results.push_back(dev);
         _cachedResults[dev["serial"]] = dev;
         continue;
      }

      // RSPduo case
      modelName = "RSPduo";
      if (rspDevs[i].rspDuoMode & sdrplay_api_RspDuoMode_Single_Tuner)
      {
         dev["mode"] = "ST";
         const bool modeMatch = args.count("mode") == 0 or args.at("mode") == dev["mode"];
         if (modeMatch)
         {
            sprintf_s(lblstr, sizeof(lblstr), "SDRplay Dev%ld %s %s - Single Tuner", results.size(), modelName.c_str(), rspDevs[i].SerNo);
            dev["label"] = lblstr;
            results.push_back(dev);
            _cachedResults[dev["serial"] + "@" + dev["mode"]] = dev;
         }
      }
      if (rspDevs[i].rspDuoMode & sdrplay_api_RspDuoMode_Dual_Tuner)
      {
         dev["mode"] = "DT";
         const bool modeMatch = args.count("mode") == 0 or args.at("mode") == dev["mode"];
         if (modeMatch)
         {
            sprintf_s(lblstr, sizeof(lblstr), "SDRplay Dev%ld %s %s - Dual Tuner", results.size(), modelName.c_str(), rspDevs[i].SerNo);
            dev["label"] = lblstr;
            results.push_back(dev);
            _cachedResults[dev["serial"] + "@" + dev["mode"]] = dev;
         }
      }
      if (rspDevs[i].rspDuoMode & sdrplay_api_RspDuoMode_Master)
      {
         dev["mode"] = "MA";
         const bool modeMatch = args.count("mode") == 0 or args.at("mode") == dev["mode"];
         if (modeMatch)
         {
            sprintf_s(lblstr, sizeof(lblstr), "SDRplay Dev%ld %s %s - Master", results.size(), modelName.c_str(), rspDevs[i].SerNo);
            dev["label"] = lblstr;
            results.push_back(dev);
            _cachedResults[dev["serial"] + "@" + dev["mode"]] = dev;
         }
      }
      if (rspDevs[i].rspDuoMode & sdrplay_api_RspDuoMode_Master)
      {
         dev["mode"] = "MA8";
         const bool modeMatch = args.count("mode") == 0 or args.at("mode") == dev["mode"];
         if (modeMatch)
         {
            sprintf_s(lblstr, sizeof(lblstr), "SDRplay Dev%ld %s %s - Master (RSPduo sample rate=8Mhz)", results.size(), modelName.c_str(), rspDevs[i].SerNo);
            dev["label"] = lblstr;
            results.push_back(dev);
            _cachedResults[dev["serial"] + "@" + dev["mode"]] = dev;
         }
      }
      if (rspDevs[i].rspDuoMode & sdrplay_api_RspDuoMode_Slave)
      {
         dev["mode"] = "SL";
         const bool modeMatch = args.count("mode") == 0 or args.at("mode") == dev["mode"];
         if (modeMatch)
         {
            sprintf_s(lblstr, sizeof(lblstr), "SDRplay Dev%ld %s %s - Slave", results.size(), modelName.c_str(), rspDevs[i].SerNo);
            dev["label"] = lblstr;
            results.push_back(dev);
            _cachedResults[dev["serial"] + "@" + dev["mode"]] = dev;
         }
      }
   }

   sdrplay_api_UnlockDeviceApi();

   // fill in the cached results for claimed handles
   for (const auto &serial : SoapySDRPlay_getClaimedSerials())
   {
      if (_cachedResults.count(serial) == 0) continue;
      if (args.count("serial") != 0)
      {
         std::string cacheKey = args.at("serial");
         if (args.count("mode") != 0) cacheKey += "@" + args.at("mode");
         if (cacheKey != serial) continue;
      }
      results.push_back(_cachedResults.at(serial));
   }

   return results;
}

static SoapySDR::Device *makeSDRPlay(const SoapySDR::Kwargs &args)
{
    return new SoapySDRPlay(args);
}

static SoapySDR::Registry registerSDRPlay("sdrplay", &findSDRPlay, &makeSDRPlay, SOAPY_SDR_ABI_VERSION);
