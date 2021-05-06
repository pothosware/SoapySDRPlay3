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

/* Gain mode with RF 'gain' in dB
 *   - RF: RF gain in dB defined as: GAIN_MODE_RF_OFFSET_DB - RFGRdB (function of LNA state)
 *                 higher values mean more gain - range: varies
 *   - IF: IF gain in dB defined as: GAIN_MODE_IF_OFFSET_DB - IFGR
 *                 higher values mean more gain - range: 20-59 (or from -59 to -20)
 *
 * IMPORTANT: IF gain control is not available when AGC is enabled
 *            (an error message is returned)
 */


#include "SoapySDRPlay.hpp"
#include <cfloat>

#if defined(_M_X64) || defined(_M_IX86)
#define strcasecmp _stricmp
#elif defined (__GNUC__)
#include <strings.h>
#endif

#ifndef GAIN_MODE_IF_OFFSET_DB
#define GAIN_MODE_IF_OFFSET_DB 0
#endif
#ifndef GAIN_MODE_RF_OFFSET_DB
#define GAIN_MODE_RF_OFFSET_DB 0
#endif


std::vector<std::string> SoapySDRPlay::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("RF");
    if (chParams->ctrlParams.agc.enable == sdrplay_api_AGC_DISABLE)
    {
        results.push_back("IF");
    }

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

void SoapySDRPlay::setGain(const int direction, const size_t channel, const double value)
{
   if (chParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE) {
      SoapySDR_logf(SOAPY_SDR_ERROR, "error in setGain() - function is disabled when AGC is enabled");
      return;
   }

   // partition the requested gain between RF and IF proportionally to their range
   SoapySDR::Range rfRange = getGainRange(direction, channel, "RF");
   SoapySDR::Range ifRange = getGainRange(direction, channel, "IF");
   double minRfGain = rfRange.minimum();
   double maxRfGain = rfRange.maximum();
   double minGain = minRfGain + ifRange.minimum();
   double maxGain = maxRfGain + ifRange.maximum();
   // do not change the gain if it is out of range
   if (value < minGain || value > maxGain) {
      SoapySDR_logf(SOAPY_SDR_ERROR, "error in setGain() - gain=%lf is out of range=[%lf,%lf]", value, minGain, maxGain);
      return;
   }
   double normalizedGain = (value - minGain) / (maxGain - minGain);
   double rfGain = minRfGain + normalizedGain * (maxRfGain - minRfGain);
   setGain(direction, channel, "RF", rfGain);
   rfGain = getGain(direction, channel, "RF");
   double ifGain = value - rfGain;
   setGain(direction, channel, "IF", ifGain);
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (name == "IF" && chParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE) {
      SoapySDR_logf(SOAPY_SDR_ERROR, "error in setGain(%s) - function is disabled when AGC is enabled", name.c_str());
   }

   // do not change the gain if it is out of range
   SoapySDR::Range range = getGainRange(direction, channel, name);
   if (value < range.minimum() || value > range.maximum()) {
      SoapySDR_logf(SOAPY_SDR_ERROR, "error in setGain(%s) - gain=%lf is out of range=[%lf,%lf]", name.c_str(), value, range.minimum(), range.maximum());
      return;
   }

   bool doUpdate = false;

   if (name == "RF")
   {
      double rfgRdB = GAIN_MODE_RF_OFFSET_DB - value;
      // find the closest LNA state
      int LNAstate = 0;
      double minDiff = DBL_MAX;
      for (int i = 0; i <= maxLNAstate; i++) {
         double diff = abs(rfgRdB - LNAstateGainReductions[i]);
         if (diff < minDiff) {
             LNAstate = i;
             minDiff = diff;
         }
      }
      if (chParams->tunerParams.gain.LNAstate != LNAstate) {

          chParams->tunerParams.gain.LNAstate = LNAstate;
          doUpdate = true;
      }
   }
   else if (name == "IF" && chParams->ctrlParams.agc.enable == sdrplay_api_AGC_DISABLE)
   {
      //apply the change if the required value is different from gRdB 
      int ifgRdB = GAIN_MODE_IF_OFFSET_DB - (int) value;
      if (chParams->tunerParams.gain.gRdB != ifgRdB)
      {
         chParams->tunerParams.gain.gRdB = ifgRdB;
         doUpdate = true;
      }
   }
   if (doUpdate && streamActive)
   {
      sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
   }
}

double SoapySDRPlay::getGain(const int direction, const size_t channel) const
{
   return getGain(direction, channel, "RF") + getGain(direction, channel, "IF");
}

double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (name == "IF" && chParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE) {
      SoapySDR_logf(SOAPY_SDR_ERROR, "error in getGain(%s) - function is disabled when AGC is enabled", name.c_str());
   }

   if (name == "RF")
   {
      return GAIN_MODE_RF_OFFSET_DB - LNAstateGainReductions[chParams->tunerParams.gain.LNAstate];
   }
   else if (name == "IF")
   {
      return GAIN_MODE_IF_OFFSET_DB - chParams->tunerParams.gain.gRdB;
   }
   return 0;
}

SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
   if (name == "RF")
   {
      return SoapySDR::Range(
              GAIN_MODE_RF_OFFSET_DB - LNAstateGainReductions[maxLNAstate],
              GAIN_MODE_RF_OFFSET_DB - LNAstateGainReductions[0]);
   }
   else if (name == "IF")
   {
      if (chParams->ctrlParams.agc.enable == sdrplay_api_AGC_DISABLE)
      {
         return SoapySDR::Range(
                 GAIN_MODE_IF_OFFSET_DB - MAX_BB_GR,
                 GAIN_MODE_IF_OFFSET_DB - sdrplay_api_NORMAL_MIN_GR);
      }
   }
   SoapySDR_logf(SOAPY_SDR_ERROR, "error in getGainRange() - gain=%s is unknown or unavailable", name.c_str());
   return SoapySDR::Range(0, 0);
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
