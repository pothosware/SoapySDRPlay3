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

/* Gain mode as proposed by SDRplay here: https://github.com/cjcliffe/CubicSDR/issues/825
 *   - IFGR (hidden): IF gain reduction in dB
 *                             higher values mean less gain - range: 20-59
 *   - IF:            IF gain in dB defined as: 79 - IFGR
 *                             higher values mean more gain - range: 20-59
 *   - RFGR (hidden): RF gain reduction as LNA state
 *                             higher values mean less gain - range: 0-varies
 *   - RF:            RF gain defined as: maxLNAstate - LNAstate
 *                             higher values mean more gain - range: 0-varies
 */


#include "SoapySDRPlay.hpp"

#if defined(_M_X64) || defined(_M_IX86)
#define strcasecmp _stricmp
#elif defined (__GNUC__)
#include <strings.h>
#endif


#define GAIN_STEPS (29)

std::vector<std::string> SoapySDRPlay::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    // only return the gains (not the GRs)
    //results.push_back("IFGR");
    results.push_back("IF");
    //results.push_back("RFGR");
    results.push_back("RF");

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

static int getMaxRFGR(unsigned char hwVer)
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

void SoapySDRPlay::setGain(const int direction, const size_t channel, const double value)
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);
   bool doUpdate = false;

   const uint8_t rsp1_lnastates[] = { 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1,
       2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
   const uint8_t rsp1_ifgains[] = { 59, 56, 53, 50, 47, 44, 41, 58, 55, 52, 49,
       46, 43, 45, 42, 58, 55, 52, 49, 46, 43, 41, 38, 35, 32, 29, 26, 23, 20 };
   const uint8_t rsp1a_lnastates[] = { 9, 9, 9, 9, 9, 9, 8, 7, 7, 7, 7, 7, 6,
       6, 5, 5, 4, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
   const uint8_t rsp1a_ifgains[] = { 59, 55, 52, 48, 45, 41, 42, 58, 54, 51, 47,
       43, 46, 42, 44, 41, 43, 42, 44, 40, 43, 45, 42, 38, 34, 31, 27, 24, 20 };
   const uint8_t rsp2_lnastates[] = { 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 5, 5,
       4, 4, 4, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
   const uint8_t rsp2_ifgains[] = { 59, 55, 52, 48, 44, 41, 56, 52, 49, 45, 41,
       44, 45, 41, 48, 44, 40, 45, 42, 43, 49, 46, 42, 38, 35, 31, 27, 24, 20 };
   const uint8_t rspduo_lnastates[] = { 9, 9, 9, 9, 9, 9, 8, 7, 7, 7, 7, 7, 6,
       6, 5, 5, 4, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
   const uint8_t rspduo_ifgains[] = { 59, 55, 52, 48, 45, 41, 42, 58, 54, 51,
       47, 43, 46, 42, 44, 41, 43, 42, 44, 40, 43, 45, 42, 38, 34, 31, 27, 24,
       20 };
   const uint8_t rspdx_lnastates[] = { 26, 26, 26, 26, 26, 25, 23, 22, 20, 19,
       17, 16, 14, 13, 11, 10, 8, 7, 5, 5, 5, 3, 2, 0, 0, 0, 0, 0, 0 };
   const uint8_t rspdx_ifgains[] = { 59, 55, 50, 46, 41, 40, 42, 40, 42, 40, 42,
       41, 42, 41, 43, 41, 43, 41, 49, 45, 40, 42, 40, 42, 38, 33, 29, 24, 20 };

   int gain = std::min(std::max(0, (int)value), GAIN_STEPS - 1);
   int rfGR = 0;
   int ifGR = 0;

   switch(device.hwVer)
   {
      case SDRPLAY_RSP1_ID:
         rfGR = rsp1_lnastates[gain];
         ifGR = rsp1_ifgains[gain];
         break;
      case SDRPLAY_RSP1A_ID:
         rfGR = rsp1a_lnastates[gain];
         ifGR = rsp1a_ifgains[gain];
         break;
      case SDRPLAY_RSP2_ID:
         rfGR = rsp2_lnastates[gain];
         ifGR = rsp2_ifgains[gain];
         break;
      case SDRPLAY_RSPduo_ID:
         rfGR = rspduo_lnastates[gain];
         ifGR = rspduo_ifgains[gain];
         break;
      case SDRPLAY_RSPdx_ID:
         rfGR = rspdx_lnastates[gain];
         ifGR = rspdx_ifgains[gain];
         break;
   }

   if (chParams->tunerParams.gain.gRdB != ifGR && chParams->ctrlParams.agc.enable == sdrplay_api_AGC_DISABLE)
   {
      chParams->tunerParams.gain.gRdB = ifGR;
      doUpdate = true;
   }

   if (chParams->tunerParams.gain.LNAstate != rfGR)
   {
      chParams->tunerParams.gain.LNAstate = rfGR;
      doUpdate = true;
   }

   if (doUpdate && streamActive)
   {
      gr_changed = 0;
      sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Tuner_Gr,
                         sdrplay_api_Update_Ext1_None);
      for (int i = 0; i < updateTimeout && gr_changed == 0; ++i)
      {
         std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      if (gr_changed == 0)
      {
         SoapySDR_log(SOAPY_SDR_WARNING, "Gain reduction update timeout.");
      }
   }
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   bool doUpdate = false;

   if (chParams->ctrlParams.agc.enable == sdrplay_api_AGC_DISABLE)
   {
      if (name == "IFGR" || name == "IF")
      {
         return;
      }
   }

   if (name == "IF" || name == "IFGR")
   {
      int ifGR = (name == "IF") ? 20 + (59 - value) : value;
      if (chParams->tunerParams.gain.gRdB != ifGR)
      {
         chParams->tunerParams.gain.gRdB = ifGR;
         doUpdate = true;
      }
   }
   else if (name == "RF" || name == "RFGR")
   {
      int rfGR = (name == "RF") ? getMaxRFGR(device.hwVer) - value : value;
      if (chParams->tunerParams.gain.LNAstate != rfGR) {

          chParams->tunerParams.gain.LNAstate = rfGR;
          doUpdate = true;
      }
   }

   if (doUpdate && streamActive)
   {
      gr_changed = 0;
      sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Tuner_Gr,
                         sdrplay_api_Update_Ext1_None);
      for (int i = 0; i < updateTimeout && gr_changed == 0; ++i)
      {
         std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      if (gr_changed == 0)
      {
         SoapySDR_log(SOAPY_SDR_WARNING, "Gain reduction update timeout.");
      }
   }
}

double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
{
    std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (name == "IFGR")
   {
       return chParams->tunerParams.gain.gRdB;
   }
   else if (name == "IF")
   {
       return 20 + (59 - chParams->tunerParams.gain.gRdB);
   }
   else if (name == "RFGR")
   {
      return chParams->tunerParams.gain.LNAstate;
   }
   else if (name == "RF")
   {
      return getMaxRFGR(device.hwVer) - chParams->tunerParams.gain.LNAstate;
   }

   return 0;
}

SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
   if (name == "IFGR" || name == "IF")
   {
      return SoapySDR::Range(20, 59);
   }
   else if (name == "RFGR" || name == "RF")
   {
      return SoapySDR::Range(0, getMaxRFGR(device.hwVer));
   }
   return SoapySDR::Range(20, 59);
}


/* RfGainSetting methods */
std::string SoapySDRPlay::getRfGainSettingName() const
{
   return "RF Gain Select";
}

int *SoapySDRPlay::getRfGainSettingOptions(int &length, int &defaultValue) const
{
   static int options[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
   int maxRFGR = getMaxRFGR(device.hwVer);
   length = maxRFGR + 1;
   defaultValue = maxRFGR > options[4] ? options[4] : options[1];
   return options;
}

int SoapySDRPlay::readRfGainSetting() const
{
   return static_cast<int>(chParams->tunerParams.gain.LNAstate);
}

void SoapySDRPlay::writeRfGainSetting(int value)
{
   chParams->tunerParams.gain.LNAstate = static_cast<unsigned char>(value);
   return;
}
