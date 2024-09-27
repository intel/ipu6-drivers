/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2020 - 2024 Intel Corporation */

#ifndef IPU6EP_PLATFORM_RESOURCES_H
#define IPU6EP_PLATFORM_RESOURCES_H

#include <linux/kernel.h>
#include <linux/device.h>

enum {
	IPU6EP_FW_PSYS_SP0_ID = 0,
	IPU6EP_FW_PSYS_VP0_ID,
	IPU6EP_FW_PSYS_PSA_ACC_BNLM_ID,
	IPU6EP_FW_PSYS_PSA_ACC_DM_ID,
	IPU6EP_FW_PSYS_PSA_ACC_ACM_ID,
	IPU6EP_FW_PSYS_PSA_ACC_GTC_YUV1_ID,
	IPU6EP_FW_PSYS_BB_ACC_OFS_PIN_MAIN_ID,
	IPU6EP_FW_PSYS_BB_ACC_OFS_PIN_DISPLAY_ID,
	IPU6EP_FW_PSYS_BB_ACC_OFS_PIN_PP_ID,
	IPU6EP_FW_PSYS_PSA_ACC_GAMMASTAR_ID,
	IPU6EP_FW_PSYS_PSA_ACC_GLTM_ID,
	IPU6EP_FW_PSYS_PSA_ACC_XNR_ID,
	IPU6EP_FW_PSYS_PSA_VCSC_ID,	/* VCSC */
	IPU6EP_FW_PSYS_ISA_ICA_ID,
	IPU6EP_FW_PSYS_ISA_LSC_ID,
	IPU6EP_FW_PSYS_ISA_DPC_ID,
	IPU6EP_FW_PSYS_ISA_SIS_A_ID,
	IPU6EP_FW_PSYS_ISA_SIS_B_ID,
	IPU6EP_FW_PSYS_ISA_B2B_ID,
	IPU6EP_FW_PSYS_ISA_B2R_R2I_SIE_ID,
	IPU6EP_FW_PSYS_ISA_R2I_DS_A_ID,
	IPU6EP_FW_PSYS_ISA_AWB_ID,
	IPU6EP_FW_PSYS_ISA_AE_ID,
	IPU6EP_FW_PSYS_ISA_AF_ID,
	IPU6EP_FW_PSYS_ISA_X2B_MD_ID,
	IPU6EP_FW_PSYS_ISA_X2B_SVE_RGBIR_ID,
	IPU6EP_FW_PSYS_ISA_PAF_ID,
	IPU6EP_FW_PSYS_BB_ACC_GDC0_ID,
	IPU6EP_FW_PSYS_BB_ACC_TNR_ID,
	IPU6EP_FW_PSYS_N_CELL_ID
};
#endif /* IPU6EP_PLATFORM_RESOURCES_H */