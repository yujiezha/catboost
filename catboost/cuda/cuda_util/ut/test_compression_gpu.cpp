#include <catboost/cuda/cuda_lib/cuda_base.h>
#include <library/unittest/registar.h>
#include <iostream>
#include <thread>
#include <catboost/cuda/cuda_lib/cuda_manager.h>
#include <catboost/cuda/cuda_util/cpu_random.h>
#include <catboost/cuda/cuda_lib/cuda_buffer.h>
#include <catboost/cuda/cuda_util/fill.h>
#include <catboost/cuda/cuda_util/compression_helpers.h>
#include <catboost/cuda/cuda_util/compression_helpers_gpu.h>
#include <catboost/cuda/cuda_lib/cuda_profiler.h>
#include <catboost/cuda/cuda_util/helpers.h>

using namespace NCudaLib;

SIMPLE_UNIT_TEST_SUITE(TCompressionGpuTest) {
    SIMPLE_UNIT_TEST(TestCompressAndDecompress) {
        StartCudaManager();
        {
            ui32 tries = 5;
            ui64 bits = 25;
            TRandom rand(0);

            for (ui32 i = 0; i < tries; ++i) {
                for (ui32 bitsPerKey = 1; bitsPerKey < bits; ++bitsPerKey) {
                    yvector<ui32> vec;
                    ui32 uniqueValues = (1 << bitsPerKey);

                    ui64 size = 100000 + rand.NextUniformL() % 10000;
                    for (ui64 i = 0; i < size; ++i) {
                        vec.push_back(rand.NextUniformL() % uniqueValues);
                    }

                    auto vecGpu = TMirrorBuffer<ui32>::Create(TMirrorMapping(vec.size()));
                    auto decompressedGpu = TMirrorBuffer<ui32>::CopyMapping(vecGpu);
                    vecGpu.Write(vec);

                    auto compressedMapping = CompressedSize<ui64>(vecGpu, uniqueValues);
                    auto compressedGpu = TMirrorBuffer<ui64>::Create(compressedMapping);
                    Compress(vecGpu, compressedGpu, uniqueValues);
                    FillBuffer(decompressedGpu, static_cast<ui32>(0));
                    Decompress(compressedGpu, decompressedGpu, uniqueValues);

                    yvector<ui32> decompressedTmp;
                    decompressedGpu.Read(decompressedTmp);

                    for (ui32 i = 0; i < size; ++i) {
                        UNIT_ASSERT_VALUES_EQUAL(vec[i], decompressedTmp[i]);
                    }
                }
            }
        }
        StopCudaManager();
    }

    SIMPLE_UNIT_TEST(TestCompressAndGatherDecompress) {
        StartCudaManager();
        {
            ui32 tries = 5;
            ui64 bits = 25;
            TRandom rand(0);

            for (ui32 i = 0; i < tries; ++i) {
                for (ui32 bitsPerKey = 1; bitsPerKey < bits; ++bitsPerKey) {
                    yvector<ui32> vec;
                    yvector<ui32> map;
                    ui32 uniqueValues = (1 << bitsPerKey);

                    ui64 size = 100000 + rand.NextUniformL() % 10000;
                    const ui32 idxMask = (1u << 31) - 1;
                    for (ui64 i = 0; i < size; ++i) {
                        vec.push_back(rand.NextUniformL() % uniqueValues);
                        ui32 idx = i;
                        if (rand.NextUniform() < 0.05) {
                            idx |= (1u << 31);
                        }
                        map.push_back(idx);
                    }
                    std::random_shuffle(map.begin(), map.end(), rand);

                    auto vecGpu = TMirrorBuffer<ui32>::Create(TMirrorMapping(vec.size()));
                    auto mapGpu = TMirrorBuffer<ui32>::Create(TMirrorMapping(vec.size()));
                    auto decompressedGpu = TMirrorBuffer<ui32>::CopyMapping(vecGpu);
                    vecGpu.Write(vec);
                    mapGpu.Write(map);

                    auto compressedMapping = CompressedSize<ui64>(vecGpu, uniqueValues);
                    auto compressedGpu = TMirrorBuffer<ui64>::Create(compressedMapping);
                    Compress(vecGpu, compressedGpu, uniqueValues);
                    FillBuffer(decompressedGpu, static_cast<ui32>(0));
                    GatherFromCompressed(compressedGpu, uniqueValues, mapGpu, idxMask, decompressedGpu);

                    yvector<ui32> decompressedTmp;
                    decompressedGpu.Read(decompressedTmp);

                    for (ui32 i = 0; i < size; ++i) {
                        UNIT_ASSERT_VALUES_EQUAL(vec[map[i] & idxMask], decompressedTmp[i]);
                    }
                }
            }
        }
        StopCudaManager();
    }

    template <class TStorageType, NCudaLib::EPtrType Type = NCudaLib::CudaDevice>
    void BenchmarkCompress(ui32 tries = 10) {
        StartCudaManager();
        {
            ui64 bits = 25;
            TRandom rand(0);

            auto& profiler = GetCudaManager().GetProfiler();
            SetDefaultProfileMode(EProfileMode::ImplicitLabelSync);

            for (ui32 bitsPerKey = 1; bitsPerKey < bits; ++bitsPerKey) {
                yvector<ui32> vec;
                ui32 uniqueValues = (1 << bitsPerKey);

                ui64 size = 10000000;
                for (ui64 i = 0; i < size; ++i) {
                    vec.push_back(rand.NextUniformL() % uniqueValues);
                }

                auto vecGpu = TSingleBuffer<ui32>::Create(TSingleMapping(0, vec.size()));
                auto decompressedGpu = TCudaBuffer<ui32, TSingleMapping>::CopyMapping(vecGpu);
                vecGpu.Write(vec);

                auto compressedMapping = CompressedSize<TStorageType>(vecGpu, uniqueValues);
                auto compressedGpu = TCudaBuffer<TStorageType, TSingleMapping, Type>::Create(compressedMapping);

                for (ui32 i = 0; i < tries; ++i) {
                    {
                        auto compressGuard = profiler.Profile(TStringBuilder() << "Compress for " << uniqueValues
                                                                               << " unique values with storage type size "
                                                                               << sizeof(TStorageType) * 8);
                        Compress(vecGpu, compressedGpu, uniqueValues);
                    }
                    {
                        auto decompressGuard = profiler.Profile(TStringBuilder() << "Decompress for " << uniqueValues
                                                                                 << " unique values with storage type size "
                                                                                 << sizeof(TStorageType) * 8);
                        Decompress(compressedGpu, decompressedGpu, uniqueValues);
                    }
                }
            }
        }
        StopCudaManager();
    }

    SIMPLE_UNIT_TEST(TestCompressAndDecompressPerformanceui64) {
        BenchmarkCompress<ui64>();
    }

    SIMPLE_UNIT_TEST(TestCompressAndDecompressPerformanceui64FromHost) {
        BenchmarkCompress<ui64, NCudaLib::CudaHost>();
    }

    SIMPLE_UNIT_TEST(TestCompressAndDecompressPerformanceui32) {
        BenchmarkCompress<ui32>();
    }
}
