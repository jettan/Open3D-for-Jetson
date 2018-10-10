//
// Created by wei on 18-3-29.
//

#pragma once

#include "MemoryHeapCuda.h"
#include <Cuda/Common/UtilsCuda.h>
#include <Core/Core.h>
#include <cassert>

namespace open3d {

/**
 * Server end
 * In fact we don't know which indices are used and which are freed,
 * we can only give a coarse boundary test.
 */
template<typename T>
__device__
int &MemoryHeapCudaServer<T>::get_heap(int index) {
    assert(index >= 0 && index < max_capacity_);
    return heap_[index];
}

template<typename T>
__device__
T &MemoryHeapCudaServer<T>::get_value(int addr) {
    assert(addr >= 0 && addr < max_capacity_);
    return data_[addr];
}

/**
 * Only the unallocated part is maintained
 * Notice: heap grows toward low-numbered addresses
 * heap addr <- Free(*1*)        addr <- Malloc()         addr
 * N-1 | 0 |                 => | 0 |                 => | 0 |
 * N-2 | 1 |                 => | 1 |                 => | 1 |
 * N-3 | 2 |                 => | 2 |                 => | 2 |
 * N-4 | 3 |                 => |*1*| <- heap_counter => | 1 |
 * N-5 | 4 | <- heap_counter => | 4 |                 => | 4 | <- heap_counter
 * N-6 | 5 |                 => | 5 |                 => | 5 |
 *      ...                  =>  ...                     ...
 * 0   |N-1|                 => |N-1|                 => |N-1|
 */
template<class T>
__device__
int MemoryHeapCudaServer<T>::Malloc() {
    int index = atomicSub(heap_counter_, 1);
    if (index < 0) {
        printf("Heap exhausted, return.\n");
        return -1;
    }
    return heap_[index];
}

template<class T>
__device__
void MemoryHeapCudaServer<T>::Free(int addr) {
    int index = atomicAdd(heap_counter_, 1);
    assert(index + 1 < max_capacity_);
    heap_[index + 1] = addr;
}

/**
 * Client end
 */

template<typename T>
MemoryHeapCuda<T>::MemoryHeapCuda() {
    max_capacity_ = -1;
}

template<typename T>
MemoryHeapCuda<T>::~MemoryHeapCuda() {
    Release();
}

template<typename T>
MemoryHeapCuda<T>::MemoryHeapCuda(const MemoryHeapCuda<T> &other) {
    PrintInfo("Copy construct\n");
    server_ = other.server();
    max_capacity_ = other.max_capacity();
}

template<typename T>
MemoryHeapCuda<T>& MemoryHeapCuda<T>::operator=(
    const MemoryHeapCuda<T> &other) {
    if (this != &other) {
        Release();

        server_ = other.server();
        max_capacity_ = other.max_capacity();
    }
    return *this;
}

template<typename T>
void MemoryHeapCuda<T>::Create(int max_capacity) {
    assert(max_capacity > 0);

    if (server_ != nullptr) {
        PrintWarning("Already created, stop re-creating!\n");
        return;
    }

    max_capacity_ = max_capacity;

    server_ = std::make_shared<MemoryHeapCudaServer<T>>();
    server_->max_capacity_ = max_capacity;

    CheckCuda(cudaMalloc(&(server_->heap_counter_), sizeof(int)));
    CheckCuda(cudaMalloc(&(server_->heap_), sizeof(int) * max_capacity));
    CheckCuda(cudaMalloc(&(server_->data_), sizeof(T) * max_capacity));

    Reset();
}

template<typename T>
void MemoryHeapCuda<T>::Release() {
    if (server_ != nullptr && server_.use_count() == 1) {
        CheckCuda(cudaFree(server_->heap_counter_));
        CheckCuda(cudaFree(server_->heap_));
        CheckCuda(cudaFree(server_->data_));
    }

    server_ = nullptr;
    max_capacity_ = -1;
}

template<typename T>
void MemoryHeapCuda<T>::Reset() {
    const int threads = THREAD_1D_UNIT;
    const int blocks = UPPER_ALIGN(max_capacity_, THREAD_1D_UNIT);

    ResetMemoryHeapKernel << < blocks, threads >> > (*server_);
    CheckCuda(cudaDeviceSynchronize());
    CheckCuda(cudaGetLastError());

    int heap_counter = max_capacity_ - 1;
    CheckCuda(cudaMemcpy(server_->heap_counter_, &heap_counter,
                         sizeof(int), cudaMemcpyHostToDevice));
}

template<typename T>
std::vector<int> MemoryHeapCuda<T>::DownloadHeap() {
    std::vector<int> ret;
    ret.resize(max_capacity_);

    CheckCuda(cudaMemcpy(ret.data(), server_->heap_,
                         sizeof(int) * max_capacity_,
                         cudaMemcpyDeviceToHost));
    return ret;
}

template<typename T>
std::vector<T> MemoryHeapCuda<T>::DownloadValue() {
    std::vector<T> ret;
    ret.resize(max_capacity_);

    CheckCuda(cudaMemcpy(ret.data(), server_->data_,
                         sizeof(T) * max_capacity_,
                         cudaMemcpyDeviceToHost));

    return ret;
}

template<typename T>
int MemoryHeapCuda<T>::HeapCounter() {
    int heap_counter;

    CheckCuda(cudaMemcpy(&heap_counter, server_->heap_counter_,
                         sizeof(int),
                         cudaMemcpyDeviceToHost));

    return heap_counter;
}
};