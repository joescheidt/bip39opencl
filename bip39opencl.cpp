#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>
#include <CL/cl.h>

#include "util.h"
#include "cl_util.h"
#include "bip39opencl.h"

extern char _bip39_cl[];

std::string _kernelSource;

void clCall(cl_int err)
{
    if(err != CL_SUCCESS) {
        std::cout << "OpenCL error: " << getErrorString(err) << std::endl;
        exit(1);
    }
}

void load_kernel_source()
{
    _kernelSource = std::string(_bip39_cl);
}

std::vector<struct device_info> get_devices()
{
    int err = 0;

    unsigned int num_platforms = 0;

    cl_platform_id platform_ids[10];

    std::vector<struct device_info> devices;

    err = clGetPlatformIDs(0, NULL, &num_platforms);

    if(num_platforms == 0) {
        return devices;
    }

    clGetPlatformIDs(num_platforms, platform_ids, NULL);

    // Get device ID's and names
    for(unsigned int i = 0; i < num_platforms; i++) {
        unsigned int num_devices = 0;
        cl_device_id ids[10];

        clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices);

        if(num_devices == 0) {
            continue;
        }

        clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_ALL, num_devices, ids, NULL);

        for(unsigned int j = 0; j < num_devices; j++) {
            struct device_info d;

            d.logical_id = (int)devices.size();
            d.id = ids[j];

            // Get device name
            char buf[256] = { 0 };
            size_t name_size;
            clGetDeviceInfo(ids[j], CL_DEVICE_NAME, sizeof(buf), buf, &name_size);
            d.name = std::string(buf, name_size);

            unsigned int cores = 0;
            clGetDeviceInfo(ids[j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cores), &cores, NULL);
            d.cores = cores;

            unsigned int frequency = 0;
            clGetDeviceInfo(ids[j], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(frequency), &frequency, NULL);
            d.clock_frequency = frequency;

            uint64_t memory = 0;
            clGetDeviceInfo(ids[j], CL_DEVICE_GLOBAL_MEM_SIZE, sizeof(memory), &memory, NULL);
            d.memory = memory;

            devices.push_back(d);
        }
    }

    return devices;
}

cl_device_id get_device(int idx)
{
    int err = 0;

    unsigned int num_platforms = 0;

    cl_platform_id platform_ids[10];
    std::vector<cl_device_id> device_ids;
    std::vector<std::string> device_names;

    err = clGetPlatformIDs(0, NULL, &num_platforms);

    if(num_platforms == 0) {
        std::cout << "No OpenCL platforms found" << std::endl;
        return 0;
    }

    clGetPlatformIDs(num_platforms, platform_ids, NULL);

    // Get device ID's and names
    for(unsigned int i = 0; i < num_platforms; i++) {
        unsigned int num_devices = 0;
        cl_device_id ids[10];

        clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices);

        if(num_devices == 0) {
            continue;
        }

        clGetDeviceIDs(platform_ids[i], CL_DEVICE_TYPE_ALL, num_devices, ids, NULL);

        for(unsigned int j = 0; j < num_devices; j++) {
            char buf[256] = { 0 };
            size_t name_size;

            clGetDeviceInfo(ids[j], CL_DEVICE_NAME, sizeof(buf), buf, &name_size);
            device_ids.push_back(ids[j]);
            device_names.push_back(std::string(buf));
        }
    }

    std::cout << "Selected " << device_names[idx] << std::endl;

    return device_ids[idx];
}
// opencl
static void find_best_parameters(struct device_info &device, cl_context ctx, cl_command_queue cmd, cl_kernel kernel, size_t &global, size_t &local)
{
    unsigned int iterations = 2000;

    std::vector<int> local_sizes({ {32, 64, 128, 256, 384, 512, 768, 1024, 1536, 2048}});

    std::vector<unsigned int> speeds;

    unsigned int highest = 0;
    int highest_idx = 0;

    std::cout << "Finding optimal kernel size for " << device.name << std::endl;

    for(int i = 0; i < local_sizes.size(); i++) {
        size_t global_size = local_sizes[i] * device.cores;
        size_t local_size = local_sizes[i];

        int err = 0;

 
        cl_mem dev_hashes = clCreateBuffer(ctx, 0, sizeof(uint8_t) * 8 * global, NULL, &err);
        if(err) {
            continue;
        }

        uint64_t t0 = getSystemTime();

        if(clSetKernelArg(kernel, 0, sizeof(cl_mem), &dev_hashes)) {
            clReleaseMemObject(dev_hashes);
            continue;
        }
        
        if(clSetKernelArg(kernel, 1, sizeof(unsigned int), &iterations)) {
            clReleaseMemObject(dev_hashes);
            continue;
        }

        if(clEnqueueNDRangeKernel(cmd, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL)) {
            clReleaseMemObject(dev_hashes);
            continue;
        }

        if(clFinish(cmd)) {
            clReleaseMemObject(dev_hashes);
            continue;
        }

        uint64_t t1 = getSystemTime() - t0;
        clReleaseMemObject(dev_hashes);

        unsigned int speed = (unsigned int)(((double)global_size * iterations) / ((double)t1 / 1000.0));

        speeds.push_back(speed);

        if(speed > highest) {
            highest = speed;
            highest_idx = i;
        }
    }

    local = local_sizes[highest_idx];
    global = local * device.cores;
}

static void do_get_mnemonic(struct device_info &device)
{
  cl_context ctx;
  cl_command_queue cmd;
  cl_program prog;
  cl_kernel kernel;
  cl_int err = 0;

  ctx = clCreateContext(0, 1, &device.id, NULL, NULL, &err);
  clCall(err);
  
  cmd = clCreateCommandQueue(ctx, device.id, 0, &err);
  clCall(err);

  size_t len = _kernelSource.length();
  const char *ptr = _kernelSource.c_str();
  prog = clCreateProgramWithSource(ctx, 1, &ptr, &len, &err);
  clCall(err);

  std::cout << "Building kernel... ";
  err = clBuildProgram(prog, 0, NULL, NULL, NULL, NULL);
  if(err != CL_SUCCESS)
  {
    char *buffer = NULL;
    size_t len = 0;
    std::cout << "Error: Failed to build program executable! error code " << err << std::endl;
    clGetProgramBuildInfo(prog, device.id, CL_PROGRAM_BUILD_LOG, 0, NULL, &len);
    buffer = new char[len];
    clGetProgramBuildInfo(prog, device.id, CL_PROGRAM_BUILD_LOG, len, buffer, NULL);
    std::cout << buffer << std::endl;
    delete[] buffer;
    clCall(err);
  }

  std::cout << "Done" << std::endl;

  kernel = clCreateKernel(prog, "int_to_address", &err);
  clCall(err);
  size_t global = device.cores * 1024;
  size_t local = 64;
  find_best_parameters(device, ctx, cmd, kernel, global, local);
  cl_mem res_address_buff;
  cl_mem target_mnemonic_buff;
  cl_mem found_mnemonic_buff;

  res_address_buff = clCreateBuffer(ctx, 0, sizeof(uint8_t) * 20 , NULL, &err);
  clCall(err);
  target_mnemonic_buff = clCreateBuffer(ctx, 0, sizeof(uint8_t) * 120 , NULL, &err);
  clCall(err);
  found_mnemonic_buff = clCreateBuffer(ctx, 0, sizeof(uint8_t) * 1, NULL, &err);
  clCall(err);
  
  uint8_t res_address[20] = {0};  
  uint8_t target_mnemonic[120] = {0};
  uint8_t found_mnemonic[1] = {0};
  
  int kernel_calls = 0;
  std::cout << "Running..." << std::endl;
  for(*found_mnemonic == 1 )
  {
    clCall(clSetKernelArg(kernel, 0, sizeof(uint8_t), NULL));
    clCall(clSetKernelArg(kernel, 1, sizeof(uint8_t), NULL));
    clCall(clSetKernelArg(kernel, 2, sizeof(uint8_t), NULL));
    clCall(clEnqueueReadBuffer(cmd, res_address_buff, CL_TRUE, 0, sizeof(uint8_t) * 20 , &res_address, 0, NULL, NULL));
    clCall(clEnqueueReadBuffer(cmd, target_mnemonic_buff, CL_TRUE, 0, sizeof(uint8_t) * 120 , &target_mnemonic, 0, NULL, NULL));
    clCall(clEnqueueReadBuffer(cmd, found_mnemonic_buff, CL_TRUE, 0, sizeof(uint8_t), &found_mnemonic, 0, NULL, NULL));
    clCall(clEnqueueNDRangeKernel(cmd, kernel, 1, NULL, &global, &local, 0, NULL, NULL));
    clCall(clFinish(cmd));
    kernel_calls++;
    uint64_t t1 = getSystemTime() - t0;
    total_time += t1;
    if(t1 >= 1800) 
    {
        std::cout << device.name.substr(0, 16) << "| "
        << formatSeconds((unsigned int)(total_time/1000)) << " "
        << ((kernel_calls * global) / (t1/1000)) << "/sec "
        << std::endl;
        t0 = getSystemTime();
        kernel_calls = 0;
    }
  }
}
bool mnemonic_cl(struct device_info &device){
  load_kernel_source();
  do_get_mnemonic(device);
}

