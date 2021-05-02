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

/* "Legacy" gain mode:
 *   - IFGR: IF gain reduction in dB
 *                   higher values mean less gain - range: 20-59
 *   - RFGR: RF gain reduction as LNA state
 *                   higher values mean less gain - range: 0-varies
 */


#include "SoapySDRPlay.hpp"

#if defined(_M_X64) || defined(_M_IX86)
#define strcasecmp _stricmp
#elif defined (__GNUC__)
#include <strings.h>
#endif


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

int getMaxRFGR(unsigned char hwVer)
{
   switch(hwVer)
   {
      case SDRPLAY_RSP1_ID:
         return 3;
      case SDRPLAY_RSP1A_ID:
         return 9;
      case SDRPLAY_RSP2_ID:
         return 8;
      case SDRPLAY_RSPduo_ID:
         return 9;
      case SDRPLAY_RSPdx_ID:
         return 27;
      default:
         return 0;
   }
}
