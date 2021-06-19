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

#if GAIN_MODE_LEGACY_GENERIC_GAIN == 1
void SoapySDRPlay::setGain(const int direction, const size_t channel, const double value)
{
   int rfGR = (int) value / 100;
   int ifGR = (int) value % 100;
   setGain(direction, channel, "RFGR", rfGR);
   if (ifGR == 0) {
      setGainMode(direction, channel, true);
   }
   else
   {
      setGain(direction, channel, "IFGR", ifGR);
   }
}
#elif GAIN_MODE_LEGACY_GENERIC_GAIN == 2
void SoapySDRPlay::setGain(const int direction, const size_t channel, const double value)
{
   int rfGR = (int) value;
   int ifGR = (int) (100 * (value - rfGR + 0.00001));
   setGain(direction, channel, "RFGR", rfGR);
   if (ifGR == 0) {
      setGainMode(direction, channel, true);
   }
   else
   {
      setGain(direction, channel, "IFGR", ifGR);
   }
}
#endif /* GAIN_MODE_LEGACY_GENERIC_GAIN */

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

#if GAIN_MODE_LEGACY_GENERIC_GAIN == 1
double SoapySDRPlay::getGain(const int direction, const size_t channel) const
{
   double rfGR = getGain(direction, channel, "RFGR");
   double ifGR = getGain(direction, channel, "IFGR");
   return rfGR * 100 + ifGR;
}
#elif GAIN_MODE_LEGACY_GENERIC_GAIN == 2
double SoapySDRPlay::getGain(const int direction, const size_t channel) const
{
   double rfGR = getGain(direction, channel, "RFGR");
   double ifGR = getGain(direction, channel, "IFGR");
   return rfGR + ifGR / 100.0;
}
#endif

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


/* RfGainSetting methods */
std::string SoapySDRPlay::getRfGainSettingName() const
{
   return "RF Gain Select";
}

int *SoapySDRPlay::getRfGainSettingOptions(int &length, int &defaultValue) const
{
   static int options[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
   switch(device.hwVer)
   {
      case SDRPLAY_RSP1_ID:
         length = 3 + 1;
         defaultValue = options[1];
         break;
      case SDRPLAY_RSP1A_ID:
         length = 9 + 1;
         defaultValue = options[4];
         break;
      case SDRPLAY_RSP2_ID:
         length = 8 + 1;
         defaultValue = options[4];
         break;
      case SDRPLAY_RSPduo_ID:
         length = 9 + 1;
         defaultValue = options[4];
         break;
      case SDRPLAY_RSPdx_ID:
         length = 27 + 1;
         defaultValue = options[4];
         break;
      default:
         length = 1;
         defaultValue = options[0];
         break;
   }
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
