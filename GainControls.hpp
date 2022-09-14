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
#pragma once

#include <numeric>
#include <string>
#include <vector>
#include <SoapySDR/Types.hpp>

#include <sdrplay_api.h>

class GainControls
{
public:
    GainControls(sdrplay_api_DeviceT &device,
                 sdrplay_api_RxChannelParamsT *chParams) :
        device(device), chParams(chParams) {}

    virtual std::vector<std::string> listGains(const int direction, const size_t channel) const = 0;

    virtual bool hasGainMode(const int direction, const size_t channel) const = 0;

    virtual bool setGainMode(const int direction, const size_t channel, const bool automatic) = 0;

    virtual bool getGainMode(const int direction, const size_t channel) const = 0;

    virtual bool hasGenericGain() const { return false; }

    virtual bool setGain(const int direction, const size_t channel, const double value);

    virtual bool setGain(const int direction, const size_t channel, const std::string &name, const double value) = 0;

    virtual double getGain(const int direction, const size_t channel) const;

    virtual double getGain(const int direction, const size_t channel, const std::string &name) const = 0;

    virtual SoapySDR::Range getGainRange(const int direction, const size_t channel) const;

    virtual SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const = 0;

    /* RfGainSetting methods */
    virtual std::string getRfGainSettingName() const = 0;

    virtual std::vector<int> getRfGainSettingOptions(int &defaultValue) const = 0;

    virtual int readRfGainSetting() const = 0;

    virtual void writeRfGainSetting(int value) = 0;

    virtual ~GainControls() = default;

protected:
    sdrplay_api_DeviceT &device;
    sdrplay_api_RxChannelParamsT *chParams;
};


// Legacy
class GainControlsLegacy: public GainControls
{
public:
    GainControlsLegacy(sdrplay_api_DeviceT &device,
                       sdrplay_api_RxChannelParamsT *chParams) :
        GainControls(device, chParams) {}

    std::vector<std::string> listGains(const int direction, const size_t channel) const override;

    bool hasGainMode(const int direction, const size_t channel) const override;

    bool setGainMode(const int direction, const size_t channel, const bool automatic) override;

    bool getGainMode(const int direction, const size_t channel) const override;

    bool setGain(const int direction, const size_t channel, const std::string &name, const double value) override;

    double getGain(const int direction, const size_t channel, const std::string &name) const override;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const override;

    /* RfGainSetting methods */
    std::string getRfGainSettingName() const override;

    std::vector<int> getRfGainSettingOptions(int &defaultValue) const override;

    int readRfGainSetting() const override;

    void writeRfGainSetting(int value) override;
};


class GainControlsWithGainReductionTable: public GainControls
{
public:
    class GainReductionTable;

    GainControlsWithGainReductionTable(sdrplay_api_DeviceT &device,
                                       sdrplay_api_DeviceParamsT *deviceParams,
                                       sdrplay_api_RxChannelParamsT *chParams);

protected:
    GainReductionTable *gain_reduction_table;

private:
    sdrplay_api_DeviceParamsT *deviceParams;


public:
    class GainReductionTable
    {
    public:
        GainReductionTable(GainControlsWithGainReductionTable *gain_controls) :
            gain_controls(gain_controls) {}

        virtual std::vector<int> getGainReductionRow(void) = 0;

        int gRdBToLNAstate(double rfgRdB, std::vector<int> &gain_reduction_row, bool exact=false);

        virtual ~GainReductionTable() = default;

    protected:
        GainControlsWithGainReductionTable *gain_controls;
    };

    class GainReductionTableRSP1: public GainReductionTable
    {
    public:
        GainReductionTableRSP1(GainControlsWithGainReductionTable *gain_controls) :
            GainReductionTable(gain_controls) {}

        std::vector<int> getGainReductionRow(void) override;
    };

    class GainReductionTableRSP1A: public GainReductionTable
    {
    public:
        GainReductionTableRSP1A(GainControlsWithGainReductionTable *gain_controls) :
            GainReductionTable(gain_controls) {}

        std::vector<int> getGainReductionRow(void) override;
    };

    class GainReductionTableRSP2: public GainReductionTable
    {
    public:
        GainReductionTableRSP2(GainControlsWithGainReductionTable *gain_controls) :
            GainReductionTable(gain_controls) {}

        std::vector<int> getGainReductionRow(void) override;
    };

    class GainReductionTableRSPduo: public GainReductionTable
    {
    public:
        GainReductionTableRSPduo(GainControlsWithGainReductionTable *gain_controls) :
            GainReductionTable(gain_controls) {}

        std::vector<int> getGainReductionRow(void) override;
    };

    class GainReductionTableRSPdx: public GainReductionTable
    {
    public:
        GainReductionTableRSPdx(GainControlsWithGainReductionTable *gain_controls) :
            GainReductionTable(gain_controls) {}

        std::vector<int> getGainReductionRow(void) override;
    };
};


// DB
class GainControlsDB: public GainControlsWithGainReductionTable
{
public:
    GainControlsDB(sdrplay_api_DeviceT &device,
                   sdrplay_api_DeviceParamsT *deviceParams,
                   sdrplay_api_RxChannelParamsT *chParams) :
        GainControlsWithGainReductionTable(device, deviceParams, chParams) {}

    std::vector<std::string> listGains(const int direction, const size_t channel) const override;

    bool hasGainMode(const int direction, const size_t channel) const override;

    bool setGainMode(const int direction, const size_t channel, const bool automatic) override;

    bool getGainMode(const int direction, const size_t channel) const override;

    bool hasGenericGain() const override { return true; }

    bool setGain(const int direction, const size_t channel, const double value) override;

    bool setGain(const int direction, const size_t channel, const std::string &name, const double value) override;

    double getGain(const int direction, const size_t channel) const override;

    double getGain(const int direction, const size_t channel, const std::string &name) const override;

    SoapySDR::Range getGainRange(const int direction, const size_t channel) const override;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const override;

    /* RfGainSetting methods */
    std::string getRfGainSettingName() const override;

    std::vector<int> getRfGainSettingOptions(int &defaultValue) const override;

    int readRfGainSetting() const override;

    void writeRfGainSetting(int value) override;
};


// RFATT
class GainControlsRFATT: public GainControlsWithGainReductionTable
{
public:
    GainControlsRFATT(sdrplay_api_DeviceT &device,
                      sdrplay_api_DeviceParamsT *deviceParams,
                      sdrplay_api_RxChannelParamsT *chParams) :
        GainControlsWithGainReductionTable(device, deviceParams, chParams) {}

    std::vector<std::string> listGains(const int direction, const size_t channel) const override;

    bool hasGainMode(const int direction, const size_t channel) const override;

    bool setGainMode(const int direction, const size_t channel, const bool automatic) override;

    bool getGainMode(const int direction, const size_t channel) const override;

    bool hasGenericGain() const override { return true; }

    bool setGain(const int direction, const size_t channel, const double value) override;

    bool setGain(const int direction, const size_t channel, const std::string &name, const double value) override;

    double getGain(const int direction, const size_t channel) const override;

    double getGain(const int direction, const size_t channel, const std::string &name) const override;

    SoapySDR::Range getGainRange(const int direction, const size_t channel) const override;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const override;

    /* RfGainSetting methods */
    std::string getRfGainSettingName() const override;

    std::vector<int> getRfGainSettingOptions(int &defaultValue) const override;

    int readRfGainSetting() const override;

    void writeRfGainSetting(int value) override;
};


// Steps
class GainControlsSteps: public GainControls
{
public:
    GainControlsSteps(sdrplay_api_DeviceT &device,
                      sdrplay_api_RxChannelParamsT *chParams);

    std::vector<std::string> listGains(const int direction, const size_t channel) const override;

    bool hasGainMode(const int direction, const size_t channel) const override;

    bool setGainMode(const int direction, const size_t channel, const bool automatic) override;

    bool getGainMode(const int direction, const size_t channel) const override;

    bool hasGenericGain() const override { return true; }

    bool setGain(const int direction, const size_t channel, const double value) override;

    bool setGain(const int direction, const size_t channel, const std::string &name, const double value) override;

    double getGain(const int direction, const size_t channel) const override;

    double getGain(const int direction, const size_t channel, const std::string &name) const override;

    SoapySDR::Range getGainRange(const int direction, const size_t channel) const override;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const override;

    /* RfGainSetting methods */
    std::string getRfGainSettingName() const override;

    std::vector<int> getRfGainSettingOptions(int &defaultValue) const override;

    int readRfGainSetting() const override;

    void writeRfGainSetting(int value) override;

private:
    void getGainSteps(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains);
    void getGainStepsRSP1(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains);
    void getGainStepsRSP1A(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains);
    void getGainStepsRSP2(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains);
    void getGainStepsRSPduo(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains);
    void getGainStepsRSPdx(std::vector<uint8_t> &lnastates, std::vector<uint8_t> &if_gains);

private:
    static constexpr int GAIN_STEPS = 29;
    int step;
};
