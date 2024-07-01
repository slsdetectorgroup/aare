#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace aare {
class ArgParser {
  public:
    std::string desc;
    struct option_ {
        std::string name;
        std::string short_name;
        bool requires_value;
        bool required;
        std::string default_value;
        std::string description;
    };
    std::map<std::string, option_> options;
    ArgParser(std::string program_description = "") : desc(program_description) {}
    /**
     * @brief
     *
     * @param name name of the options and should be unique, can be used in argument with --name
     * @param short_name short name of the option and should be unique, can be used in argument with -short_name
     * @param requires_value should be true if the option requires a value (e.g. --name value) and false if it does not
     * (e.g. ls -l)
     * @param required should be true if the option is required (cannot have a default value and must require a value)
     * @param default_value
     * @param description description of the option
     * @return ArgParser
     */
    ArgParser add_option(std::string name, std::string short_name, bool requires_value, bool required = false,
                         std::string default_value = "", std::string description = "") {

        if (name == "" || short_name == "") {
            std::cerr << "Name and short name cannot be empty" << std::endl;
            exit(1);
        }

        if (options.find(name) != options.end()) {
            std::cerr << "Option " << name << " already exists" << std::endl;
            exit(1);
        }
        if (short_name[0] == '-') {
            std::cerr << "Short name cannot be -" << std::endl;
            exit(1);
        }
        if (required && default_value != "") {
            std::cerr << "Option " << name << " is required, default value is not allowed" << std::endl;
            exit(1);
        }
        if (required && !requires_value) {
            std::cerr << "Option " << name << " is required and does not require a value" << std::endl;
            exit(1);
        }
        if (!requires_value && !(default_value == "" || default_value == "0")) {
            std::cerr << "Option " << name << " does not require a value, default value must be 0 or \"\" "
                      << std::endl;
            exit(1);
        }
        if (!requires_value && default_value != "") {
            default_value = "";
        }
        options[name] = {name, short_name, requires_value, required, default_value, description};
        return *this;
    }
    void reset() { options.clear(); }
    std::map<std::string, std::string> parse(int argc, char **argv) {
        std::map<std::string, std::string> result;
        for (auto &[name, option] : options) {
            if (option.default_value != "") {
                result[name] = option.default_value;
            }
        }
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg[0] == '-') {
                if (arg[1] == '-') {
                    arg = arg.substr(2);
                    if (options.find(arg) == options.end()) {
                        std::cerr << "Unknown option: " << arg << std::endl;
                        print_help();
                        exit(1);
                    }
                    if (options[arg].requires_value) {
                        if (i + 1 >= argc) {
                            std::cerr << "Option " << arg << " requires a value" << std::endl;
                            print_help();
                            exit(1);
                        }
                        result[arg] = argv[i + 1];
                        i++;
                    } else {
                        result[arg] = "1";
                    }
                } else {
                    arg = arg.substr(1);
                    bool found = false;
                    for (auto &[name, option] : options) {
                        if (option.short_name == arg) {
                            found = true;
                            if (option.requires_value) {
                                if (i + 1 >= argc) {
                                    std::cerr << "Option " << arg << " requires a value" << std::endl;
                                    print_help();
                                    exit(1);
                                }
                                result[name] = argv[i + 1];
                                i++;
                            } else {
                                result[name] = "1";
                            }
                            break;
                        }
                    }
                    if (!found) {
                        std::cerr << "Unknown option: " << arg << std::endl;
                        print_help();
                        exit(1);
                    }
                }
            }
        }
        for (auto &[name, option] : options) {
            if (option.required && result[name] == "") {
                std::cerr << "Option " << name << " is required" << std::endl;
                print_help();
                exit(1);
            }
            if (!option.requires_value) {
                if (result[name] == "") {
                    result[name] = "0";
                }
            }
        }

        return result;
    }
    void print_help() {
        std::cout << desc << std::endl;
        for (auto &[name, option] : options) {
            std::cout << "  --" << option.name << " -" << option.short_name;
            if (option.requires_value) {
                std::cout << " <value>";
            }
            std::cout << " : " << option.description;
            if (option.required) {
                std::cout << " (required)";
            }
            if (option.default_value != "") {
                std::cout << " (default: " << option.default_value << ")";
            }
            std::cout << std::endl;
        }
    }
};
} // namespace aare