#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <memory>
#include <boost/program_options.hpp>

#include "aare/network_io/ZmqSocketSender.hpp"

#include <boost/program_options/options_description.hpp>


using namespace aare;
using namespace std;
namespace po = boost::program_options;

int main(int argc, char **argv) {
    po::options_description desc("Allowed options");
    desc.add_options()
    ("help", "produce help message")
    // ("input,i", po::value<string>(), "input file");
    ("port,p", po::value<int>(), "port number");
    // ("loop,l", "loop over the file");
    // po::positional_options_description pd;
    // pd.add("input,i", 1);
}