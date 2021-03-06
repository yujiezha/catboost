#pragma once
#include <catboost/cuda/cuda_lib/kernel/kernel.cuh>

namespace NKernel {
    template<typename T>
    void AddVector(T *x, const T *y, ui64 size, TCudaStream stream);

    template<typename T>
    void AddVector(T *x, const T y, ui64 size, TCudaStream stream);

    template<typename T>
    void SubtractVector(T *x, const T *y, ui64 size, TCudaStream stream);

    template<typename T>
    void MultiplyVector(T *x, const T *y, ui64 size, TCudaStream stream);

    template<typename T>
    void MultiplyVector(T *x, const T c, ui64 size, TCudaStream stream);

    template<typename T>
    void DivideVector(T *x, const T *y, ui64 size, TCudaStream stream);

    template<typename T>
    void ExpVector(T *x, ui64 size, TCudaStream stream);

    template<typename T, typename Index>
    void Gather(T *dst, const T* src, const Index* map, ui64 size, TCudaStream stream);
//
    template<typename T, typename Index>
    void Scatter(T *dst, const T* src, const Index* map, ui64 size, TCudaStream stream);

    template<typename T, typename Index>
    void GatherWithMask(T *dst, const T* src, const Index* map,  ui64 size, Index mapMask, TCudaStream stream);
//
    template<typename T, typename Index>
    void ScatterWithMask(T *dst, const T* src, const Index* map, ui64 size,  Index mapMask, TCudaStream stream);

    template<typename T>
    void Reverse(T* data, ui64 size, TCudaStream stream);

//
//    template<typename T>
//    void Reduce(T* data, ui64 size, uint partCount, TCudaStream stream);
//
//

}
