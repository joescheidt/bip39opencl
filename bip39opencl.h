#ifndef _BIP39GPU_H
#define _BIP39GPU_H

#include <string>
#include <stdint.h>
#include <vector>
#include <CL/cl.h>

struct device_info {
    cl_device_id id;
    int logical_id;
    std::string name;
    unsigned int cores;
    unsigned int clock_frequency;
    uint64_t memory;
};
std::vector<struct device_info> get_devices();
bool mnemonic_cl(struct device_info &device);
#endif