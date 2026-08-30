/**
 * @file add_custom.cpp
 *
 * Copyright (C) 2024. Huawei Technologies Co., Ltd. All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 */
#include "kernel_operator.h"
//定义常量
constexpr int32_t TOTAL_LENGTH = 8 * 2048;   //定义总数据长度为 8 * 2048
constexpr int32_t USE_CORE_NUM = 8;        //定义使用 AI Core 数为 8
constexpr int32_t BLOCK_LENGTH = TOTAL_LENGTH / USE_CORE_NUM; //定义每个 AI的数据长度为总的数据长度为总数据长度除以 AI Core 数  
constexpr int32_t TILE_NUM = 8;                                       //定义每个 AI Core 处理的轮数为 8
constexpr int32_t BUFFER_NUM = 2;                                     //定义每个队列的缓冲区数为 2
constexpr int32_t TILE_LENGTH = BLOCK_LENGTH / TILE_NUM / BUFFER_NUM; //定义每个 AI Core 处理的轮数为 8，每个轮处理 2 个缓冲区

class KernelAdd {
public:
    __aicore__ inline KernelAdd() {}
    __aicore__ inline void Init(GM_ADDR x, GM_ADDR y, GM_ADDR z)
    {
        xGm.SetGlobalBuffer((__gm__ half *)x + BLOCK_LENGTH * AscendC::GetBlockIdx(), BLOCK_LENGTH);
        yGm.SetGlobalBuffer((__gm__ half *)y + BLOCK_LENGTH * AscendC::GetBlockIdx(), BLOCK_LENGTH);
        zGm.SetGlobalBuffer((__gm__ half *)z + BLOCK_LENGTH * AscendC::GetBlockIdx(), BLOCK_LENGTH);
        pipe.InitBuffer(inQueueX, BUFFER_NUM, TILE_LENGTH * sizeof(half));
        pipe.InitBuffer(inQueueY, BUFFER_NUM, TILE_LENGTH * sizeof(half));
        pipe.InitBuffer(outQueueZ, BUFFER_NUM, TILE_LENGTH * sizeof(half));
    }
    /**
     * @brief 主循环：分轮处理数据。
     * 每个 AI Core 会执行本函数，处理自己负责的数据段。
     * 主循环共分 TILE_NUM 轮，每轮处理 TILE_LENGTH 个元素。
     * 每轮分 2 个阶段：CopyIn（取数）-> Compute（计算）-> CopyOut（存回）。
     * 双缓冲机制：每次取数时，申请 2 个缓冲区，计算完成后，立即存回。
     * 这样，计算和取数可以并行执行，硬件流水线效率高。
     */
    __aicore__ inline void Process()
    {
        int32_t loopCount = TILE_NUM * BUFFER_NUM;
        for (int32_t i = 0; i < loopCount; i++) {
            CopyIn(i);
            Compute(i);
            CopyOut(i);
        }
    }

    /**
     * @brief 取数阶段：把第 progress 轮的数据从全局内存拷贝到本地队列。
     * AllocTensor 从队列申请空闲缓冲槽，DataCopy 完成 GM->LM 搬运，
     * EnQue 将填好的缓冲槽入队交给下一阶段（Compute）。
     */
    __aicore__ inline void CopyIn(int32_t progress)
    {
        AscendC::LocalTensor<half> xLocal = inQueueX.AllocTensor<half>();
        AscendC::LocalTensor<half> yLocal = inQueueY.AllocTensor<half>();
        AscendC::DataCopy(xLocal, xGm[progress * TILE_LENGTH], TILE_LENGTH);
        AscendC::DataCopy(yLocal, yGm[progress * TILE_LENGTH], TILE_LENGTH);
        inQueueX.EnQue(xLocal);
        inQueueY.EnQue(yLocal);
    }
    /**
     * @brief 计算阶段：从队列取数，向量计算，结果入队。
     * DeQue 从队列取出已填好数据的缓冲槽，Add 完成向量计算，
     * AllocTensor 从队列申请空闲缓冲槽，EnQue 将计算结果入队交给下一阶段（CopyOut）。
     */
    __aicore__ inline void Compute(int32_t progress)
    {
        AscendC::LocalTensor<half> xLocal = inQueueX.DeQue<half>();
        AscendC::LocalTensor<half> yLocal = inQueueY.DeQue<half>();
        AscendC::LocalTensor<half> zLocal = outQueueZ.AllocTensor<half>();
        AscendC::Add(zLocal, xLocal, yLocal, TILE_LENGTH);
        outQueueZ.EnQue<half>(zLocal);
        inQueueX.FreeTensor(xLocal);
        inQueueY.FreeTensor(yLocal);
    }
    /**
     * @brief 存回阶段：把第 progress 轮的数据从本地队列拷贝到全局内存。
     * DataCopy 完成 LM->GM 搬运，FreeTensor 释放队列缓冲槽。
     */
    __aicore__ inline void CopyOut(int32_t progress)
    {
        AscendC::LocalTensor<half> zLocal = outQueueZ.DeQue<half>();
        AscendC::DataCopy(zGm[progress * TILE_LENGTH], zLocal, TILE_LENGTH);
        outQueueZ.FreeTensor(zLocal);
    }

private:
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::QuePosition::VECIN, BUFFER_NUM> inQueueX, inQueueY;
    AscendC::TQue<AscendC::QuePosition::VECOUT, BUFFER_NUM> outQueueZ;
    AscendC::GlobalTensor<half> xGm;
    AscendC::GlobalTensor<half> yGm;
    AscendC::GlobalTensor<half> zGm;
};

extern "C" __global__ __aicore__ void add_custom(GM_ADDR x, GM_ADDR y, GM_ADDR z)
{
    KernelAdd op;
    op.Init(x, y, z);
    op.Process();
}

#ifndef ASCENDC_CPU_DEBUG
void add_custom_do(uint32_t blockDim, void *stream, uint8_t *x, uint8_t *y, uint8_t *z)
{
    add_custom<<<blockDim, nullptr, stream>>>(x, y, z);
}
#endif
