/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2014 - 2024 Intel Corporation */

#ifndef IPU_TRACE_H
#define IPU_TRACE_H
#include <linux/debugfs.h>

/* Trace unit register offsets */
#define TRACE_REG_TUN_DDR_ENABLE        0x000
#define TRACE_REG_TUN_NPK_ENABLE	0x004
#define TRACE_REG_TUN_DDR_INFO_VAL	0x008
#define TRACE_REG_TUN_NPK_ADDR		0x00C
#define TRACE_REG_TUN_DRAM_BASE_ADDR	0x010
#define TRACE_REG_TUN_DRAM_END_ADDR	0x014
#define TRACE_REG_TUN_LOCAL_TIMER0	0x018
#define TRACE_REG_TUN_LOCAL_TIMER1	0x01C
#define TRACE_REG_TUN_WR_PTR		0x020
#define TRACE_REG_TUN_RD_PTR		0x024

/*
 * Following registers are left out on purpose:
 * TUN_LOCAL_TIMER0, TUN_LOCAL_TIMER1, TUN_DRAM_BASE_ADDR
 * TUN_DRAM_END_ADDR, TUN_WR_PTR, TUN_RD_PTR
 */

/* Trace monitor register offsets */
#define TRACE_REG_TM_TRACE_ADDR_A		0x0900
#define TRACE_REG_TM_TRACE_ADDR_B		0x0904
#define TRACE_REG_TM_TRACE_ADDR_C		0x0908
#define TRACE_REG_TM_TRACE_ADDR_D		0x090c
#define TRACE_REG_TM_TRACE_ENABLE_NPK		0x0910
#define TRACE_REG_TM_TRACE_ENABLE_DDR		0x0914
#define TRACE_REG_TM_TRACE_PER_PC		0x0918
#define TRACE_REG_TM_TRACE_PER_BRANCH		0x091c
#define TRACE_REG_TM_TRACE_HEADER		0x0920
#define TRACE_REG_TM_TRACE_CFG			0x0924
#define TRACE_REG_TM_TRACE_LOST_PACKETS		0x0928
#define TRACE_REG_TM_TRACE_LP_CLEAR		0x092c
#define TRACE_REG_TM_TRACE_LMRUN_MASK		0x0930
#define TRACE_REG_TM_TRACE_LMRUN_PC_LOW		0x0934
#define TRACE_REG_TM_TRACE_LMRUN_PC_HIGH	0x0938
#define TRACE_REG_TM_TRACE_MMIO_SEL		0x093c
#define TRACE_REG_TM_TRACE_MMIO_WP0_LOW		0x0940
#define TRACE_REG_TM_TRACE_MMIO_WP1_LOW		0x0944
#define TRACE_REG_TM_TRACE_MMIO_WP2_LOW		0x0948
#define TRACE_REG_TM_TRACE_MMIO_WP3_LOW		0x094c
#define TRACE_REG_TM_TRACE_MMIO_WP0_HIGH	0x0950
#define TRACE_REG_TM_TRACE_MMIO_WP1_HIGH	0x0954
#define TRACE_REG_TM_TRACE_MMIO_WP2_HIGH	0x0958
#define TRACE_REG_TM_TRACE_MMIO_WP3_HIGH	0x095c
#define TRACE_REG_TM_FWTRACE_FIRST		0x0A00
#define TRACE_REG_TM_FWTRACE_MIDDLE		0x0A04
#define TRACE_REG_TM_FWTRACE_LAST		0x0A08

/*
 * Following exists only in (I)SP address space:
 * TM_FWTRACE_FIRST, TM_FWTRACE_MIDDLE, TM_FWTRACE_LAST
 */

#define TRACE_REG_GPC_RESET			0x000
#define TRACE_REG_GPC_OVERALL_ENABLE		0x004
#define TRACE_REG_GPC_TRACE_HEADER		0x008
#define TRACE_REG_GPC_TRACE_ADDRESS		0x00C
#define TRACE_REG_GPC_TRACE_NPK_EN		0x010
#define TRACE_REG_GPC_TRACE_DDR_EN		0x014
#define TRACE_REG_GPC_TRACE_LPKT_CLEAR		0x018
#define TRACE_REG_GPC_TRACE_LPKT		0x01C

#define TRACE_REG_GPC_ENABLE_ID0		0x020
#define TRACE_REG_GPC_ENABLE_ID1		0x024
#define TRACE_REG_GPC_ENABLE_ID2		0x028
#define TRACE_REG_GPC_ENABLE_ID3		0x02c

#define TRACE_REG_GPC_VALUE_ID0			0x030
#define TRACE_REG_GPC_VALUE_ID1			0x034
#define TRACE_REG_GPC_VALUE_ID2			0x038
#define TRACE_REG_GPC_VALUE_ID3			0x03c

#define TRACE_REG_GPC_CNT_INPUT_SELECT_ID0	0x040
#define TRACE_REG_GPC_CNT_INPUT_SELECT_ID1	0x044
#define TRACE_REG_GPC_CNT_INPUT_SELECT_ID2	0x048
#define TRACE_REG_GPC_CNT_INPUT_SELECT_ID3	0x04c

#define TRACE_REG_GPC_CNT_START_SELECT_ID0	0x050
#define TRACE_REG_GPC_CNT_START_SELECT_ID1	0x054
#define TRACE_REG_GPC_CNT_START_SELECT_ID2	0x058
#define TRACE_REG_GPC_CNT_START_SELECT_ID3	0x05c

#define TRACE_REG_GPC_CNT_STOP_SELECT_ID0	0x060
#define TRACE_REG_GPC_CNT_STOP_SELECT_ID1	0x064
#define TRACE_REG_GPC_CNT_STOP_SELECT_ID2	0x068
#define TRACE_REG_GPC_CNT_STOP_SELECT_ID3	0x06c

#define TRACE_REG_GPC_CNT_MSG_SELECT_ID0	0x070
#define TRACE_REG_GPC_CNT_MSG_SELECT_ID1	0x074
#define TRACE_REG_GPC_CNT_MSG_SELECT_ID2	0x078
#define TRACE_REG_GPC_CNT_MSG_SELECT_ID3	0x07c

#define TRACE_REG_GPC_CNT_MSG_PLOAD_SELECT_ID0	0x080
#define TRACE_REG_GPC_CNT_MSG_PLOAD_SELECT_ID1	0x084
#define TRACE_REG_GPC_CNT_MSG_PLOAD_SELECT_ID2	0x088
#define TRACE_REG_GPC_CNT_MSG_PLOAD_SELECT_ID3	0x08c

#define TRACE_REG_GPC_IRQ_TRIGGER_VALUE_ID0	0x090
#define TRACE_REG_GPC_IRQ_TRIGGER_VALUE_ID1	0x094
#define TRACE_REG_GPC_IRQ_TRIGGER_VALUE_ID2	0x098
#define TRACE_REG_GPC_IRQ_TRIGGER_VALUE_ID3	0x09c

#define TRACE_REG_GPC_IRQ_TIMER_SELECT_ID0	0x0a0
#define TRACE_REG_GPC_IRQ_TIMER_SELECT_ID1	0x0a4
#define TRACE_REG_GPC_IRQ_TIMER_SELECT_ID2	0x0a8
#define TRACE_REG_GPC_IRQ_TIMER_SELECT_ID3	0x0ac

#define TRACE_REG_GPC_IRQ_ENABLE_ID0		0x0b0
#define TRACE_REG_GPC_IRQ_ENABLE_ID1		0x0b4
#define TRACE_REG_GPC_IRQ_ENABLE_ID2		0x0b8
#define TRACE_REG_GPC_IRQ_ENABLE_ID3		0x0bc

struct ipu_trace;
struct ipu_subsystem_trace_config;

enum ipu_trace_block_type {
	IPU_TRACE_BLOCK_TUN = 0,	/* Trace unit */
	IPU_TRACE_BLOCK_TM,	/* Trace monitor */
	IPU_TRACE_BLOCK_GPC,	/* General purpose control */
	IPU_TRACE_CSI2,	/* CSI2 legacy receiver */
	IPU_TRACE_CSI2_3PH,	/* CSI2 combo receiver */
	IPU_TRACE_SIG2CIOS,
	IPU_TRACE_TIMER_RST,	/* Trace reset control timer */
	IPU_TRACE_BLOCK_END	/* End of list */
};

struct ipu_trace_block {
	u32 offset;	/* Offset to block inside subsystem */
	enum ipu_trace_block_type type;
};

int ipu_trace_add(struct ipu_device *isp);
int ipu_trace_debugfs_add(struct ipu_device *isp, struct dentry *dir);
void ipu_trace_release(struct ipu_device *isp);
int ipu_trace_init(struct ipu_device *isp, void __iomem *base,
		   struct device *dev, struct ipu_trace_block *blocks);
void ipu_trace_restore(struct device *dev);
void ipu_trace_uninit(struct device *dev);
void ipu_trace_stop(struct device *dev);
int ipu_trace_buffer_dma_handle(struct device *dev, dma_addr_t *dma_handle);
#endif
