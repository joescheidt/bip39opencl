#include <stdio.h>
#include <iostream>
#include <cstring>
#include "bip39opencl.h"

void list_device(struct device_info &device)
{
    std::cout << "ID:         " << device.logical_id << std::endl;
    std::cout << "Name:       " << device.name << std::endl;
    std::cout << "Memory:     " << (device.memory / (1024 * 1024)) << "MB" << std::endl;
    std::cout << "Processors: " << device.cores << std::endl;
    std::cout << "Clock:      " << device.clock_frequency << "MHz" << std::endl;
}

void list_devices(std::vector<struct device_info> &devices)
{
    for(int i = 0; i < devices.size(); i++) {
        list_device(devices[i]);
        if(i < devices.size() - 1) {
            std::cout << std::endl;
        }
    }
}

bool parse_int(std::string s, int *x)
{
    if(sscanf(s.c_str(), "%d", x) != 1) {
        return false;
    }

    return true;
}

bool parse_uint64(std::string s, uint64_t *x)
{
    if(sscanf(s.c_str(), "%lld", x) != 1) {
        return false;
    }

    return true;
}

int main(int argc, char **argv){
  std::vector<std::string> args;
    for(int i = 1; i < argc; i++) {
        args.push_back(std::string(argv[i]));
    }

  int selected_device = 0;
  std::vector<struct device_info> devices = get_devices();
  if(devices.size() == 0) {
    std::cout << "No OpenCL devices found" << std::endl;
    return 1;
    }
  
  for(int i = 0; i < args.size(); i++) {
        if(args[i] == "--list-devices") {
            list_devices(devices);
            return 0;
        }
    }

  std::vector<std::string> operands;

    for(int i = 0; i < args.size(); i++) {
        std::string arg = args[i];
        std::string prefix = args[i] + ": ";
        bool arg_consumed = false;

        if(arg == "--device") {
            if(args.size() <= i + 1) {
                std::cout << prefix << "argument required" << std::endl;
                return 1;
            }
            if(!parse_int(args[i + 1], &selected_device)) {
                std::cout << prefix << "invalid argument" << std::endl;
                return 1;
            }

            if(selected_device >= devices.size()) {
                std::cout << "Invalid device ID" << std::endl;
                return 1;
            }

            arg_consumed = true;
        }  else {
            operands.push_back(args[i]);
        }

        if(arg_consumed) {
            i++;
        }
    }
  std::cout << "Selected device: " << devices[selected_device].name  << std::endl;
  mnemonic_cl(devices[selected_device]);     
}