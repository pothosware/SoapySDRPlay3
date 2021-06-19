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

/* Gain mode with RF 'gain' using raw LNA state values
 *   - LNA: RF gain defined as: maxLNAstate - LNAstate
 *                 higher values mean more gain - range: 0-varies (or from -varies to 0)
 *   - IF:  IF gain in dB defined as: GAIN_MODE_IF_OFFSET_DB - IFGR
 *                 higher values mean more gain - range: 20-59 (or from -59 to -20)
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


std::vector<std::string> SoapySDRPlay::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("LNA");
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

void SoapySDRPlay::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);

   // do not change the gain if it is out of range
   SoapySDR::Range range = getGainRange(direction, channel, name);
   if (value < range.minimum() || value > range.maximum()) {
      SoapySDR_logf(SOAPY_SDR_ERROR, "error in setGain(%s) - gain=%lf is out of range=[%lf,%lf]", name.c_str(), value, range.minimum(), range.maximum());
      return;
   }

   bool doUpdate = false;

   if (name == "LNA")
   {
#ifdef GAIN_MODE_LNA_POSITIVE
      int LNAstate = maxLNAstate - (int) value;
#else
      int LNAstate = -(int) value;
#endif
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

double SoapySDRPlay::getGain(const int direction, const size_t channel, const std::string &name) const
{
   std::lock_guard <std::mutex> lock(_general_state_mutex);

   if (name == "LNA")
   {
#ifdef GAIN_MODE_LNA_POSITIVE
      return maxLNAstate - chParams->tunerParams.gain.LNAstate;
#else
      return -chParams->tunerParams.gain.LNAstate;
#endif
   }
   else if (name == "IF")
   {
      return GAIN_MODE_IF_OFFSET_DB - chParams->tunerParams.gain.gRdB;
   }
   return 0;
}

SoapySDR::Range SoapySDRPlay::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
   if (name == "LNA")
   {
#ifdef GAIN_MODE_LNA_POSITIVE
      return SoapySDR::Range(0, maxLNAstate);
#else
      return SoapySDR::Range(-maxLNAstate, 0);
#endif
   }
   else if (name == "IF")
   {
      return SoapySDR::Range(
              GAIN_MODE_IF_OFFSET_DB - MAX_BB_GR,
              GAIN_MODE_IF_OFFSET_DB - sdrplay_api_NORMAL_MIN_GR);
   }
   return SoapySDR::Range(0, 0);
}


/* RfGainSetting methods */
std::string SoapySDRPlay::getRfGainSettingName() const
{
   return "LNA state";
}

int *SoapySDRPlay::getRfGainSettingOptions(int &length, int &defaultValue) const
{
#ifdef GAIN_MODE_LNA_POSITIVE
   static int options[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
   length = maxLNAstate + 1;
   defaultValue = options[length / 2];
   return options;
#else
   static int options[] = {-27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1, 0};
   int optionsLength = sizeof(options) / sizeof(options[0]);
   length = maxLNAstate + 1;
   defaultValue = options[optionsLength - length / 2];
   return options + (optionsLength - length);
#endif
}

int SoapySDRPlay::readRfGainSetting() const
{
#ifdef GAIN_MODE_LNA_POSITIVE
   return static_cast<int>(maxLNAstate - chParams->tunerParams.gain.LNAstate);
#else
   return static_cast<int>(-chParams->tunerParams.gain.LNAstate);
#endif
}

void SoapySDRPlay::writeRfGainSetting(int value)
{
#ifdef GAIN_MODE_LNA_POSITIVE
   int LNAstate = maxLNAstate - value;
#else
   int LNAstate = -value;
#endif
   chParams->tunerParams.gain.LNAstate = static_cast<unsigned char>(LNAstate);
   return;
}
