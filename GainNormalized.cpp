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

/* Gain mode with both gains normalized to a scale 0-100
 *   - RF: RF gain normalized to a scale 0-100
 *                 higher values mean more gain - range: [0-100]
 *   - IF: IF gain normalized to a scale 0-100
 *                 higher values mean more gain - range: [0-100]
 */


#include "SoapySDRPlay.hpp"
#include <cfloat>

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

    results.push_back("RF");
    results.push_back("IF");

    return results;
}

#ifndef GAIN_MODE_IF_AGC_AS_SETTING
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
#endif /* !GAIN_MODE_IF_AGC_AS_SETTING */

void SoapySDRPlay::setGain(const int direction, const size_t channel, const double value)
{
   setGain(direction, channel, "RF", value);
   setGain(direction, channel, "IF", value);
}

void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);

   // do not change the gain if it is out of range
   if (value < 0 || value > 100) {
      SoapySDR_logf(SOAPY_SDR_ERROR, "error in setGain(%s) - gain=%lf is out of range=[0,100]", name.c_str(), value);
      return;
   }

   bool doUpdate = false;

   double normalizedGR = 1.0 - value / 100;
   if (name == "RF")
   {
      double rfgRdB = LNAstateGainReductions[0] + normalizedGR * (LNAstateGainReductions[maxLNAstate] - LNAstateGainReductions[0]);
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
      int ifgRdB = (int) (sdrplay_api_NORMAL_MIN_GR + normalizedGR * (MAX_BB_GR - sdrplay_api_NORMAL_MIN_GR + 0.4999));
      if (chParams->tunerParams.gain.gRdB != ifgRdB)
      {
         chParams->tunerParams.gain.gRdB = ifgRdB;
         doUpdate = true;
      }
   }
   if (doUpdate && streamActive)
   {
      gr_changed = 0;
      sdrplay_api_Update(device.dev, device.tuner, sdrplay_api_Update_Tuner_Gr, sdrplay_api_Update_Ext1_None);
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

double SoapySDRPlay::getGain(const int direction, const size_t channel) const
{
   double rfGR = LNAstateGainReductions[chParams->tunerParams.gain.LNAstate] - LNAstateGainReductions[0];
   double maxRfGR = LNAstateGainReductions[maxLNAstate] - LNAstateGainReductions[0];
   double ifGR = chParams->tunerParams.gain.gRdB - sdrplay_api_NORMAL_MIN_GR;
   double maxIfGR = MAX_BB_GR - sdrplay_api_NORMAL_MIN_GR;
   double normalizedGR = (rfGR + ifGR) / (maxRfGR + maxIfGR);
   return 100 * (1 - normalizedGR);
}

double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (name == "RF")
   {
      double rfGR = LNAstateGainReductions[chParams->tunerParams.gain.LNAstate] - LNAstateGainReductions[0];
      double maxRfGR = LNAstateGainReductions[maxLNAstate] - LNAstateGainReductions[0];
      double normalizedGR = rfGR / maxRfGR;
      return 100 * (1 - normalizedGR);
   }
   else if (name == "IF")
   {
      double ifGR = chParams->tunerParams.gain.gRdB - sdrplay_api_NORMAL_MIN_GR;
      double maxIfGR = MAX_BB_GR - sdrplay_api_NORMAL_MIN_GR;
      double normalizedGR = ifGR / maxIfGR;
      return 100 * (1 - normalizedGR);
   }
   return 0;
}

SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
   return SoapySDR::Range(0, 100);
}


/* RfGainSetting methods */
std::string SoapySDRPlay::getRfGainSettingName() const
{
   return "RF Gain (%)";
}

int *SoapySDRPlay::getRfGainSettingOptions(int &length, int &defaultValue) const
{
   static int options[] = {0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 100};
   length = sizeof(options) / sizeof(options[0]);
   defaultValue = options[length / 2];
   return options;
}

int SoapySDRPlay::readRfGainSetting() const
{
   double rfGR = LNAstateGainReductions[chParams->tunerParams.gain.LNAstate] - LNAstateGainReductions[0];
   double maxRfGR = LNAstateGainReductions[maxLNAstate] - LNAstateGainReductions[0];
   double normalizedGR = rfGR / maxRfGR;
   double rfGain = 100 * (1 - normalizedGR);

   // return the closest value among the available setting options
   int rfGainSettingOptionsLength;
   int rfGainSettingDefault;
   int *rfGainSettingOptions = getRfGainSettingOptions(rfGainSettingOptionsLength, rfGainSettingDefault);
   int rfGainSetting = 0;
   double minDiff = DBL_MAX;
   for (int i = 0; i < rfGainSettingOptionsLength; i++) {
      double diff = abs(rfGain - rfGainSettingOptions[i]);
      if (diff < minDiff) {
          rfGainSetting = rfGainSettingOptions[i];
          minDiff = diff;
      }
   }
   return rfGainSetting;
}

void SoapySDRPlay::writeRfGainSetting(int value)
{
   double normalizedGR = 1.0 - (double) value / 100;
   double rfgRdB = LNAstateGainReductions[0] + normalizedGR * (LNAstateGainReductions[maxLNAstate] - LNAstateGainReductions[0]);
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
   chParams->tunerParams.gain.LNAstate = static_cast<unsigned char>(LNAstate);
   return;
}