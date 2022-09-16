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

#include <GainControls.hpp>
#include <chrono>
#include <thread>
#include <SoapySDR/Logger.hpp>

/* default implementations for the generic setGain(), getGain(), and getGainRange() */
bool GainControls::setGain(const int direction, const size_t channel, const double value)
{
    SoapySDR_log(SOAPY_SDR_ERROR, "setGain() is not implemented in this gain control mode");
    return false;
}

double GainControls::getGain(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_ERROR, "getGain() is not implemented in this gain control mode");
    return 0;
}

SoapySDR::Range GainControls::getGainRange(const int direction, const size_t channel) const
{
    SoapySDR_log(SOAPY_SDR_ERROR, "getGainRange() is not implemented in this gain control mode");
    return SoapySDR::Range();
}


/* "Legacy" gain mode:
 *   - IFGR: IF gain reduction in dB
 *                   higher values mean less gain - range: 20-59
 *   - RFGR: RF gain reduction as LNA state
 *                   higher values mean less gain - range: 0-varies
 */

std::vector<std::string> GainControlsLegacy::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("IFGR");
    results.push_back("RFGR");

    return results;
}

bool GainControlsLegacy::hasGainMode(const int direction, const size_t channel) const
{
    return true;
}

bool GainControlsLegacy::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    bool doUpdate = false;

    sdrplay_api_AgcControlT agc_control = automatic ? sdrplay_api_AGC_CTRL_EN : sdrplay_api_AGC_DISABLE;
    if (chParams->ctrlParams.agc.enable != agc_control)
    {
        chParams->ctrlParams.agc.enable = agc_control;
        doUpdate = true;
    }
    return doUpdate;
}

bool GainControlsLegacy::getGainMode(const int direction, const size_t channel) const
{
    return chParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE;
}

bool GainControlsLegacy::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    bool doUpdate = false;

    if (name == "IFGR")
    {
        if (chParams->ctrlParams.agc.enable == sdrplay_api_AGC_DISABLE)
        {
            // apply the change if the required value is different from gRdB
            if (chParams->tunerParams.gain.gRdB != (int)value)
            {
                chParams->tunerParams.gain.gRdB = (int)value;
                doUpdate = true;
            }
        }
        else
        {
            SoapySDR_log(SOAPY_SDR_WARNING, "Not updating IFGR gain because AGC is enabled");
        }
    }
    else if (name == "RFGR")
    {
        if (chParams->tunerParams.gain.LNAstate != (int)value) {
            chParams->tunerParams.gain.LNAstate = (int)value;
            doUpdate = true;
        }
    }
    return doUpdate;
}

double GainControlsLegacy::getGain(const int direction, const size_t channel, const std::string &name) const
{
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

SoapySDR::Range GainControlsLegacy::getGainRange(const int direction, const size_t channel, const std::string &name) const
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

std::string GainControlsLegacy::getRfGainSettingName() const
{
    return "RF Gain Select";
}

std::vector<int> GainControlsLegacy::getRfGainSettingOptions(int &defaultValue) const
{
    std::vector<int> options;
    switch(device.hwVer)
    {
        case SDRPLAY_RSP1_ID:
            options.resize(3+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[1];
            break;
        case SDRPLAY_RSP1A_ID:
            options.resize(9+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[4];
            break;
        case SDRPLAY_RSP2_ID:
            options.resize(8+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[4];
            break;
        case SDRPLAY_RSPduo_ID:
            options.resize(9+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[4];
            break;
        case SDRPLAY_RSPdx_ID:
            options.resize(27+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[4];
            break;
        default:
            defaultValue = -1;
            break;
    }
    return options;
}

int GainControlsLegacy::readRfGainSetting() const
{
    return static_cast<int>(chParams->tunerParams.gain.LNAstate);
}

void GainControlsLegacy::writeRfGainSetting(int value)
{
    chParams->tunerParams.gain.LNAstate = static_cast<unsigned char>(value);
    return;
}


/* GainReductionTable */

GainControlsWithGainReductionTable::GainControlsWithGainReductionTable(
                                    sdrplay_api_DeviceT &device,
                                    sdrplay_api_DeviceParamsT *deviceParams,
                                    sdrplay_api_RxChannelParamsT *chParams) :
    GainControls(device, chParams),
    deviceParams(deviceParams)
{
    if (device.hwVer == SDRPLAY_RSP1_ID) {
        gain_reduction_table = new GainReductionTableRSP1(this);
    } else if (device.hwVer == SDRPLAY_RSP1A_ID) {
        gain_reduction_table = new GainReductionTableRSP1A(this);
    } else if (device.hwVer == SDRPLAY_RSP2_ID) {
        gain_reduction_table = new GainReductionTableRSP2(this);
    } else if (device.hwVer == SDRPLAY_RSPduo_ID) {
        gain_reduction_table = new GainReductionTableRSPduo(this);
    } else if (device.hwVer == SDRPLAY_RSPdx_ID) {
        gain_reduction_table = new GainReductionTableRSPdx(this);
    } else {
        SoapySDR_logf(SOAPY_SDR_ERROR, "unknown RSP model %d. Aborting.", device.hwVer);
        throw std::runtime_error("unknown RSP model. Aborting.");
    }
}

int GainControlsWithGainReductionTable::GainReductionTable::gRdBToLNAstate(double rfgRdB, std::vector<int> &gain_reduction_row, bool exact)
{
    int LNAstate = -1;
    if (exact) {
        int rfgRdB_as_int = rfgRdB;
        // find the first LNA state that matches the requested gain reduction
        for (auto it = gain_reduction_row.begin(); it != gain_reduction_row.end(); it++) {
            if (*it == rfgRdB_as_int) {
                LNAstate = std::distance(gain_reduction_row.begin(), it);;
                break;
            }
        }
    } else {
        // find the closest LNA state
        double minDiff = 1e6;
        for (auto it = gain_reduction_row.begin(); it != gain_reduction_row.end(); it++) {
            double diff = abs(rfgRdB - *it);
            if (diff < minDiff) {
                LNAstate = std::distance(gain_reduction_row.begin(), it);;
                minDiff = diff;
            }
        }
    }
    return LNAstate;
}

std::vector<int> GainControlsWithGainReductionTable::GainReductionTableRSP1::getGainReductionRow(void)
{
    double rfHz = gain_controls->chParams->tunerParams.rfFreq.rfHz;
    if (rfHz < 420e6) {
        return std::vector<int>{ 0, 24, 19, 43 };
    } else if (rfHz < 1000e6) {
        return std::vector<int>{ 0, 7, 19, 26 };
    } else if (rfHz <= 2000e6) {
        return std::vector<int>{ 0, 5, 19, 24 };
    }
    return std::vector<int>();
}

std::vector<int> GainControlsWithGainReductionTable::GainReductionTableRSP1A::getGainReductionRow(void)
{
    double rfHz = gain_controls->chParams->tunerParams.rfFreq.rfHz;
    if (rfHz < 60e6) {
        return std::vector<int>{ 0, 6, 12, 18, 37, 42, 61 };
    } else if (rfHz < 420e6) {
        return std::vector<int>{ 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 };
    } else if (rfHz < 1000e6) {
        return std::vector<int>{ 0, 7, 13, 19, 20, 27, 33, 39, 45, 64 };
    } else if (rfHz <= 2000e6) {
        return std::vector<int>{ 0, 6, 12, 20, 26, 32, 38, 43, 62 };
    }
    return std::vector<int>();
}

std::vector<int> GainControlsWithGainReductionTable::GainReductionTableRSP2::getGainReductionRow(void)
{
    double rfHz = gain_controls->chParams->tunerParams.rfFreq.rfHz;
    sdrplay_api_Rsp2_AmPortSelectT amPortSel = gain_controls->chParams->rsp2TunerParams.amPortSel;
    if (rfHz < 420e6 && amPortSel == sdrplay_api_Rsp2_AMPORT_1) {
        return std::vector<int>{ 0, 6, 12, 18, 37 };
    } else if (rfHz < 420e6) {
        return std::vector<int>{ 0, 10, 15, 21, 24, 34, 39, 45, 64 };
    } else if (rfHz < 1000e6) {
        return std::vector<int>{ 0, 7, 10, 17, 22, 41 };
    } else if (rfHz <= 2000e6) {
        return std::vector<int>{ 0, 5, 21, 15, 15, 34 };
    }
    return std::vector<int>();
}

std::vector<int> GainControlsWithGainReductionTable::GainReductionTableRSPduo::getGainReductionRow(void)
{
    double rfHz = gain_controls->chParams->tunerParams.rfFreq.rfHz;
    sdrplay_api_RspDuo_AmPortSelectT tuner1AmPortSel = gain_controls->chParams->rspDuoTunerParams.tuner1AmPortSel;
    if (rfHz < 60e6 && tuner1AmPortSel == sdrplay_api_RspDuo_AMPORT_1) {
        return std::vector<int>{ 0, 6, 12, 18, 37 };
    } else if (rfHz < 60e6) {
        return std::vector<int>{ 0, 6, 12, 18, 37, 42, 61 };
    } else if (rfHz < 420e6) {
        return std::vector<int>{ 0, 6, 12, 18, 20, 26, 32, 38, 57, 62 };
    } else if (rfHz < 1000e6) {
        return std::vector<int>{ 0, 7, 13, 19, 20, 27, 33, 39, 45, 64 };
    } else if (rfHz <= 2000e6) {
        return std::vector<int>{ 0, 6, 12, 20, 26, 32, 38, 43, 62 };
    }
    return std::vector<int>();
}

std::vector<int> GainControlsWithGainReductionTable::GainReductionTableRSPdx::getGainReductionRow(void)
{
    double rfHz = gain_controls->chParams->tunerParams.rfFreq.rfHz;
    unsigned char hdrEnable = gain_controls->deviceParams->devParams->rspDxParams.hdrEnable;
    if (rfHz < 2e6 && hdrEnable) {
        return std::vector<int>{ 0, 3, 6, 9, 12, 15, 18, 21, 24, 25, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60 };
    } else if (rfHz < 12e6) {
        return std::vector<int>{ 0, 3, 6, 9, 12, 15, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60 };
    } else if (rfHz < 60e6) {
        return std::vector<int>{ 0, 3, 6, 9, 12, 15, 18, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60 };
    } else if (rfHz < 250e6) {
        return std::vector<int>{ 0, 3, 6, 9, 12, 15, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84 };
    } else if (rfHz < 420e6) {
        return std::vector<int>{ 0, 3, 6, 9, 12, 15, 18, 24, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81, 84 };
    } else if (rfHz < 1000e6) {
        return std::vector<int>{ 0, 7, 10, 13, 16, 19, 22, 25, 31, 34, 37, 40, 43, 46, 49, 52, 55, 58, 61, 64, 67 };
    } else if (rfHz <= 2000e6) {
        return std::vector<int>{ 0, 5, 8, 11, 14, 17, 20, 32, 35, 38, 41, 44, 47, 50, 53, 56, 59, 62, 65 };
    }
    return std::vector<int>();
}


/* Gain mode with RF 'gain' in dB
 *   - RF: RF gain in dB defined as: maxRFGRdB - RFGRdB (function of LNA state)
 *                 higher values mean more gain - range: varies
 *   - IF: IF gain in dB defined as: 79 - IFGR
 *                 higher values mean more gain - range: 20-59
 */

std::vector<std::string> GainControlsDB::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("RF");
    results.push_back("IF");

    return results;
}

bool GainControlsDB::hasGainMode(const int direction, const size_t channel) const
{
    return true;
}

bool GainControlsDB::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    bool doUpdate = false;

    sdrplay_api_AgcControlT agc_control = automatic ? sdrplay_api_AGC_CTRL_EN : sdrplay_api_AGC_DISABLE;
    if (chParams->ctrlParams.agc.enable != agc_control)
    {
        chParams->ctrlParams.agc.enable = agc_control;
        doUpdate = true;
    }
    return doUpdate;
}

bool GainControlsDB::getGainMode(const int direction, const size_t channel) const
{
    return chParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE;
}

bool GainControlsDB::setGain(const int direction, const size_t channel, const double value)
{
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
       return false;
    }
    double normalizedGain = (value - minGain) / (maxGain - minGain);
    double rfGain = minRfGain + normalizedGain * (maxRfGain - minRfGain);
    bool doUpdate = setGain(direction, channel, "RF", rfGain);
    rfGain = getGain(direction, channel, "RF");
    double ifGain = value - rfGain;
    doUpdate |= setGain(direction, channel, "IF", ifGain);
    return doUpdate;
}

bool GainControlsDB::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    // do not change the gain if it is out of range
    SoapySDR::Range range = getGainRange(direction, channel, name);
    if (value < range.minimum() || value > range.maximum()) {
        SoapySDR_logf(SOAPY_SDR_ERROR, "error in setGain(%s) - gain=%lf is out of range=[%lf,%lf]", name.c_str(), value, range.minimum(), range.maximum());
        return false;
    }

    bool doUpdate = false;

    if (name == "RF")
    {
        std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
        int gain_reduction_max = gain_reduction_row[gain_reduction_row.size() - 1];
        double rfgRdB = gain_reduction_max - value;
        int LNAstate = gain_reduction_table->gRdBToLNAstate(rfgRdB, gain_reduction_row);
        if (chParams->tunerParams.gain.LNAstate != LNAstate) {
             chParams->tunerParams.gain.LNAstate = LNAstate;
             doUpdate = true;
        }
    }
    if (name == "IF")
    {
        if (chParams->ctrlParams.agc.enable == sdrplay_api_AGC_DISABLE)
        {
            // apply the change if the required value is different from gRdB
            int ifgRdB = sdrplay_api_NORMAL_MIN_GR + MAX_BB_GR - (int) value;
            if (chParams->tunerParams.gain.gRdB != ifgRdB)
            {
                chParams->tunerParams.gain.gRdB = ifgRdB;
                doUpdate = true;
            }
        }
        else
        {
            SoapySDR_log(SOAPY_SDR_WARNING, "Not updating IF gain because AGC is enabled");
        }
    }
    return doUpdate;
}

double GainControlsDB::getGain(const int direction, const size_t channel) const
{
    return getGain(direction, channel, "RF") + getGain(direction, channel, "IF");
}

double GainControlsDB::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
        int gain_reduction_max = gain_reduction_row[gain_reduction_row.size() - 1];
        return gain_reduction_max - gain_reduction_row[chParams->tunerParams.gain.LNAstate];
    }
    else if (name == "IF")
    {
        return sdrplay_api_NORMAL_MIN_GR + MAX_BB_GR - chParams->tunerParams.gain.gRdB;
    }
    return 0;
}

SoapySDR::Range GainControlsDB::getGainRange(const int direction, const size_t channel) const
{
    SoapySDR::Range rf_gain_range = getGainRange(direction, channel, "RF");
    SoapySDR::Range if_gain_range = getGainRange(direction, channel, "IF");
    return SoapySDR::Range(rf_gain_range.minimum() + if_gain_range.minimum(),
                           rf_gain_range.maximum() + if_gain_range.maximum());
}

SoapySDR::Range GainControlsDB::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RF")
    {
        std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
        int gain_reduction_max = gain_reduction_row[gain_reduction_row.size() - 1];
        return SoapySDR::Range(gain_reduction_row[0], gain_reduction_max);
    }
    else if (name == "IF")
    {
        return SoapySDR::Range(sdrplay_api_NORMAL_MIN_GR, MAX_BB_GR);
    }
    return SoapySDR::Range(0, 0);
}

std::string GainControlsDB::getRfGainSettingName() const
{
    return "RF Gain (dB)";
}

std::vector<int> GainControlsDB::getRfGainSettingOptions(int &defaultValue) const
{
    std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
    std::vector<int> options(gain_reduction_row.size());
    int gain_reduction_max = gain_reduction_row[gain_reduction_row.size() - 1];
    for (int gain_reduction : gain_reduction_row) {
        options.push_back(gain_reduction_max - gain_reduction);
    }
    defaultValue = options[options.size() / 2];
    return options;
}

int GainControlsDB::readRfGainSetting() const
{
    std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
    int gain_reduction_max = gain_reduction_row[gain_reduction_row.size() - 1];
    return gain_reduction_max - gain_reduction_row[chParams->tunerParams.gain.LNAstate];
}

void GainControlsDB::writeRfGainSetting(int value)
{
    std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
    int gain_reduction_max = gain_reduction_row[gain_reduction_row.size() - 1];
    double rfgRdB = gain_reduction_max - value;
    int LNAstate = gain_reduction_table->gRdBToLNAstate(rfgRdB, gain_reduction_row, true);
    if (LNAstate == -1)
    {
        SoapySDR_logf(SOAPY_SDR_ERROR, "error in writeRfGainSetting() - gain=%d is invalid", value);
        return;
    }
    chParams->tunerParams.gain.LNAstate = static_cast<unsigned char>(LNAstate);
    return;
}


/* Gain mode with only RF attenuation in dB
 * IF attenuation is always controlled by AGC
 *   - RFATT: RF gain reduction in dB defined as: RFGRdB (function of LNA state)
 *            higher values mean less gain - range: varies
 */

std::vector<std::string> GainControlsRFATT::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("RFATT");

    return results;
}

bool GainControlsRFATT::hasGainMode(const int direction, const size_t channel) const
{
    // return false since IF AGC is always on
    return false;
}

bool GainControlsRFATT::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    // this method should never be called
    return false;
}

bool GainControlsRFATT::getGainMode(const int direction, const size_t channel) const
{
    // this method should never be called
    return false;
}

bool GainControlsRFATT::setGain(const int direction, const size_t channel, const double value)
{
    return setGain(direction, channel, "RFATT", value);
}

bool GainControlsRFATT::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    // do not change the gain if it is out of range
    SoapySDR::Range range = getGainRange(direction, channel, name);
    if (value < range.minimum() || value > range.maximum()) {
        SoapySDR_logf(SOAPY_SDR_ERROR, "error in setGain(%s) - gain=%lf is out of range=[%lf,%lf]", name.c_str(), value, range.minimum(), range.maximum());
        return false;
    }

    bool doUpdate = false;

    // always enable IF AGC
    if (chParams->ctrlParams.agc.enable != sdrplay_api_AGC_50HZ)
    {
        chParams->ctrlParams.agc.enable = sdrplay_api_AGC_50HZ;
    }
    if (name == "RFATT")
    {
        std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
        int LNAstate = gain_reduction_table->gRdBToLNAstate(value, gain_reduction_row);
        if (chParams->tunerParams.gain.LNAstate != LNAstate) {
             chParams->tunerParams.gain.LNAstate = LNAstate;
             doUpdate = true;
        }
    }
    return doUpdate;
}

double GainControlsRFATT::getGain(const int direction, const size_t channel) const
{
    return getGain(direction, channel, "RFATT");
}

double GainControlsRFATT::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RFATT")
    {
        std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
        return gain_reduction_row[chParams->tunerParams.gain.LNAstate];
    }
    return 0;
}

SoapySDR::Range GainControlsRFATT::getGainRange(const int direction, const size_t channel) const
{
    return getGainRange(direction, channel, "RFATT");
}

SoapySDR::Range GainControlsRFATT::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "RFATT")
    {
        std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
        int gain_reduction_max = gain_reduction_row[gain_reduction_row.size() - 1];
        return SoapySDR::Range(gain_reduction_row[0], gain_reduction_max);
    }
    return SoapySDR::Range(0, 0);
}

std::string GainControlsRFATT::getRfGainSettingName() const
{
    return "RF Attenuation (dB)";
}

std::vector<int> GainControlsRFATT::getRfGainSettingOptions(int &defaultValue) const
{
    std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
    defaultValue = gain_reduction_row[gain_reduction_row.size() / 2];
    return gain_reduction_row;
}

int GainControlsRFATT::readRfGainSetting() const
{
    std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
    return gain_reduction_row[chParams->tunerParams.gain.LNAstate];
}

void GainControlsRFATT::writeRfGainSetting(int value)
{
    std::vector<int> gain_reduction_row = gain_reduction_table->getGainReductionRow();
    int LNAstate = gain_reduction_table->gRdBToLNAstate(value, gain_reduction_row, true);
    if (LNAstate == -1)
    {
        SoapySDR_logf(SOAPY_SDR_ERROR, "error in writeRfGainSetting() - gain=%d is invalid", value);
        return;
    }
    chParams->tunerParams.gain.LNAstate = static_cast<unsigned char>(LNAstate);
    return;
}


/* "Steps" gain mode: gain steps as in SDRplay RSPTCPServer program
 * see: https://github.com/SDRplay/RSPTCPServer/blob/master/rsp_tcp.c#L154-L225
 *   - STEP: combination of RF gain reduction (LNA state) and IF gain reduction
 *                   higher values mean more gain - range: 1-29
 */

GainControlsSteps::GainControlsSteps(sdrplay_api_DeviceT &device,
                                     sdrplay_api_RxChannelParamsT *chParams) :
        GainControls(device, chParams)
{
    step = GAIN_STEPS / 2 + 1;
    std::vector<uint8_t> lnastates;
    std::vector<uint8_t> if_gains;
    getGainSteps(lnastates, if_gains);
    chParams->tunerParams.gain.LNAstate = lnastates[step - 1];
    chParams->tunerParams.gain.gRdB = if_gains[step - 1];
    chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
}

std::vector<std::string> GainControlsSteps::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("STEP");

    return results;
}

bool GainControlsSteps::hasGainMode(const int direction, const size_t channel) const
{
    // AGC is turned off since IF gain reduction is set by the step value
    return false;
}

bool GainControlsSteps::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    // this method should never be called
    return false;
}

bool GainControlsSteps::getGainMode(const int direction, const size_t channel) const
{
    return false;
}

bool GainControlsSteps::setGain(const int direction, const size_t channel, const double value)
{
    return setGain(direction, channel, "STEP", value);
}

bool GainControlsSteps::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    // do not change the gain if it is out of range
    SoapySDR::Range range = getGainRange(direction, channel, name);
    if (value < range.minimum() || value > range.maximum()) {
        SoapySDR_logf(SOAPY_SDR_ERROR, "error in setGain(%s) - gain=%lf is out of range=[%lf,%lf]", name.c_str(), value, range.minimum(), range.maximum());
        return false;
    }

    bool doUpdate = false;

    // always disable IF AGC
    if (chParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE)
    {
        chParams->ctrlParams.agc.enable = sdrplay_api_AGC_DISABLE;
    }
    if (name == "STEP")
    {
        step = (int)value;
        std::vector<uint8_t> lnastates;
        std::vector<uint8_t> if_gains;
        getGainSteps(lnastates, if_gains);
        if (chParams->tunerParams.gain.LNAstate != lnastates[step - 1]) {
            chParams->tunerParams.gain.LNAstate = lnastates[step - 1];
            doUpdate = true;
        }
        if (chParams->tunerParams.gain.gRdB != if_gains[step - 1]) {
            chParams->tunerParams.gain.gRdB = if_gains[step - 1];
            doUpdate = true;
        }
    }
    return doUpdate;
}

double GainControlsSteps::getGain(const int direction, const size_t channel) const
{
    return getGain(direction, channel, "STEP");
}

double GainControlsSteps::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "STEP")
    {
        return step;
    }
    return 0;
}

SoapySDR::Range GainControlsSteps::getGainRange(const int direction, const size_t channel) const
{
    return getGainRange(direction, channel, "STEP");
}

SoapySDR::Range GainControlsSteps::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "STEP")
    {
        return SoapySDR::Range(1, GAIN_STEPS);
    }
    return SoapySDR::Range(0, 0);
}

std::string GainControlsSteps::getRfGainSettingName() const
{
    return "Step";
}

std::vector<int> GainControlsSteps::getRfGainSettingOptions(int &defaultValue) const
{
    std::vector<int> options(GAIN_STEPS);
    std::iota(options.begin(), options.end(), 1);
    defaultValue = options[GAIN_STEPS / 2];
    return options;
}

int GainControlsSteps::readRfGainSetting() const
{
    return step;
}

void GainControlsSteps::writeRfGainSetting(int value)
{
    step = value;
    std::vector<uint8_t> lnastates;
    std::vector<uint8_t> if_gains;
    getGainSteps(lnastates, if_gains);
    chParams->tunerParams.gain.LNAstate = lnastates[step - 1];
    chParams->tunerParams.gain.gRdB = if_gains[step - 1];
    return;
}

void GainControlsSteps::getGainSteps(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains)
{
    if (device.hwVer == SDRPLAY_RSP1_ID) {
        getGainStepsRSP1(lnastates, if_gains);
    } else if (device.hwVer == SDRPLAY_RSP1A_ID) {
        getGainStepsRSP1A(lnastates, if_gains);
    } else if (device.hwVer == SDRPLAY_RSP2_ID) {
        getGainStepsRSP2(lnastates, if_gains);
    } else if (device.hwVer == SDRPLAY_RSPduo_ID) {
        getGainStepsRSPduo(lnastates, if_gains);
    } else if (device.hwVer == SDRPLAY_RSPdx_ID) {
        getGainStepsRSPdx(lnastates, if_gains);
    } else {
        SoapySDR_logf(SOAPY_SDR_ERROR, "unknown RSP model %d. Aborting.", device.hwVer);
        throw std::runtime_error("unknown RSP model. Aborting.");
    }
}

void GainControlsSteps::getGainStepsRSP1(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains)
{
    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    if (rfHz < 60e6) {            // AM band
        lnastates = std::vector<uint8_t>{  3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,56,53,50,47,44,41,58,55,52,49,46,43,45,42,58,55,52,49,46,43,41,38,35,32,29,26,23,20 };
    } else if (rfHz < 120e6) {    // VHF band
        lnastates = std::vector<uint8_t>{  3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,56,53,50,47,44,41,58,55,52,49,46,43,45,42,58,55,52,49,46,43,41,38,35,32,29,26,23,20 };
    } else if (rfHz < 250e6) {    // band 3
        lnastates = std::vector<uint8_t>{  3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,56,53,50,47,44,41,58,55,52,49,46,43,45,42,58,55,52,49,46,43,41,38,35,32,29,26,23,20 };
    } else if (rfHz < 420e6) {    // band X
        lnastates = std::vector<uint8_t>{  3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,56,53,50,47,44,41,58,55,52,49,46,43,45,42,58,55,52,49,46,43,41,38,35,32,29,26,23,20 };
    } else if (rfHz < 1000e6) {   // band 4-5
        lnastates = std::vector<uint8_t>{  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,57,54,52,50,47,45,43,40,38,36,33,31,29,27,24,22,27,24,22,32,29,27,25,22,27,25,22,20 };
    } else if (rfHz <= 2000e6) {  // band L
        lnastates = std::vector<uint8_t>{  3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,57,55,52,50,48,46,43,41,44,42,53,51,49,47,44,42,45,43,40,38,36,34,31,29,27,25,22,20 };
    }
    return;
}

void GainControlsSteps::getGainStepsRSP1A(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains)
{
    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    if (rfHz < 60e6) {            // AM band
        lnastates = std::vector<uint8_t>{  6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 3, 3, 3, 3, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,57,53,49,46,42,44,40,56,52,48,45,41,44,40,43,45,41,38,34,31,27,24,20 };
    } else if (rfHz < 120e6) {    // VHF band
        lnastates = std::vector<uint8_t>{  9, 9, 9, 9, 9, 9, 8, 7, 7, 7, 7, 7, 6, 6, 5, 5, 4, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,42,58,54,51,47,43,46,42,44,41,43,42,44,40,43,45,42,38,34,31,27,24,20 };
    } else if (rfHz < 250e6) {    // band 3
        lnastates = std::vector<uint8_t>{  9, 9, 9, 9, 9, 9, 8, 7, 7, 7, 7, 7, 6, 6, 5, 5, 4, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,42,58,54,51,47,43,46,42,44,41,43,42,44,40,43,45,42,38,34,31,27,24,20 };
    } else if (rfHz < 420e6) {    // band X
        lnastates = std::vector<uint8_t>{  9, 9, 9, 9, 9, 9, 8, 7, 7, 7, 7, 7, 6, 6, 5, 5, 4, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,42,58,54,51,47,43,46,42,44,41,43,42,44,40,43,45,42,38,34,31,27,24,20 };
    } else if (rfHz < 1000e6) {   // band 4-5
        lnastates = std::vector<uint8_t>{  9, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 7, 6, 6, 5, 5, 4, 4, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,44,41,56,52,49,45,41,44,46,42,45,41,44,40,44,40,42,46,42,38,35,31,27,24,20 };
    } else if (rfHz <= 2000e6) {  // band L
        lnastates = std::vector<uint8_t>{  8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 5, 5, 4, 4, 3, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,56,53,49,46,42,43,46,42,44,41,43,48,44,40,43,45,42,38,34,31,27,24,20 };
    }
    return;
}

void GainControlsSteps::getGainStepsRSP2(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains)
{
    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    sdrplay_api_Rsp2_AmPortSelectT amPortSel = chParams->rsp2TunerParams.amPortSel;
    if (rfHz < 60e6 && amPortSel == sdrplay_api_Rsp2_AMPORT_1) {   // Hi-Z port
        lnastates = std::vector<uint8_t>{  4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,56,54,51,48,45,43,40,56,54,51,48,45,43,40,43,41,44,41,44,42,39,36,34,31,28,25,23,20 };
    } else if (rfHz < 60e6) {     // AM band
        lnastates = std::vector<uint8_t>{  8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 5, 5, 4, 4, 4, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,44,41,56,52,49,45,41,44,45,41,48,44,40,45,42,43,49,46,42,38,35,31,27,24,20 };
    } else if (rfHz < 120e6) {    // VHF band
        lnastates = std::vector<uint8_t>{  8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 5, 5, 4, 4, 4, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,44,41,56,52,49,45,41,44,45,41,48,44,40,45,42,43,49,46,42,38,35,31,27,24,20 };
    } else if (rfHz < 250e6) {    // band 3
        lnastates = std::vector<uint8_t>{  8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 5, 5, 4, 4, 4, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,44,41,56,52,49,45,41,44,45,41,48,44,40,45,42,43,49,46,42,38,35,31,27,24,20 };
    } else if (rfHz < 420e6) {    // band X
        lnastates = std::vector<uint8_t>{  8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 5, 5, 4, 4, 4, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,44,41,56,52,49,45,41,44,45,41,48,44,40,45,42,43,49,46,42,38,35,31,27,24,20 };
    } else if (rfHz < 1000e6) {   // band 4-5
        lnastates = std::vector<uint8_t>{  5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 3, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,56,53,50,48,45,42,58,55,52,49,47,44,41,43,40,44,41,42,46,43,40,37,34,31,29,26,23,20 };
    } else if (rfHz <= 2000e6) {  // band L
        lnastates = std::vector<uint8_t>{  4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,56,54,51,48,45,43,40,56,54,51,48,45,43,40,43,41,44,41,44,42,39,36,34,31,28,25,23,20 };
    }
    return;
}

void GainControlsSteps::getGainStepsRSPduo(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains)
{
    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    sdrplay_api_RspDuo_AmPortSelectT tuner1AmPortSel = chParams->rspDuoTunerParams.tuner1AmPortSel;
    if (rfHz < 60e6 && tuner1AmPortSel == sdrplay_api_RspDuo_AMPORT_1) {   // Hi-Z port
        lnastates = std::vector<uint8_t>{  4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,56,54,51,48,45,43,40,56,54,51,48,45,43,40,43,41,44,41,44,42,39,36,34,31,28,25,23,20 };
    } else if (rfHz < 60e6) {     // AM band
        lnastates = std::vector<uint8_t>{  6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 3, 3, 3, 3, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,57,53,49,46,42,44,40,56,52,48,45,41,44,40,43,45,41,38,34,31,27,24,20 };
    } else if (rfHz < 120e6) {    // VHF band
        lnastates = std::vector<uint8_t>{  9, 9, 9, 9, 9, 9, 8, 7, 7, 7, 7, 7, 6, 6, 5, 5, 4, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,42,58,54,51,47,43,46,42,44,41,43,42,44,40,43,45,42,38,34,31,27,24,20 };
    } else if (rfHz < 250e6) {    // band 3
        lnastates = std::vector<uint8_t>{  9, 9, 9, 9, 9, 9, 8, 7, 7, 7, 7, 7, 6, 6, 5, 5, 4, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,42,58,54,51,47,43,46,42,44,41,43,42,44,40,43,45,42,38,34,31,27,24,20 };
    } else if (rfHz < 420e6) {    // band X
        lnastates = std::vector<uint8_t>{  9, 9, 9, 9, 9, 9, 8, 7, 7, 7, 7, 7, 6, 6, 5, 5, 4, 3, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,42,58,54,51,47,43,46,42,44,41,43,42,44,40,43,45,42,38,34,31,27,24,20 };
    } else if (rfHz < 1000e6) {   // band 4-5
        lnastates = std::vector<uint8_t>{  9, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 7, 6, 6, 5, 5, 4, 4, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,44,41,56,52,49,45,41,44,46,42,45,41,44,40,44,40,42,46,42,38,35,31,27,24,20 };
    } else if (rfHz <= 2000e6) {  // band L
        lnastates = std::vector<uint8_t>{  8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 5, 5, 4, 4, 3, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,56,53,49,46,42,43,46,42,44,41,43,48,44,40,43,45,42,38,34,31,27,24,20 };
    }
    return;
}

void GainControlsSteps::getGainStepsRSPdx(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains)
{
    double rfHz = chParams->tunerParams.rfFreq.rfHz;
    if (rfHz < 60e6) {            // AM band
        lnastates = std::vector<uint8_t>{ 18,18,18,18,18,18,17,16,14,13,12,11,10, 9, 7, 6, 5, 5, 5, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,45,41,41,40,43,42,42,41,41,40,42,42,47,44,40,43,42,42,41,38,34,31,27,24,20 };
    } else if (rfHz < 120e6) {    // VHF band
        lnastates = std::vector<uint8_t>{ 26,26,26,26,26,25,23,22,20,19,17,16,14,13,11,10, 8, 7, 5, 5, 5, 3, 2, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,50,46,41,40,42,40,42,40,42,41,42,41,43,41,43,41,49,45,40,42,40,42,38,33,29,24,20 };
    } else if (rfHz < 250e6) {    // band 3
        lnastates = std::vector<uint8_t>{ 26,26,26,26,26,25,23,22,20,19,17,16,14,13,11,10, 8, 7, 5, 5, 5, 3, 2, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,50,46,41,40,42,40,42,40,42,41,42,41,43,41,43,41,49,45,40,42,40,42,38,33,29,24,20 };
    } else if (rfHz < 420e6) {    // band X
        lnastates = std::vector<uint8_t>{ 27,27,27,27,27,26,24,23,21,20,18,17,15,14,12,11, 9, 8, 6, 6, 5, 3, 2, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,50,46,41,40,42,40,42,40,42,41,42,41,43,41,43,41,46,42,40,42,40,42,38,33,29,24,20 };
    } else if (rfHz < 1000e6) {   // band 4-5
        lnastates = std::vector<uint8_t>{ 20,20,20,20,20,20,18,17,16,14,13,12,11, 9, 8, 7, 7, 5, 4, 3, 2, 0, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,51,48,44,40,42,42,41,43,42,41,41,43,42,44,40,43,42,41,40,46,43,39,35,31,28,24,20 };
    } else if (rfHz <= 2000e6) {  // band L
        lnastates = std::vector<uint8_t>{ 18,18,18,18,18,18,16,15,14,13,11,10, 9, 8, 7, 6, 6, 6, 5, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0 };
        if_gains = std::vector<uint8_t>{ 59,55,52,48,44,40,43,42,41,41,43,42,41,41,40,48,45,41,40,42,42,41,42,39,35,31,27,24,20 };
    }
    return;
}


/* Gain mode with only IF gain reduction in dB
 * RF gain reduction is controlled by the 'RF Gain Select' setting
 *   - IFGR: IF gain reduction in dB
 *                   higher values mean less gain - range: 20-59
 */

std::vector<std::string> GainControlsIFGR::listGains(const int direction, const size_t channel) const
{
    //list available gain elements,
    //the functions below have a "name" parameter
    std::vector<std::string> results;

    results.push_back("IFGR");

    return results;
}

bool GainControlsIFGR::hasGainMode(const int direction, const size_t channel) const
{
    return true;
}

bool GainControlsIFGR::setGainMode(const int direction, const size_t channel, const bool automatic)
{
    bool doUpdate = false;

    sdrplay_api_AgcControlT agc_control = automatic ? sdrplay_api_AGC_50HZ : sdrplay_api_AGC_DISABLE;
    if (chParams->ctrlParams.agc.enable != agc_control)
    {
        chParams->ctrlParams.agc.enable = agc_control;
        doUpdate = true;
    }
    return doUpdate;
}

bool GainControlsIFGR::getGainMode(const int direction, const size_t channel) const
{
    return chParams->ctrlParams.agc.enable != sdrplay_api_AGC_DISABLE;
}

bool GainControlsIFGR::setGain(const int direction, const size_t channel, const double value)
{
    return setGain(direction, channel, "IFGR", value);
}

bool GainControlsIFGR::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    bool doUpdate = false;

    if (name == "IFGR")
    {
        if (chParams->ctrlParams.agc.enable == sdrplay_api_AGC_DISABLE)
        {
            // apply the change if the required value is different from gRdB
            if (chParams->tunerParams.gain.gRdB != (int)value)
            {
                chParams->tunerParams.gain.gRdB = (int)value;
                doUpdate = true;
            }
        }
        else
        {
            SoapySDR_log(SOAPY_SDR_WARNING, "Not updating IFGR gain because AGC is enabled");
        }
    }
    return doUpdate;
}

double GainControlsIFGR::getGain(const int direction, const size_t channel) const
{
    return getGain(direction, channel, "IFGR");
}

double GainControlsIFGR::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "IFGR")
    {
        return chParams->tunerParams.gain.gRdB;
    }

    return 0;
}

SoapySDR::Range GainControlsIFGR::getGainRange(const int direction, const size_t channel) const
{
    return getGainRange(direction, channel, "IFGR");
}

SoapySDR::Range GainControlsIFGR::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    if (name == "IFGR")
    {
        return SoapySDR::Range(sdrplay_api_NORMAL_MIN_GR, MAX_BB_GR);
    }
    return SoapySDR::Range(0, 0);
}

std::string GainControlsIFGR::getRfGainSettingName() const
{
    return "RF Gain Select";
}

std::vector<int> GainControlsIFGR::getRfGainSettingOptions(int &defaultValue) const
{
    std::vector<int> options;
    switch(device.hwVer)
    {
        case SDRPLAY_RSP1_ID:
            options.resize(3+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[1];
            break;
        case SDRPLAY_RSP1A_ID:
            options.resize(9+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[4];
            break;
        case SDRPLAY_RSP2_ID:
            options.resize(8+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[4];
            break;
        case SDRPLAY_RSPduo_ID:
            options.resize(9+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[4];
            break;
        case SDRPLAY_RSPdx_ID:
            options.resize(27+1);
            std::iota(options.begin(), options.end(), 0);
            defaultValue = options[4];
            break;
        default:
            defaultValue = -1;
            break;
    }
    return options;
}

int GainControlsIFGR::readRfGainSetting() const
{
    return static_cast<int>(chParams->tunerParams.gain.LNAstate);
}

void GainControlsIFGR::writeRfGainSetting(int value)
{
    chParams->tunerParams.gain.LNAstate = static_cast<unsigned char>(value);
    return;
}
