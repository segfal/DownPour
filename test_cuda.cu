#include <cuda_runtime.h>
#include <iostream>

// Simple CUDA kernel for smoke test
__global__ void vectorAdd(const float* a, const float* b, float* c, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        c[idx] = a[idx] + b[idx];
    }
}

int main() {
    std::cout << "CUDA Smoke Test - Compilation Successful!" << std::endl;
    
    // Query CUDA device properties (no actual execution needed for smoke test)
    int deviceCount = 0;
    cudaError_t error = cudaGetDeviceCount(&deviceCount);
    
    if (error == cudaSuccess && deviceCount > 0) {
        std::cout << "CUDA devices found: " << deviceCount << std::endl;
        
        cudaDeviceProp prop;
        cudaGetDeviceProperties(&prop, 0);
        std::cout << "Device 0: " << prop.name << std::endl;
        std::cout << "Compute Capability: " << prop.major << "." << prop.minor << std::endl;
    } else {
        std::cout << "No CUDA devices found (expected in CI environment)" << std::endl;
        std::cout << "Compilation test passed!" << std::endl;
    }
    
    return 0;
}
