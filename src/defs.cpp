// SPDX-License-Identifier: MPL-2.0
#include "aare/defs.hpp"
#include <stdexcept>
#include <string>

#include <fmt/core.h>
namespace aare {

void assert_failed(const std::string &msg) {
    fmt::print(msg);
    exit(1);
}

/**
 * @brief Convert a DetectorType to a string
 * @param type DetectorType
 * @return string representation of the DetectorType
 */
template <> std::string ToString(DetectorType arg) {
    switch (arg) {
    case DetectorType::Generic:
        return "Generic";
    case DetectorType::Eiger:
        return "Eiger";
    case DetectorType::Gotthard:
        return "Gotthard";
    case DetectorType::Jungfrau:
        return "Jungfrau";
    case DetectorType::ChipTestBoard:
        return "ChipTestBoard";
    case DetectorType::Moench:
        return "Moench";
    case DetectorType::Mythen3:
        return "Mythen3";
    case DetectorType::Gotthard2:
        return "Gotthard2";
    case DetectorType::Xilinx_ChipTestBoard:
        return "Xilinx_ChipTestBoard";

    // Custom ones
    case DetectorType::Moench03:
        return "Moench03";
    case DetectorType::Moench03_old:
        return "Moench03_old";
    case DetectorType::Unknown:
        return "Unknown";

        // no default case to trigger compiler warning if not all
        // enum values are handled
    }
    throw std::runtime_error("Could not decode detector to string");
}

/**
 * @brief Convert a string to a DetectorType
 * @param name string representation of the DetectorType
 * @return DetectorType
 * @throw runtime_error if the string does not match any DetectorType
 */
template <> DetectorType StringTo(const std::string &arg) {
    if (arg == "Generic")
        return DetectorType::Generic;
    if (arg == "Eiger")
        return DetectorType::Eiger;
    if (arg == "Gotthard")
        return DetectorType::Gotthard;
    if (arg == "Jungfrau")
        return DetectorType::Jungfrau;
    if (arg == "ChipTestBoard")
        return DetectorType::ChipTestBoard;
    if (arg == "Moench")
        return DetectorType::Moench;
    if (arg == "Mythen3")
        return DetectorType::Mythen3;
    if (arg == "Gotthard2")
        return DetectorType::Gotthard2;
    if (arg == "Xilinx_ChipTestBoard")
        return DetectorType::Xilinx_ChipTestBoard;

    // Custom ones
    if (arg == "Moench03")
        return DetectorType::Moench03;
    if (arg == "Moench03_old")
        return DetectorType::Moench03_old;
    if (arg == "Unknown")
        return DetectorType::Unknown;

    throw std::runtime_error("Could not decode detector from: \"" + arg + "\"");
}

/**
 * @brief Convert a string to a TimingMode
 * @param mode string representation of the TimingMode
 * @return TimingMode
 * @throw runtime_error if the string does not match any TimingMode
 */
template <> TimingMode StringTo(const std::string &arg) {
    if (arg == "auto")
        return TimingMode::Auto;
    if (arg == "trigger")
        return TimingMode::Trigger;
    throw std::runtime_error("Could not decode timing mode from: \"" + arg +
                             "\"");
}

template <> FrameDiscardPolicy StringTo(const std::string &arg) {
    if (arg == "nodiscard")
        return FrameDiscardPolicy::NoDiscard;
    if (arg == "discard")
        return FrameDiscardPolicy::Discard;
    if (arg == "discardpartial")
        return FrameDiscardPolicy::DiscardPartial;
    throw std::runtime_error("Could not decode frame discard policy from: \"" +
                             arg + "\"");
}

// template <> TimingMode StringTo<TimingMode>(std::string mode);

template <> DACIndex StringTo(const std::string &arg) {
    if (arg == "dac 0")
        return DACIndex::DAC_0;
    else if (arg == "dac 1")
        return DACIndex::DAC_1;
    else if (arg == "dac 2")
        return DACIndex::DAC_2;
    else if (arg == "dac 3")
        return DACIndex::DAC_3;
    else if (arg == "dac 4")
        return DACIndex::DAC_4;
    else if (arg == "dac 5")
        return DACIndex::DAC_5;
    else if (arg == "dac 6")
        return DACIndex::DAC_6;
    else if (arg == "dac 7")
        return DACIndex::DAC_7;
    else if (arg == "dac 8")
        return DACIndex::DAC_8;
    else if (arg == "dac 9")
        return DACIndex::DAC_9;
    else if (arg == "dac 10")
        return DACIndex::DAC_10;
    else if (arg == "dac 11")
        return DACIndex::DAC_11;
    else if (arg == "dac 12")
        return DACIndex::DAC_12;
    else if (arg == "dac 13")
        return DACIndex::DAC_13;
    else if (arg == "dac 14")
        return DACIndex::DAC_14;
    else if (arg == "dac 15")
        return DACIndex::DAC_15;
    else if (arg == "dac 16")
        return DACIndex::DAC_16;
    else if (arg == "dac 17")
        return DACIndex::DAC_17;
    else if (arg == "vsvp")
        return DACIndex::VSVP;
    else if (arg == "vtrim")
        return DACIndex::VTRIM;
    else if (arg == "vrpreamp")
        return DACIndex::VRPREAMP;
    else if (arg == "vrshaper")
        return DACIndex::VRSHAPER;
    else if (arg == "vsvn")
        return DACIndex::VSVN;
    else if (arg == "vtgstv")
        return DACIndex::VTGSTV;
    else if (arg == "vcmp_ll")
        return DACIndex::VCMP_LL;
    else if (arg == "vcmp_lr")
        return DACIndex::VCMP_LR;
    else if (arg == "vcal")
        return DACIndex::VCAL;
    else if (arg == "vcmp_rl")
        return DACIndex::VCMP_RL;
    else if (arg == "rxb_rb")
        return DACIndex::RXB_RB;
    else if (arg == "rxb_lb")
        return DACIndex::RXB_LB;
    else if (arg == "vcmp_rr")
        return DACIndex::VCMP_RR;
    else if (arg == "vcp")
        return DACIndex::VCP;
    else if (arg == "vcn")
        return DACIndex::VCN;
    else if (arg == "vishaper")
        return DACIndex::VISHAPER;
    else if (arg == "vthreshold")
        return DACIndex::VTHRESHOLD;
    else if (arg == "vref_ds")
        return DACIndex::VREF_DS;
    else if (arg == "vout_cm")
        return DACIndex::VOUT_CM;
    else if (arg == "vin_cm")
        return DACIndex::VIN_CM;
    else if (arg == "vref_comp")
        return DACIndex::VREF_COMP;
    else if (arg == "vb_comp")
        return DACIndex::VB_COMP;
    else if (arg == "vdd_prot")
        return DACIndex::VDD_PROT;
    else if (arg == "vin_com")
        return DACIndex::VIN_COM;
    else if (arg == "vref_prech")
        return DACIndex::VREF_PRECH;
    else if (arg == "vb_pixbuf")
        return DACIndex::VB_PIXBUF;
    else if (arg == "vb_ds")
        return DACIndex::VB_DS;
    else if (arg == "vref_h_adc")
        return DACIndex::VREF_H_ADC;
    else if (arg == "vb_comp_fe")
        return DACIndex::VB_COMP_FE;
    else if (arg == "vb_comp_adc")
        return DACIndex::VB_COMP_ADC;
    else if (arg == "vcom_cds")
        return DACIndex::VCOM_CDS;
    else if (arg == "vref_rstore")
        return DACIndex::VREF_RSTORE;
    else if (arg == "vb_opa_1st")
        return DACIndex::VB_OPA_1ST;
    else if (arg == "vref_comp_fe")
        return DACIndex::VREF_COMP_FE;
    else if (arg == "vcom_adc1")
        return DACIndex::VCOM_ADC1;
    else if (arg == "vref_l_adc")
        return DACIndex::VREF_L_ADC;
    else if (arg == "vref_cds")
        return DACIndex::VREF_CDS;
    else if (arg == "vb_cs")
        return DACIndex::VB_CS;
    else if (arg == "vb_opa_fd")
        return DACIndex::VB_OPA_FD;
    else if (arg == "vcom_adc2")
        return DACIndex::VCOM_ADC2;
    else if (arg == "vcassh")
        return DACIndex::VCASSH;
    else if (arg == "vth2")
        return DACIndex::VTH2;
    else if (arg == "vrshaper_n")
        return DACIndex::VRSHAPER_N;
    else if (arg == "vipre_out")
        return DACIndex::VIPRE_OUT;
    else if (arg == "vth3")
        return DACIndex::VTH3;
    else if (arg == "vth1")
        return DACIndex::VTH1;
    else if (arg == "vicin")
        return DACIndex::VICIN;
    else if (arg == "vcas")
        return DACIndex::VCAS;
    else if (arg == "vcal_n")
        return DACIndex::VCAL_N;
    else if (arg == "vipre")
        return DACIndex::VIPRE;
    else if (arg == "vcal_p")
        return DACIndex::VCAL_P;
    else if (arg == "vdcsh")
        return DACIndex::VDCSH;
    else if (arg == "vbp_colbuf")
        return DACIndex::VBP_COLBUF;
    else if (arg == "vb_sda")
        return DACIndex::VB_SDA;
    else if (arg == "vcasc_sfp")
        return DACIndex::VCASC_SFP;
    else if (arg == "vipre_cds")
        return DACIndex::VIPRE_CDS;
    else if (arg == "ibias_sfp")
        return DACIndex::IBIAS_SFP;
    else if (arg == "trimbits")
        return DACIndex::TRIMBIT_SCAN;
    else if (arg == "highvoltage")
        return DACIndex::HIGH_VOLTAGE;
    else if (arg == "iodelay")
        return DACIndex::IO_DELAY;
    else if (arg == "temp_adc")
        return DACIndex::TEMPERATURE_ADC;
    else if (arg == "temp_fpga")
        return DACIndex::TEMPERATURE_FPGA;
    else if (arg == "temp_fpgaext")
        return DACIndex::TEMPERATURE_FPGAEXT;
    else if (arg == "temp_10ge")
        return DACIndex::TEMPERATURE_10GE;
    else if (arg == "temp_dcdc")
        return DACIndex::TEMPERATURE_DCDC;
    else if (arg == "temp_sodl")
        return DACIndex::TEMPERATURE_SODL;
    else if (arg == "temp_sodr")
        return DACIndex::TEMPERATURE_SODR;
    else if (arg == "temp_fpgafl")
        return DACIndex::TEMPERATURE_FPGA2;
    else if (arg == "temp_fpgafr")
        return DACIndex::TEMPERATURE_FPGA3;
    else if (arg == "temp_slowadc")
        return DACIndex::SLOW_ADC_TEMP;
    else
        throw std::invalid_argument("Could not decode DACIndex from: \"" + arg +
                                    "\"");
}

BitOffset::BitOffset(uint32_t offset){
    if (offset>7)
        throw std::runtime_error(fmt::format("{} expand24to32bit called with bit_offset: {}, bit offset must be <8", LOCATION, offset));

    m_offset = static_cast<uint8_t>(offset);

}

bool BitOffset::operator==(const BitOffset& other) const {
        return m_offset == other.m_offset;
    }

bool BitOffset::operator<(const BitOffset& other) const {
        return m_offset < other.m_offset;
    }



} // namespace aare