// SPDX-License-Identifier: MPL-2.0
#include "aare/defs.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>

#include "to_string.hpp"
using aare::string_to;

TEST_CASE("DetectorType string to enum") {
    REQUIRE(string_to<aare::DetectorType>("Generic") ==
            aare::DetectorType::Generic);
    REQUIRE(string_to<aare::DetectorType>("Eiger") == aare::DetectorType::Eiger);
    REQUIRE(string_to<aare::DetectorType>("Gotthard") ==
            aare::DetectorType::Gotthard);
    REQUIRE(string_to<aare::DetectorType>("Jungfrau") ==
            aare::DetectorType::Jungfrau);
    REQUIRE(string_to<aare::DetectorType>("ChipTestBoard") ==
            aare::DetectorType::ChipTestBoard);
    REQUIRE(string_to<aare::DetectorType>("Moench") ==
            aare::DetectorType::Moench);
    REQUIRE(string_to<aare::DetectorType>("Mythen3") ==
            aare::DetectorType::Mythen3);
    REQUIRE(string_to<aare::DetectorType>("Gotthard2") ==
            aare::DetectorType::Gotthard2);
    REQUIRE(string_to<aare::DetectorType>("Xilinx_ChipTestBoard") ==
            aare::DetectorType::Xilinx_ChipTestBoard);
    REQUIRE(string_to<aare::DetectorType>("Moench03") ==
            aare::DetectorType::Moench03);
    REQUIRE(string_to<aare::DetectorType>("Moench03_old") ==
            aare::DetectorType::Moench03_old);
    REQUIRE(string_to<aare::DetectorType>("Unknown") ==
            aare::DetectorType::Unknown);
    REQUIRE_THROWS(string_to<aare::DetectorType>("invalid_detector"));
}

TEST_CASE("TimingMode string to enum") {
    REQUIRE(string_to<aare::TimingMode>("auto") == aare::TimingMode::Auto);
    REQUIRE(string_to<aare::TimingMode>("trigger") == aare::TimingMode::Trigger);
    REQUIRE_THROWS(string_to<aare::TimingMode>("invalid_mode"));
}

TEST_CASE("FrameDiscardPolicy string to enum") {
    REQUIRE(string_to<aare::FrameDiscardPolicy>("nodiscard") ==
            aare::FrameDiscardPolicy::NoDiscard);
    REQUIRE(string_to<aare::FrameDiscardPolicy>("discard") ==
            aare::FrameDiscardPolicy::Discard);
    REQUIRE(string_to<aare::FrameDiscardPolicy>("discardpartial") ==
            aare::FrameDiscardPolicy::DiscardPartial);
    REQUIRE_THROWS(string_to<aare::FrameDiscardPolicy>("invalid_policy"));
}

TEST_CASE("DACIndex string to enum") {
    // Numeric DACs
    REQUIRE(string_to<aare::DACIndex>("dac 0") == aare::DACIndex::DAC_0);
    REQUIRE(string_to<aare::DACIndex>("dac 1") == aare::DACIndex::DAC_1);
    REQUIRE(string_to<aare::DACIndex>("dac 2") == aare::DACIndex::DAC_2);
    REQUIRE(string_to<aare::DACIndex>("dac 3") == aare::DACIndex::DAC_3);
    REQUIRE(string_to<aare::DACIndex>("dac 4") == aare::DACIndex::DAC_4);
    REQUIRE(string_to<aare::DACIndex>("dac 5") == aare::DACIndex::DAC_5);
    REQUIRE(string_to<aare::DACIndex>("dac 6") == aare::DACIndex::DAC_6);
    REQUIRE(string_to<aare::DACIndex>("dac 7") == aare::DACIndex::DAC_7);
    REQUIRE(string_to<aare::DACIndex>("dac 8") == aare::DACIndex::DAC_8);
    REQUIRE(string_to<aare::DACIndex>("dac 9") == aare::DACIndex::DAC_9);
    REQUIRE(string_to<aare::DACIndex>("dac 10") == aare::DACIndex::DAC_10);
    REQUIRE(string_to<aare::DACIndex>("dac 11") == aare::DACIndex::DAC_11);
    REQUIRE(string_to<aare::DACIndex>("dac 12") == aare::DACIndex::DAC_12);
    REQUIRE(string_to<aare::DACIndex>("dac 13") == aare::DACIndex::DAC_13);
    REQUIRE(string_to<aare::DACIndex>("dac 14") == aare::DACIndex::DAC_14);
    REQUIRE(string_to<aare::DACIndex>("dac 15") == aare::DACIndex::DAC_15);
    REQUIRE(string_to<aare::DACIndex>("dac 16") == aare::DACIndex::DAC_16);
    REQUIRE(string_to<aare::DACIndex>("dac 17") == aare::DACIndex::DAC_17);

    // Named DACs
    REQUIRE(string_to<aare::DACIndex>("vsvp") == aare::DACIndex::VSVP);
    REQUIRE(string_to<aare::DACIndex>("vtrim") == aare::DACIndex::VTRIM);
    REQUIRE(string_to<aare::DACIndex>("vrpreamp") == aare::DACIndex::VRPREAMP);
    REQUIRE(string_to<aare::DACIndex>("vrshaper") == aare::DACIndex::VRSHAPER);
    REQUIRE(string_to<aare::DACIndex>("vsvn") == aare::DACIndex::VSVN);
    REQUIRE(string_to<aare::DACIndex>("vtgstv") == aare::DACIndex::VTGSTV);
    REQUIRE(string_to<aare::DACIndex>("vcmp_ll") == aare::DACIndex::VCMP_LL);
    REQUIRE(string_to<aare::DACIndex>("vcmp_lr") == aare::DACIndex::VCMP_LR);
    REQUIRE(string_to<aare::DACIndex>("vcal") == aare::DACIndex::VCAL);
    REQUIRE(string_to<aare::DACIndex>("vcmp_rl") == aare::DACIndex::VCMP_RL);
    REQUIRE(string_to<aare::DACIndex>("rxb_rb") == aare::DACIndex::RXB_RB);
    REQUIRE(string_to<aare::DACIndex>("rxb_lb") == aare::DACIndex::RXB_LB);
    REQUIRE(string_to<aare::DACIndex>("vcmp_rr") == aare::DACIndex::VCMP_RR);
    REQUIRE(string_to<aare::DACIndex>("vcp") == aare::DACIndex::VCP);
    REQUIRE(string_to<aare::DACIndex>("vcn") == aare::DACIndex::VCN);
    REQUIRE(string_to<aare::DACIndex>("vishaper") == aare::DACIndex::VISHAPER);
    REQUIRE(string_to<aare::DACIndex>("vthreshold") == aare::DACIndex::VTHRESHOLD);
    REQUIRE(string_to<aare::DACIndex>("vref_ds") == aare::DACIndex::VREF_DS);
    REQUIRE(string_to<aare::DACIndex>("vout_cm") == aare::DACIndex::VOUT_CM);
    REQUIRE(string_to<aare::DACIndex>("vin_cm") == aare::DACIndex::VIN_CM);
    REQUIRE(string_to<aare::DACIndex>("vref_comp") == aare::DACIndex::VREF_COMP);
    REQUIRE(string_to<aare::DACIndex>("vb_comp") == aare::DACIndex::VB_COMP);
    REQUIRE(string_to<aare::DACIndex>("vdd_prot") == aare::DACIndex::VDD_PROT);
    REQUIRE(string_to<aare::DACIndex>("vin_com") == aare::DACIndex::VIN_COM);
    REQUIRE(string_to<aare::DACIndex>("vref_prech") == aare::DACIndex::VREF_PRECH);
    REQUIRE(string_to<aare::DACIndex>("vb_pixbuf") == aare::DACIndex::VB_PIXBUF);
    REQUIRE(string_to<aare::DACIndex>("vb_ds") == aare::DACIndex::VB_DS);
    REQUIRE(string_to<aare::DACIndex>("vref_h_adc") == aare::DACIndex::VREF_H_ADC);
    REQUIRE(string_to<aare::DACIndex>("vb_comp_fe") == aare::DACIndex::VB_COMP_FE);
    REQUIRE(string_to<aare::DACIndex>("vb_comp_adc") == aare::DACIndex::VB_COMP_ADC);
    REQUIRE(string_to<aare::DACIndex>("vcom_cds") == aare::DACIndex::VCOM_CDS);
    REQUIRE(string_to<aare::DACIndex>("vref_rstore") == aare::DACIndex::VREF_RSTORE);
    REQUIRE(string_to<aare::DACIndex>("vb_opa_1st") == aare::DACIndex::VB_OPA_1ST);
    REQUIRE(string_to<aare::DACIndex>("vref_comp_fe") == aare::DACIndex::VREF_COMP_FE);
    REQUIRE(string_to<aare::DACIndex>("vcom_adc1") == aare::DACIndex::VCOM_ADC1);
    REQUIRE(string_to<aare::DACIndex>("vref_l_adc") == aare::DACIndex::VREF_L_ADC);
    REQUIRE(string_to<aare::DACIndex>("vref_cds") == aare::DACIndex::VREF_CDS);
    REQUIRE(string_to<aare::DACIndex>("vb_cs") == aare::DACIndex::VB_CS);
    REQUIRE(string_to<aare::DACIndex>("vb_opa_fd") == aare::DACIndex::VB_OPA_FD);
    REQUIRE(string_to<aare::DACIndex>("vcom_adc2") == aare::DACIndex::VCOM_ADC2);
    REQUIRE(string_to<aare::DACIndex>("vcassh") == aare::DACIndex::VCASSH);
    REQUIRE(string_to<aare::DACIndex>("vth2") == aare::DACIndex::VTH2);
    REQUIRE(string_to<aare::DACIndex>("vrshaper_n") == aare::DACIndex::VRSHAPER_N);
    REQUIRE(string_to<aare::DACIndex>("vipre_out") == aare::DACIndex::VIPRE_OUT);
    REQUIRE(string_to<aare::DACIndex>("vth3") == aare::DACIndex::VTH3);
    REQUIRE(string_to<aare::DACIndex>("vth1") == aare::DACIndex::VTH1);
    REQUIRE(string_to<aare::DACIndex>("vicin") == aare::DACIndex::VICIN);
    REQUIRE(string_to<aare::DACIndex>("vcas") == aare::DACIndex::VCAS);
    REQUIRE(string_to<aare::DACIndex>("vcal_n") == aare::DACIndex::VCAL_N);
    REQUIRE(string_to<aare::DACIndex>("vipre") == aare::DACIndex::VIPRE);
    REQUIRE(string_to<aare::DACIndex>("vcal_p") == aare::DACIndex::VCAL_P);
    REQUIRE(string_to<aare::DACIndex>("vdcsh") == aare::DACIndex::VDCSH);
    REQUIRE(string_to<aare::DACIndex>("vbp_colbuf") == aare::DACIndex::VBP_COLBUF);
    REQUIRE(string_to<aare::DACIndex>("vb_sda") == aare::DACIndex::VB_SDA);
    REQUIRE(string_to<aare::DACIndex>("vcasc_sfp") == aare::DACIndex::VCASC_SFP);
    REQUIRE(string_to<aare::DACIndex>("vipre_cds") == aare::DACIndex::VIPRE_CDS);
    REQUIRE(string_to<aare::DACIndex>("ibias_sfp") == aare::DACIndex::IBIAS_SFP);
    REQUIRE(string_to<aare::DACIndex>("trimbits") == aare::DACIndex::TRIMBIT_SCAN);
    REQUIRE(string_to<aare::DACIndex>("highvoltage") == aare::DACIndex::HIGH_VOLTAGE);
    REQUIRE(string_to<aare::DACIndex>("iodelay") == aare::DACIndex::IO_DELAY);
    REQUIRE(string_to<aare::DACIndex>("temp_adc") == aare::DACIndex::TEMPERATURE_ADC);
    REQUIRE(string_to<aare::DACIndex>("temp_fpga") == aare::DACIndex::TEMPERATURE_FPGA);
    REQUIRE(string_to<aare::DACIndex>("temp_fpgaext") == aare::DACIndex::TEMPERATURE_FPGAEXT);
    REQUIRE(string_to<aare::DACIndex>("temp_10ge") == aare::DACIndex::TEMPERATURE_10GE);
    REQUIRE(string_to<aare::DACIndex>("temp_dcdc") == aare::DACIndex::TEMPERATURE_DCDC);
    REQUIRE(string_to<aare::DACIndex>("temp_sodl") == aare::DACIndex::TEMPERATURE_SODL);
    REQUIRE(string_to<aare::DACIndex>("temp_sodr") == aare::DACIndex::TEMPERATURE_SODR);
    REQUIRE(string_to<aare::DACIndex>("temp_fpgafl") == aare::DACIndex::TEMPERATURE_FPGA2);
    REQUIRE(string_to<aare::DACIndex>("temp_fpgafr") == aare::DACIndex::TEMPERATURE_FPGA3);
    REQUIRE(string_to<aare::DACIndex>("temp_slowadc") == aare::DACIndex::SLOW_ADC_TEMP);
    
    REQUIRE_THROWS(string_to<aare::DACIndex>("invalid_dac"));
}

TEST_CASE("Remove unit from string") {
    using aare::remove_unit;
    
    // Test basic numeric value with unit
    {
        std::string input = "123.45 V";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "V");
        REQUIRE(input == "123.45");
    }
    
    // Test integer value with unit
    {
        std::string input = "42 Hz";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "Hz");
        REQUIRE(input == "42");
    }
    
    // Test negative value with unit
    {
        std::string input = "-50.5 mV";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "mV");
        REQUIRE(input == "-50.5");
    }
    
    // Test value with no unit (only numbers)
    {
        std::string input = "123.45";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "");
        REQUIRE(input == "123.45");
    }
    
    // Test value with only unit (letters at start)
    {
        std::string input = "kHz";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "kHz");
        REQUIRE(input == "");
    }
    
    // Test with multiple word units
    {
        std::string input = "100 degrees Celsius";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "degrees Celsius");
        REQUIRE(input == "100");
    }
    
    // Test with scientific notation
    {
        std::string input = "1.23e-5 A";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "A");
        REQUIRE(input == "1.23e-5");
    }

    // Another test with scientific notation
    {
        std::string input = "-4.56E6 m/s";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "m/s");
        REQUIRE(input == "-4.56E6");
    }
    
    // Test with scientific notation uppercase
    {
        std::string input = "5.67E+3 Hz";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "Hz");
        REQUIRE(input == "5.67E+3");
    }
    
    // Test with leading zeros
    {
        std::string input = "00123 ohm";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "ohm");
        REQUIRE(input == "00123");
    }

    // Test with leading zeros no space
    {
        std::string input = "00123ohm";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "ohm");
        REQUIRE(input == "00123");
    }
    
    // Test empty string
    {
        std::string input = "";
        std::string unit = remove_unit(input);
        REQUIRE(unit == "");
        REQUIRE(input == "");
    }
}

TEST_CASE("Conversions from time string to chrono durations") {
    using namespace std::chrono;
    using aare::string_to;

    REQUIRE(string_to<nanoseconds>("100 ns") == nanoseconds(100));
    REQUIRE(string_to<nanoseconds>("1s") == nanoseconds(1000000000));
    REQUIRE(string_to<microseconds>("200 us") == microseconds(200));
    REQUIRE(string_to<milliseconds>("300 ms") == milliseconds(300));
    REQUIRE(string_to<seconds>("5 s") == seconds(5));

    REQUIRE(string_to<nanoseconds>("1.5 us") == nanoseconds(1500));
    REQUIRE(string_to<microseconds>("2.5 ms") == microseconds(2500));
    REQUIRE(string_to<milliseconds>("3.5 s") == milliseconds(3500));

    REQUIRE(string_to<seconds>("2") == seconds(2)); // No unit defaults to seconds

    REQUIRE_THROWS(string_to<seconds>("10 min")); // Unsupported unit
}