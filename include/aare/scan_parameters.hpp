#pragma once

#include <string>
#include <sstream>

namespace aare {

class ScanParameters {
    bool m_enabled = false;
    std::string m_dac;
    int m_start = 0;
    int m_stop = 0;
    int m_step = 0;
    // ns m_dac_settle_time{0};
    //  TODO! add settleTime, requires string to time conversion

  public:
    // "[enabled\ndac dac 4\nstart 500\nstop 2200\nstep 5\nsettleTime 100us\n]"
    // TODO: use StringTo<ScanParameters> and move this to to_string
    // add ways of setting the members of the class

    ScanParameters(const std::string &par) {
        std::istringstream iss(par.substr(1, par.size() - 2));
        std::string line;
        while (std::getline(iss, line)) {
            if (line == "enabled") {
                m_enabled = true;
            } else if (line.find("dac") != std::string::npos) {
                m_dac = line.substr(4);
            } else if (line.find("start") != std::string::npos) {
                m_start = std::stoi(line.substr(6));
            } else if (line.find("stop") != std::string::npos) {
                m_stop = std::stoi(line.substr(5));
            } else if (line.find("step") != std::string::npos) {
                m_step = std::stoi(line.substr(5));
            }
        }
    };
    ScanParameters() = default;
    ScanParameters(const ScanParameters &) = default;
    ScanParameters &operator=(const ScanParameters &) = default;
    ScanParameters(ScanParameters &&) = default;
    int start() const { return m_start; };
    int stop() const { return m_stop; };
    int step() const { return m_step; };
    const std::string &dac() const { return m_dac; };
    bool enabled() const { return m_enabled; };
    void increment_stop() { m_stop += 1; };
};

} // namespace aare