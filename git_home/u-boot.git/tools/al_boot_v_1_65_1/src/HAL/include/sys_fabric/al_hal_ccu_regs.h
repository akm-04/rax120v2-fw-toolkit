/*******************************************************************************
Copyright (C) 2015 Annapurna Labs Ltd.

This file may be licensed under the terms of the Annapurna Labs Commercial
License Agreement.

Alternatively, this file can be distributed under the terms of the GNU General
Public License V2 as published by the Free Software Foundation and can be
found at http://www.gnu.org/licenses/gpl-2.0.html

Alternatively, redistribution and use in source and binary forms, with or
without modification, are permitted provided that the following conditions are
met:

    *     Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

    *     Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in
the documentation and/or other materials provided with the
distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/**
 * @defgroup groupccu abstraction layer
 *  @{
 * @file   al_hal_ccu_regs.h
 *
 * @brief Header file for the CCU HAL driver
 */

#ifndef __AL_HAL_CCU_REGS_H__
#define __AL_HAL_CCU_REGS_H__

#include "al_hal_common.h"

/* *INDENT-OFF* */
#ifdef __cplusplus
extern "C" {
#endif
/* *INDENT-ON* */


#define AL_CCU_PMU_REG_COUNT 4

#define AL_CCU_PMU_SRC_OFFSET 5

struct al_ccu_pmu_regs {
	uint32_t sel;                            /*[0x0]*/
	uint32_t counter;                        /*[0x4]*/
	uint32_t ctrl;                           /*[0x8]*/
	uint32_t overflow;                       /*[0xB]*/
	uint32_t resv_0[1020];                   /*[0x10]*/
};

struct al_ccu_regs {
	uint32_t rsrvd_0[64];                                   /* [0x0]  */
	uint32_t pmcr;                                          /* [0x100]*/
	uint32_t resvd_1[10175];                                /* [0x104]*/
	struct al_ccu_pmu_regs pmu_cntrs[AL_CCU_PMU_REG_COUNT]; /* [0xA000]*/
};

/* *INDENT-OFF* */
#ifdef __cplusplus
}
#endif
/* *INDENT-ON* */
/** @} end of DDR group */

#endif
