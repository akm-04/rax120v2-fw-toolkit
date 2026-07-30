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
 * @addtogroup groupddr
 *
 *  @{
 * @file   al_hal_ddr.c
 *
 * @brief  DDR controller & PHY HAL driver
 *
 */

#include "al_hal_ddr.h"
#include "al_hal_ddr_ctrl_regs.h"
#include "al_hal_ddr_phy_regs.h"
#include "al_hal_nb_regs.h"
#include "al_hal_ddr_utils.h"
#include "al_hal_ddr_jedec_ddr4.h"

#define BIST_TIMEOUT	1000	/* 1ms */

#define BIST_LFSR_RESET_VAL	0x1234ABCD

/* Wait for PHY BIST to be done */
static int al_ddr_phy_wait_for_bist_done(
	struct al_ddr_cfg		*ddr_cfg,
	struct al_ddr_bist_err_status	*bist_err_status);

static void read_bist_err_status(
	struct al_ddr_cfg		*ddr_cfg,
	struct al_ddr_bist_err_status	*bist_err_status);

static int al_ddr_address_map_get(
	struct al_ddr_cfg	*ddr_cfg,
	struct al_ddr_addrmap	*addrmap);

/*******************************************************************************
 ******************************************************************************/
void al_ddr_phy_datx_bist_pre(struct al_ddr_cfg *ddr_cfg)
{
	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)ddr_cfg->ddr_phy_regs_base;

	al_ddr_phy_vt_calc_disable(ddr_cfg);
	al_ddr_ctrl_stop(ddr_cfg);

	/* Disabling refreshes from PHY as it cause BIST to get stuck */
	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.DTCR,
			DWC_DDR_PHY_REGS_DTCR_RFSHDT_MASK,
			(0x0 << DWC_DDR_PHY_REGS_DTCR_RFSHDT_SHIFT));
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.dtcr[0],
			ALPINE_V2_DWC_DDR_PHY_REGS_DTCR0_RFSHDT_MASK,
			(0x0 << ALPINE_V2_DWC_DDR_PHY_REGS_DTCR0_RFSHDT_SHIFT));
	}
}
/*******************************************************************************
 ******************************************************************************/
void al_ddr_phy_datx_bist_post(struct al_ddr_cfg *ddr_cfg)
{
	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)ddr_cfg->ddr_phy_regs_base;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		/* PGCR3 - after BIST re-apply power down of unused DQs */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.PGCR[3],
			DWC_DDR_PHY_REGS_PGCR3_GATEDXCTLCLK |
			DWC_DDR_PHY_REGS_PGCR3_GATEDXDDRCLK |
			DWC_DDR_PHY_REGS_PGCR3_GATEDXRDCLK,
			DWC_DDR_PHY_REGS_PGCR3_GATEDXCTLCLK |
			DWC_DDR_PHY_REGS_PGCR3_GATEDXDDRCLK |
			DWC_DDR_PHY_REGS_PGCR3_GATEDXRDCLK);

		/* re-enable refreshes from PHY */
		al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.DTCR,
			DWC_DDR_PHY_REGS_DTCR_RFSHDT_MASK,
			(0x9 << DWC_DDR_PHY_REGS_DTCR_RFSHDT_SHIFT));

	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		/* PGCR3 - after BIST re-apply power down of unused DQs */
		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.pgcr[3],
			ALPINE_V2_DWC_DDR_PHY_REGS_PGCR3_GATEDXCTLCLK |
			ALPINE_V2_DWC_DDR_PHY_REGS_PGCR3_GATEDXDDRCLK |
			ALPINE_V2_DWC_DDR_PHY_REGS_PGCR3_GATEDXRDCLK,
			ALPINE_V2_DWC_DDR_PHY_REGS_PGCR3_GATEDXCTLCLK |
			ALPINE_V2_DWC_DDR_PHY_REGS_PGCR3_GATEDXDDRCLK |
			ALPINE_V2_DWC_DDR_PHY_REGS_PGCR3_GATEDXRDCLK);

		/* re-enable refreshes from PHY */
		al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.dtcr[0],
			ALPINE_V2_DWC_DDR_PHY_REGS_DTCR0_RFSHDT_MASK,
			(0x9 << ALPINE_V2_DWC_DDR_PHY_REGS_DTCR0_RFSHDT_SHIFT));
	}

	al_ddr_phy_vt_calc_enable(ddr_cfg);
	al_ddr_ctrl_resume(ddr_cfg);
}
/*******************************************************************************
 ******************************************************************************/
/* TODO : add Bank Group support */
int al_ddr_phy_datx_bist(
	struct al_ddr_cfg		*ddr_cfg,
	struct al_ddr_bist_params	*params,
	struct al_ddr_bist_err_status	*bist_err_status)
{
	int i;
	int err = 0;

	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)ddr_cfg->ddr_phy_regs_base;

	uint32_t mode;
	uint32_t pattern;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {

		mode =
			(params->mode == AL_DDR_BIST_MODE_LOOPBACK) ?
			DWC_DDR_PHY_REGS_BISTRR_BMODE_LOOPBACK :
			DWC_DDR_PHY_REGS_BISTRR_BMODE_DRAM;

		pattern =
			(params->pat == AL_DDR_BIST_PATTERN_WALK_0) ?
			DWC_DDR_PHY_REGS_BISTRR_BDPAT_WALKING_0 :
			(params->pat == AL_DDR_BIST_PATTERN_WALK_1) ?
			DWC_DDR_PHY_REGS_BISTRR_BDPAT_WALKING_1 :
			(params->pat == AL_DDR_BIST_PATTERN_LFSR) ?
			DWC_DDR_PHY_REGS_BISTRR_BDPAT_LFSR :
			DWC_DDR_PHY_REGS_BISTRR_BDPAT_USER;

		/**
		* Init BIST mode of operation
		*/

		/* BISTUDPR */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTUDPR,
			DWC_DDR_PHY_REGS_BISTUDPR_BUDP0_MASK |
			DWC_DDR_PHY_REGS_BISTUDPR_BUDP1_MASK,
			DWC_DDR_PHY_REGS_BISTUDPR_BUDP0(params->user_pat_even) |
			DWC_DDR_PHY_REGS_BISTUDPR_BUDP1(params->user_pat_odd));

		/* BISTWCR */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTWCR,
			DWC_DDR_PHY_REGS_BISTWCR_BWCNT_MASK,
			DWC_DDR_PHY_REGS_BISTWCR_BWCNT(params->wc));

		/* BISTAR0 */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTAR[0],
			DWC_DDR_PHY_REGS_BISTAR0_BCOL_MASK |
			DWC_DDR_PHY_REGS_BISTAR0_BROW_MASK |
			DWC_DDR_PHY_REGS_BISTAR0_BBANK_MASK,
			DWC_DDR_PHY_REGS_BISTAR0_BCOL(params->col_min) |
			DWC_DDR_PHY_REGS_BISTAR0_BROW(params->row_min) |
			DWC_DDR_PHY_REGS_BISTAR0_BBANK(params->bank_min));

		/* BISTAR1 */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTAR[1],
			DWC_DDR_PHY_REGS_BISTAR1_BRANK_MASK |
			DWC_DDR_PHY_REGS_BISTAR1_BMRANK_MASK |
			DWC_DDR_PHY_REGS_BISTAR1_BAINC_MASK,
			DWC_DDR_PHY_REGS_BISTAR1_BRANK(params->rank_min) |
			DWC_DDR_PHY_REGS_BISTAR1_BMRANK(params->rank_max) |
			DWC_DDR_PHY_REGS_BISTAR1_BAINC(params->inc));

		/* BISTAR2 */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTAR[2],
			DWC_DDR_PHY_REGS_BISTAR2_BMCOL_MASK |
			DWC_DDR_PHY_REGS_BISTAR2_BMROW_MASK |
			DWC_DDR_PHY_REGS_BISTAR2_BMBANK_MASK,
			DWC_DDR_PHY_REGS_BISTAR2_BMCOL(params->col_max) |
			DWC_DDR_PHY_REGS_BISTAR2_BMROW(params->row_max) |
			DWC_DDR_PHY_REGS_BISTAR2_BMBANK(params->bank_max));

		/* Run DATX8 BIST */
		for (i = 0; i < AL_DDR_PHY_NUM_BYTE_LANES; i++) {
			if (!params->active_byte_lanes[i])
				continue;

			/* Reset status */
			_al_reg_write32_masked(
				&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTRR,
				DWC_DDR_PHY_REGS_BISTRR_BINST_MASK,
				DWC_DDR_PHY_REGS_BISTRR_BINST_RESET);

			/* Run BIST */
			_al_reg_write32_masked(
				&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTRR,
				DWC_DDR_PHY_REGS_BISTRR_BINST_MASK |
				DWC_DDR_PHY_REGS_BISTRR_BMODE_MASK |
				DWC_DDR_PHY_REGS_BISTRR_BDXEN |
				DWC_DDR_PHY_REGS_BISTRR_BACEN |
				DWC_DDR_PHY_REGS_BISTRR_BDPAT_MASK |
				DWC_DDR_PHY_REGS_BISTRR_BDXSEL_MASK,
				DWC_DDR_PHY_REGS_BISTRR_BINST_RUN |
				mode |
				DWC_DDR_PHY_REGS_BISTRR_BDXEN |
				pattern |
				DWC_DDR_PHY_REGS_BISTRR_BDXSEL(i));

			al_data_memory_barrier();

			/* Read BISTGSR for BIST done */
			err = al_ddr_phy_wait_for_bist_done(ddr_cfg, bist_err_status);
			if (err) {
				if (bist_err_status)
					bist_err_status->lane_err = i;
				al_dbg(
					"%s:%d: al_ddr_phy_wait_for_bist_done failed "
					"(byte lane %d)!\n",
					__func__,
					__LINE__,
					i);
				break;
			}
		}

		/* stop BIST */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTRR,
			DWC_DDR_PHY_REGS_BISTRR_BINST_MASK,
			DWC_DDR_PHY_REGS_BISTRR_BINST_STOP);

		al_data_memory_barrier();

		/* Waiting BIST to stop */
		err = al_ddr_phy_wait_for_bist_done(ddr_cfg, bist_err_status);
		if (err)
			al_dbg(
				"%s:%d: al_ddr_phy_wait_for_bist_done failed\n",
				__func__,
				__LINE__);

		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTRR,
			DWC_DDR_PHY_REGS_BISTRR_BINST_MASK,
			DWC_DDR_PHY_REGS_BISTRR_BINST_NOP);
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {

		mode =
			(params->mode == AL_DDR_BIST_MODE_LOOPBACK) ?
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BMODE_LOOPBACK :
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BMODE_DRAM;

		pattern =
			(params->pat == AL_DDR_BIST_PATTERN_WALK_0) ?
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDPAT_WALKING_0 :
			(params->pat == AL_DDR_BIST_PATTERN_WALK_1) ?
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDPAT_WALKING_1 :
			(params->pat == AL_DDR_BIST_PATTERN_LFSR) ?
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDPAT_LFSR :
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDPAT_USER;

		/* Disabling refreshes from PHY as it cause BIST to get stuck */
		al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.dtcr[0],
			ALPINE_V2_DWC_DDR_PHY_REGS_DTCR0_RFSHDT_MASK,
			(0x0 << ALPINE_V2_DWC_DDR_PHY_REGS_DTCR0_RFSHDT_SHIFT));

		/**
		* Init BIST mode of operation
		*/

		/* BISTUDPR */
		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistudpr,
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTUDPR_BUDP0_MASK |
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTUDPR_BUDP1_MASK,
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTUDPR_BUDP0(
				params->user_pat_even) |
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTUDPR_BUDP1(
				params->user_pat_odd));

		/* BISTWCR */
		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistwcr,
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTWCR_BWCNT_MASK,
			(params->wc <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTWCR_BWCNT_SHIFT));

		/* BISTAR0 */
		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistar[0],
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR0_BCOL_MASK |
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR0_BBANK_MASK,
			(params->col_min <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR0_BCOL_SHIFT) |
			(params->bank_min <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR0_BBANK_SHIFT));

		/* BISTAR1 */
		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistar[1],
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR1_BRANK_MASK |
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR1_BMRANK_MASK |
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR1_BAINC_MASK,
			(params->rank_min <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR1_BRANK_SHIFT) |
			(params->rank_max <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR1_BMRANK_SHIFT) |
			(params->inc <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR1_BAINC_SHIFT));

		/* BISTAR2 */
		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistar[2],
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR2_BMCOL_MASK |
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR2_BMBANK_MASK,
			(params->col_max <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR2_BMCOL_SHIFT) |
			(params->bank_max <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR2_BMBANK_SHIFT));

		/* BISTAR3 */
		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistar[3],
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR3_BROW_MASK,
			(params->row_min <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR3_BROW_SHIFT));
		/* BISTAR4 */
		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistar[3],
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR4_BMROW_MASK,
			(params->row_max <<
			 ALPINE_V2_DWC_DDR_PHY_REGS_BISTAR4_BMROW_SHIFT));

		/* Run DATX8 BIST */
		for (i = 0; i < AL_DDR_PHY_NUM_BYTE_LANES; i++) {
			if (params->all_lanes_active && (i != 0))
				break;
			if (!params->active_byte_lanes[i] && !params->all_lanes_active)
				continue;

			/* Reinitialize LFSR seed each iteration */
			al_reg_write32(
				&phy_regs->alpine_v2.dwc_ddrphy_pub.bistlsr,
				BIST_LFSR_RESET_VAL);

			/* Reset status */
			_al_reg_write32_masked(
				&phy_regs->alpine_v2.dwc_ddrphy_pub.bistrr,
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BINST_MASK,
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BINST_RESET);

			/* Run BIST */
			_al_reg_write32_masked(
				&phy_regs->alpine_v2.dwc_ddrphy_pub.bistrr,
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BINST_MASK |
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BMODE |
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDXEN |
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BACEN |
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDPAT_MASK |
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDXSEL_MASK,
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BINST_RUN |
				mode |
				ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDXEN |
				pattern |
				(params->all_lanes_active ?
					ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDXSEL_MASK :
					(i << ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BDXSEL_SHIFT)));

			al_data_memory_barrier();

			/* Read BISTGSR for BIST done */
			err = al_ddr_phy_wait_for_bist_done(ddr_cfg, bist_err_status);
			if (err) {
				if (bist_err_status)
					bist_err_status->lane_err = i;
				al_dbg(
					"%s:%d: al_ddr_phy_wait_for_bist_done failed "
					"(byte lane %d)!\n",
					__func__,
					__LINE__,
					i);
				break;
			}
		}

		/* stop BIST */
		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistrr,
			DWC_DDR_PHY_REGS_BISTRR_BINST_MASK,
			DWC_DDR_PHY_REGS_BISTRR_BINST_STOP);

		al_data_memory_barrier();

		/* Waiting BIST to stop */
		err = al_ddr_phy_wait_for_bist_done(ddr_cfg, bist_err_status);
		if (err)
			al_dbg(
				"%s:%d: al_ddr_phy_wait_for_bist_done failed\n",
				__func__,
				__LINE__);

		_al_reg_write32_masked(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistrr,
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BINST_MASK,
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTRR_BINST_NOP);
	}

	return err;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_phy_ac_bist(
	struct al_ddr_cfg		*ddr_cfg,
	struct al_ddr_bist_err_status	*bist_err_status,
	enum al_ddr_bist_pat		pat)
{
	int err;

	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)ddr_cfg->ddr_phy_regs_base;

	uint32_t pattern;

	/* Run AC BIST */
	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {

		pattern =
			(pat == AL_DDR_BIST_PATTERN_WALK_0) ?
			DWC_DDR_PHY_REGS_BISTRR_BDPAT_WALKING_0 :
			(pat == AL_DDR_BIST_PATTERN_WALK_1) ?
			DWC_DDR_PHY_REGS_BISTRR_BDPAT_WALKING_1 :
			DWC_DDR_PHY_REGS_BISTRR_BDPAT_LFSR;

		/* Reset status */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTRR,
			DWC_DDR_PHY_REGS_BISTRR_BINST_MASK,
			DWC_DDR_PHY_REGS_BISTRR_BINST_RESET);

		/* Power up I/O receivers */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.ACIOCR[0],
			DWC_DDR_PHY_REGS_ACIOCR0_ACPDR |
			DWC_DDR_PHY_REGS_ACIOCR0_CKPDR_MASK |
			DWC_DDR_PHY_REGS_ACIOCR0_RANKPDR_MASK |
			DWC_DDR_PHY_REGS_ACIOCR0_RSTPDR,
			0);

		/* Loopback before buffer in I/O */
		al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.PGCR[1],
			DWC_DDR_PHY_REGS_PGCR1_IOLB,
			DWC_DDR_PHY_REGS_PGCR1_IOLB);

		/* Run BIST */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTRR,
			DWC_DDR_PHY_REGS_BISTRR_BINST_MASK |
			DWC_DDR_PHY_REGS_BISTRR_BMODE_MASK |
			DWC_DDR_PHY_REGS_BISTRR_BDXEN |
			DWC_DDR_PHY_REGS_BISTRR_BACEN |
			DWC_DDR_PHY_REGS_BISTRR_BDPAT_MASK,
			DWC_DDR_PHY_REGS_BISTRR_BINST_RUN |
			DWC_DDR_PHY_REGS_BISTRR_BMODE_LOOPBACK |
			DWC_DDR_PHY_REGS_BISTRR_BACEN |
			pattern);

		al_data_memory_barrier();

		/* Read BISTGSR for BIST done */
		err = al_ddr_phy_wait_for_bist_done(ddr_cfg, bist_err_status);
		if (err) {
			al_err(
				"%s:%d: al_ddr_phy_wait_for_bist_done failed!\n",
				__func__,
				__LINE__);
			return err;
		}

		/* Power down I/O receivers */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.ACIOCR[0],
			DWC_DDR_PHY_REGS_ACIOCR0_ACPDR |
			DWC_DDR_PHY_REGS_ACIOCR0_CKPDR_MASK |
			DWC_DDR_PHY_REGS_ACIOCR0_RANKPDR_MASK |
			DWC_DDR_PHY_REGS_ACIOCR0_RSTPDR,
			DWC_DDR_PHY_REGS_ACIOCR0_ACPDR |
			DWC_DDR_PHY_REGS_ACIOCR0_CKPDR_MASK |
			DWC_DDR_PHY_REGS_ACIOCR0_RANKPDR_MASK |
			DWC_DDR_PHY_REGS_ACIOCR0_RSTPDR);

		/* stop BIST */
		_al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTRR,
			DWC_DDR_PHY_REGS_BISTRR_BINST_MASK,
			DWC_DDR_PHY_REGS_BISTRR_BINST_NOP);

	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		/* TODO */
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_power_mode_set(
	struct al_ddr_cfg	*ddr_cfg,
	enum al_ddr_power_mode	power_mode,
	al_bool			clk_disable,
	unsigned int		timer_x32)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	uint32_t mode =
		(power_mode == AL_DDR_POWERMODE_SELF_REFRESH) ?
		DWC_DDR_UMCTL2_REGS_PWRCTL_SELFREF_EN :
		(power_mode == AL_DDR_POWERMODE_POWER_DOWN) ?
		DWC_DDR_UMCTL2_REGS_PWRCTL_POWERDOWN_EN :
		(power_mode == AL_DDR_POWERMODE_MPSM) ?
		DWC_DDR_UMCTL2_REGS_PWRCTL_MPSM_EN :
		(power_mode == AL_DDR_POWERMODE_SELF_REFRESH_SW) ?
		DWC_DDR_UMCTL2_REGS_PWRCTL_SELFREF_SW :
		0;

	if (clk_disable)
		mode |= DWC_DDR_UMCTL2_REGS_PWRCTL_EN_DFI_DRAM_CLK_DISABLE;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		/*
		* Addressing RMN: 1037
		*
		* RMN description:
		* In the current logic, it is possible for DRAM Read data
		* and/or Write data to be active while/after one of the
		* following occurs:
		* Power Down Entry (PDE)
		* Self Refresh Entry (SRE)
		* This would violate the memory protocol DDR3 which require
		* "no data bursts are in progress" when the above
		* commands occur.
		* Software flow:
		* For violations related to PDE, issue can be avoided by
		* ensuring that timer_x32>1. For violations related to SRE,
		* ensure that AL_DDR_POWERMODE_SELF_REFRESH is set only after
		* all Read data has been returned on your application
		* interface and all write data has reached the DRAM.
		*/

		al_assert((power_mode == AL_DDR_POWERMODE_SELF_REFRESH) ||
			(power_mode == AL_DDR_POWERMODE_POWER_DOWN));

		if (timer_x32 <= 1) {
			al_err(
				"%s:%d: power mode timer must be greater than 1!\n",
				__func__,
				__LINE__);
			return -EIO;
		}

		al_reg_write32(CTRL_REG(pwrtmg),
			timer_x32 <<
			 DWC_DDR_UMCTL2_REGS_PWRTMG_POWERDOWN_TO_X32_SHIFT);

		al_reg_write32(CTRL_REG(pwrctl), mode);
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		if (power_mode == AL_DDR_POWERMODE_SELF_REFRESH)
			al_reg_write32(CTRL_REG(pwrtmg),
				timer_x32 <<
				DWC_DDR_UMCTL2_REGS_PWRTMG_SELFREF_TO_X32_SHIFT);
		else if (power_mode == AL_DDR_POWERMODE_POWER_DOWN)
			al_reg_write32(CTRL_REG(pwrtmg),
				timer_x32 <<
				DWC_DDR_UMCTL2_REGS_PWRTMG_POWERDOWN_TO_X32_SHIFT);

		al_reg_write32(CTRL_REG(pwrctl), mode);
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
enum al_ddr_operating_mode al_ddr_operating_mode_get(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;
	uint32_t reg_val;
	enum al_ddr_operating_mode operating_mode;

	reg_val = al_reg_read32(CTRL_REG(stat));
	reg_val &= DWC_DDR_UMCTL2_REGS_STAT_OPERATING_MODE_MASK;

	operating_mode =
		(reg_val == DWC_DDR_UMCTL2_REGS_STAT_OPERATING_MODE_INIT) ?
		AL_DDR_OPERATING_MODE_INIT :
		(reg_val == DWC_DDR_UMCTL2_REGS_STAT_OPERATING_MODE_NORMAL) ?
		AL_DDR_OPERATING_MODE_NORMAL :
		(reg_val == DWC_DDR_UMCTL2_REGS_STAT_OPERATING_MODE_POWER_DOWN) ?
		AL_DDR_OPERATING_MODE_POWER_DOWN :
		AL_DDR_OPERATING_MODE_SELF_REFRESH;

	return operating_mode;
}

/*******************************************************************************
 ******************************************************************************/
static void read_bist_err_status(
	struct al_ddr_cfg		*ddr_cfg,
	struct al_ddr_bist_err_status	*bist_err_status)
{
	uint32_t reg_val;
	int i;

	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)ddr_cfg->ddr_phy_regs_base;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		bist_err_status->word_err =
			(al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTWER) &
			DWC_DDR_PHY_REGS_BISTWER_DXWER_MASK) >>
			DWC_DDR_PHY_REGS_BISTWER_DXWER_SHIFT;
	
		reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTBER[2]);
		for (i = 0; i < AL_DDR_DQ_BITS; ++i) {
			bist_err_status->even_risg_err[i] =
				(reg_val &
				DWC_DDR_PHY_REGS_BISTBER2_DQBER0_RISG_DQ_I_MASK(i)) >>
				DWC_DDR_PHY_REGS_BISTBER2_DQBER0_RISG_DQ_I_SHIFT(i);

			bist_err_status->even_fall_err[i] =
				(reg_val &
				DWC_DDR_PHY_REGS_BISTBER2_DQBER0_FALL_DQ_I_MASK(i)) >>
				DWC_DDR_PHY_REGS_BISTBER2_DQBER0_FALL_DQ_I_SHIFT(i);
		}

		for (i = 0; i < AL_DDR_DQ_BITS; ++i) {
			reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTBER[3]);
			bist_err_status->odd_risg_err[i] =
				(reg_val &
				DWC_DDR_PHY_REGS_BISTBER3_DQBER1_RISG_DQ_I_MASK(i)) >>
				DWC_DDR_PHY_REGS_BISTBER3_DQBER1_RISG_DQ_I_SHIFT(i);

			bist_err_status->odd_fall_err[i] =
				(reg_val &
				DWC_DDR_PHY_REGS_BISTBER3_DQBER1_FALL_DQ_I_MASK(i)) >>
				DWC_DDR_PHY_REGS_BISTBER3_DQBER1_FALL_DQ_I_SHIFT(i);
		}

		bist_err_status->word_count =
			(al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTWCSR) &
			DWC_DDR_PHY_REGS_BISTWCSR_DXWCNT_MASK) >>
			DWC_DDR_PHY_REGS_BISTWCSR_DXWCNT_SHIFT;
	
		reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTWER);
		for (i = 0; i < AL_DDR_DQ_BITS; ++i) {
			bist_err_status->beat_1st_err[i] =
				reg_val & DWC_DDR_PHY_REGS_BISTFWR2_DQWEBS_1ST_DQ_I(i);

			bist_err_status->beat_2nd_err[i] =
				reg_val & DWC_DDR_PHY_REGS_BISTFWR2_DQWEBS_2ND_DQ_I(i);

			bist_err_status->beat_3rd_err[i] =
				reg_val & DWC_DDR_PHY_REGS_BISTFWR2_DQWEBS_3RD_DQ_I(i);

			bist_err_status->beat_4th_err[i] =
				reg_val & DWC_DDR_PHY_REGS_BISTFWR2_DQWEBS_4TH_DQ_I(i);
		}
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		/* TODO */
	}

	bist_err_status->timeout = AL_FALSE;
}

/*******************************************************************************
 ******************************************************************************/
static int al_ddr_phy_wait_for_bist_done(
	struct al_ddr_cfg		*ddr_cfg,
	struct al_ddr_bist_err_status	*bist_err_status)
{
	int err = 0;
	uint32_t reg_val;

	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)
		ddr_cfg->ddr_phy_regs_base;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		err = al_ddr_reg_poll32(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTGSR,
			DWC_DDR_PHY_REGS_BISTGSR_BDONE,
			DWC_DDR_PHY_REGS_BISTGSR_BDONE,
			BIST_TIMEOUT);
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		err = al_ddr_reg_poll32(
			&phy_regs->alpine_v2.dwc_ddrphy_pub.bistgsr,
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTGSR_BDONE,
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTGSR_BDONE,
			BIST_TIMEOUT);
	}

	if (err) {
		if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
			int err2 = 0;
			al_reg_write32_masked(&phy_regs->alpine_v1.dwc_ddrphy_pub.PIR,
					DWC_DDR_PHY_REGS_PIR_INIT |
					DWC_DDR_PHY_REGS_PIR_PHYRST,
					DWC_DDR_PHY_REGS_PIR_INIT |
					DWC_DDR_PHY_REGS_PIR_PHYRST);
			al_data_memory_barrier();
			err2 = al_ddr_reg_poll32(
				&phy_regs->alpine_v1.dwc_ddrphy_pub.PGSR[0],
				DWC_DDR_PHY_REGS_PGSR0_IDONE,
				DWC_DDR_PHY_REGS_PGSR0_IDONE,
				DEFAULT_TIMEOUT);
			if (err2)
				al_err("%s: al_ddr_reg_poll32 failed!\n", __func__);

			if (bist_err_status) {
				read_bist_err_status(ddr_cfg, bist_err_status);
				bist_err_status->timeout = AL_TRUE;

				/* Reinitialize LFSR seed */
				al_reg_write32(
					&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTLSR,
					BIST_LFSR_RESET_VAL);
			}

			al_dbg("%s: al_ddr_reg_poll32 failed!\n", __func__);
			al_dbg("%s: PHY bist error (BISTGSR = %08x)!\n",
				__func__, al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTGSR));
			al_dbg("%s: (BISTWER = %08x)!\n",
				__func__, al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTWER));
			al_dbg("%s: (BISTBER2 = %08x)!\n",
				__func__, al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTBER[2]));
			al_dbg("%s: (BISTBER3 = %08x)!\n",
				__func__, al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTBER[3]));
			al_dbg("%s: (BISTWCSR = %08x)!\n",
				__func__, al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTWCSR));
			al_dbg("%s: (BISTFWR2 = %08x)!\n",
				__func__, al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTFWR[2]));
			return err;
		} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
			int err2 = 0;
			al_reg_write32_masked(&phy_regs->alpine_v2.dwc_ddrphy_pub.pir,
					ALPINE_V2_DWC_DDR_PHY_REGS_PIR_INIT |
					ALPINE_V2_DWC_DDR_PHY_REGS_PIR_PHYRST,
					ALPINE_V2_DWC_DDR_PHY_REGS_PIR_INIT |
					ALPINE_V2_DWC_DDR_PHY_REGS_PIR_PHYRST);
			al_data_memory_barrier();
			err2 = al_ddr_reg_poll32(
				&phy_regs->alpine_v2.dwc_ddrphy_pub.pgsr[0],
				ALPINE_V2_DWC_DDR_PHY_REGS_PGSR0_IDONE,
				ALPINE_V2_DWC_DDR_PHY_REGS_PGSR0_IDONE,
				DEFAULT_TIMEOUT);
			if (err2)
				al_err("%s: al_ddr_reg_poll32 failed!\n", __func__);
	
			if (bist_err_status) {
				read_bist_err_status(ddr_cfg, bist_err_status);
				bist_err_status->timeout = AL_TRUE;
			}

			/* TODO : check if there are any new status registers */
			al_dbg("%s: al_ddr_reg_poll32 failed!\n", __func__);
			al_dbg("%s: PHY bist error (BISTGSR = %08x)!\n", __func__,
				al_reg_read32(&phy_regs->alpine_v2.dwc_ddrphy_pub.bistgsr));
			al_dbg("%s: (BISTWER0 = %08x)!\n", __func__,
				al_reg_read32(&phy_regs->alpine_v2.dwc_ddrphy_pub.bistwer[0]));
			al_dbg("%s: (BISTBER2 = %08x)!\n", __func__,
				al_reg_read32(&phy_regs->alpine_v2.dwc_ddrphy_pub.bistber[2]));
			al_dbg("%s: (BISTBER3 = %08x)!\n", __func__,
				al_reg_read32(&phy_regs->alpine_v2.dwc_ddrphy_pub.bistber[3]));
			al_dbg("%s: (BISTWCSR = %08x)!\n", __func__,
				al_reg_read32(&phy_regs->alpine_v2.dwc_ddrphy_pub.bistwcsr));
			al_dbg("%s: (BISTFWR2 = %08x)!\n", __func__,
				al_reg_read32(&phy_regs->alpine_v2.dwc_ddrphy_pub.bistfwr[2]));
			return err;
		}
	}

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTGSR);
		/* Make sure no bist errors */
		if (reg_val &
			(DWC_DDR_PHY_REGS_BISTGSR_BACERR |
			DWC_DDR_PHY_REGS_BISTGSR_BDXERR)) {
			if (bist_err_status) {
				read_bist_err_status(ddr_cfg, bist_err_status);
				bist_err_status->timeout = AL_FALSE;
			}
			al_dbg("%s: PHY bist error (BISTGSR = %08x)!\n",
				__func__, reg_val);
			al_dbg("%s: (BISTWER = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTWER));
			al_dbg("%s: (BISTBER2 = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTBER[2]));
			al_dbg("%s: (BISTBER3 = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTBER[3]));
			al_dbg("%s: (BISTWCSR = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTWCSR));
			al_dbg("%s: (BISTFWR2 = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v1.dwc_ddrphy_pub.BISTFWR[2]));
			return -EIO;
		}
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		reg_val = al_reg_read32(&phy_regs->alpine_v2.dwc_ddrphy_pub.bistgsr);
		/* Make sure no bist errors */
		if (reg_val &
			(ALPINE_V2_DWC_DDR_PHY_REGS_BISTGSR_BACERR |
			ALPINE_V2_DWC_DDR_PHY_REGS_BISTGSR_BDXERR_MASK)) {
			if (bist_err_status) {
				read_bist_err_status(ddr_cfg, bist_err_status);
				bist_err_status->timeout = AL_FALSE;
			}
			/* TODO : check if there are any new status registers */
			al_dbg("%s: PHY bist error (BISTGSR = %08x)!\n",
				__func__, reg_val);
			al_dbg("%s: (BISTWER0 = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v2.dwc_ddrphy_pub.bistwer[0]));
			al_dbg("%s: (BISTBER2 = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v2.dwc_ddrphy_pub.bistber[2]));
			al_dbg("%s: (BISTBER3 = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v2.dwc_ddrphy_pub.bistber[3]));
			al_dbg("%s: (BISTWCSR = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v2.dwc_ddrphy_pub.bistwcsr));
			al_dbg("%s: (BISTFWR2 = %08x)!\n",
				__func__, al_reg_read32(
				&phy_regs->alpine_v2.dwc_ddrphy_pub.bistfwr[2]));
			return -EIO;
		}
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
unsigned int al_ddr_active_ranks_get(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;
	uint32_t mstr_val;
	unsigned int active_ranks = 0;

	mstr_val = al_reg_read32(CTRL_REG(mstr));
	mstr_val &= DWC_DDR_UMCTL2_REGS_MSTR_ACTIVE_RANKS_MASK;
	mstr_val >>= DWC_DDR_UMCTL2_REGS_MSTR_ACTIVE_RANKS_SHIFT;

	/* each bit on mstr_val is corresponding to an available rank */
	while(mstr_val > 0) {
		active_ranks += 1;
		mstr_val >>= 1;
	}

	return active_ranks;
}

/*******************************************************************************
 ******************************************************************************/
unsigned int al_ddr_active_banks_get(
	struct al_ddr_cfg		*ddr_cfg)

{
	struct al_ddr_addrmap addrmap;
	unsigned int num = 0;
	unsigned int i;

	al_ddr_address_map_get(ddr_cfg, &addrmap);

	for (i = 0; i < AL_ARR_SIZE(addrmap.bank_b0_2); i++) {
		if (addrmap.bank_b0_2[i] != AL_DDR_ADDRMAP_NC)
			num++;
	}

	return (1 << num);
}

/*******************************************************************************
 ******************************************************************************/
unsigned int al_ddr_active_bg_get(
	struct al_ddr_cfg		*ddr_cfg)
{
	struct al_ddr_addrmap addrmap;
	unsigned int num = 0;
	unsigned int i;

	al_ddr_address_map_get(ddr_cfg, &addrmap);

	for (i = 0; i < AL_ARR_SIZE(addrmap.bg_b0_1); i++) {
		if (addrmap.bg_b0_1[i] != AL_DDR_ADDRMAP_NC)
			num++;
	}

	return (1 << num);
}

/*******************************************************************************
 ******************************************************************************/
unsigned int al_ddr_active_columns_get(
	struct al_ddr_cfg		*ddr_cfg)

{
	struct al_ddr_addrmap addrmap;
	unsigned int num = 3; /* At least 3 column bits */
	unsigned int i;

	al_ddr_address_map_get(ddr_cfg, &addrmap);

	for (i = 0; i < AL_ARR_SIZE(addrmap.col_b3_9_b11_13); i++) {
		if (addrmap.col_b3_9_b11_13[i] != AL_DDR_ADDRMAP_NC)
			num++;
	}

	return (1 << num);
}

/*******************************************************************************
 ******************************************************************************/
unsigned int al_ddr_active_rows_get(
	struct al_ddr_cfg		*ddr_cfg)
{
	struct al_ddr_addrmap addrmap;
	unsigned int num = 0;
	unsigned int i;

	al_ddr_address_map_get(ddr_cfg, &addrmap);

	for (i = 0; i < (AL_ARR_SIZE(addrmap.row_b0_17)); i++) {
		if (addrmap.row_b0_17[i] != AL_DDR_ADDRMAP_NC)
			num++;
	}

	return (1 << num);
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_ecc_status_get(
	struct al_ddr_cfg		*ddr_cfg,
	struct al_ddr_ecc_status	*corr_status,
	struct al_ddr_ecc_status	*uncorr_status)
{
	uint32_t reg_val;

	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	/* Correctable status */
	if (corr_status) {
		reg_val = al_reg_read32(CTRL_REG(eccstat));
		corr_status->ecc_corrected_bit_num = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCSTAT_ECC_CORRECTED_BIT_NUM_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCSTAT_ECC_CORRECTED_BIT_NUM_SHIFT;

		reg_val = al_reg_read32(CTRL_REG(eccerrcnt));
		corr_status->err_cnt = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCERRCNT_ECC_CORR_ERR_CNT_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCERRCNT_ECC_CORR_ERR_CNT_SHIFT;

		reg_val = al_reg_read32(CTRL_REG(ecccaddr0));
		corr_status->row = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCCADDR0_ECC_CORR_ROW_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCCADDR0_ECC_CORR_ROW_SHIFT;
		corr_status->rank = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCCADDR0_ECC_CORR_RANK_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCCADDR0_ECC_CORR_RANK_SHIFT;

		reg_val = al_reg_read32(CTRL_REG(ecccaddr1));
		if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
			corr_status->bg = 0;
		} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
			corr_status->bg = (reg_val &
				DWC_DDR_UMCTL2_REGS_ECCCADDR1_ECC_CORR_BG_MASK)
				>> DWC_DDR_UMCTL2_REGS_ECCCADDR1_ECC_CORR_BG_SHIFT;
		}
		corr_status->bank = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCCADDR1_ECC_CORR_BANK_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCCADDR1_ECC_CORR_BANK_SHIFT;
		corr_status->col = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCCADDR1_ECC_CORR_COL_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCCADDR1_ECC_CORR_COL_SHIFT;

		corr_status->syndromes_31_0 = al_reg_read32(CTRL_REG(ecccsyn0));
		corr_status->syndromes_63_32 = al_reg_read32(CTRL_REG(ecccsyn1));
		reg_val = al_reg_read32(CTRL_REG(ecccsyn2));
		corr_status->syndromes_ecc = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCCSYN2_ECC_CORR_SYNDROMES_71_64_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCCSYN2_ECC_CORR_SYNDROMES_71_64_SHIFT;

		corr_status->corr_bit_mask_31_0 =
			al_reg_read32(CTRL_REG(eccbitmask0));
		corr_status->corr_bit_mask_63_32 =
			al_reg_read32(CTRL_REG(eccbitmask1));
		reg_val = al_reg_read32(CTRL_REG(eccbitmask2));
		corr_status->corr_bit_mask_ecc = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCBITMASK2_ECC_CORR_BIT_MASK_71_64_MASK) >>
			DWC_DDR_UMCTL2_REGS_ECCBITMASK2_ECC_CORR_BIT_MASK_71_64_SHIFT;
	}

	/* Uncorrectable status */
	if (uncorr_status) {
		reg_val = al_reg_read32(CTRL_REG(eccerrcnt));
		uncorr_status->err_cnt = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCERRCNT_ECC_UNCORR_ERR_CNT_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCERRCNT_ECC_UNCORR_ERR_CNT_SHIFT;

		reg_val = al_reg_read32(CTRL_REG(eccuaddr0));
		uncorr_status->row = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCUADDR0_ECC_UNCORR_ROW_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCUADDR0_ECC_UNCORR_ROW_SHIFT;
		uncorr_status->rank = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCUADDR0_ECC_UNCORR_RANK_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCUADDR0_ECC_UNCORR_RANK_SHIFT;

		reg_val = al_reg_read32(CTRL_REG(eccuaddr1));
		if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
			uncorr_status->bg = 0;
		} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
			uncorr_status->bg = (reg_val &
				DWC_DDR_UMCTL2_REGS_ECCUADDR1_ECC_UNCORR_BG_MASK)
				>> DWC_DDR_UMCTL2_REGS_ECCUADDR1_ECC_UNCORR_BG_SHIFT;
		}
		uncorr_status->bank = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCUADDR1_ECC_UNCORR_BANK_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCUADDR1_ECC_UNCORR_BANK_SHIFT;
		uncorr_status->col = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCUADDR1_ECC_UNCORR_COL_MASK)
			>> DWC_DDR_UMCTL2_REGS_ECCUADDR1_ECC_UNCORR_COL_SHIFT;

		uncorr_status->syndromes_31_0 = al_reg_read32(CTRL_REG(eccusyn0));
		uncorr_status->syndromes_63_32 = al_reg_read32(CTRL_REG(eccusyn1));
		reg_val = al_reg_read32(CTRL_REG(eccusyn2));
		uncorr_status->syndromes_ecc = (reg_val &
			DWC_DDR_UMCTL2_REGS_ECCUSYN2_ECC_UNCORR_SYNDROMES_71_64_MASK) >>
			DWC_DDR_UMCTL2_REGS_ECCUSYN2_ECC_UNCORR_SYNDROMES_71_64_SHIFT;
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
static al_bool _al_ddr_ecc_is_enabled(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;
	uint32_t reg_val;

	reg_val = al_reg_read32(CTRL_REG(ecccfg0));

	return ((reg_val & DWC_DDR_UMCTL2_REGS_ECCCFG0_ECC_MODE_MASK) ==
		DWC_DDR_UMCTL2_REGS_ECCCFG0_ECC_MODE_DIS) ? AL_FALSE : AL_TRUE;
}

/*******************************************************************************
 ******************************************************************************/
void al_ddr_ecc_cfg_get(
	struct al_ddr_cfg		*ddr_cfg,
	struct al_ddr_ecc_cfg		*ecc_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;
	uint32_t cfg_val;

	cfg_val = al_reg_read32(CTRL_REG(ecccfg0));

	ecc_cfg->ecc_enabled =
		((cfg_val & DWC_DDR_UMCTL2_REGS_ECCCFG0_ECC_MODE_MASK) ==
		DWC_DDR_UMCTL2_REGS_ECCCFG0_ECC_MODE_DIS) ? AL_FALSE : AL_TRUE;

	/* dis_scrub is valid only when ecc mode is enabled */
	if (ecc_cfg->ecc_enabled)
		ecc_cfg->scrub_enabled =
			(cfg_val & DWC_DDR_UMCTL2_REGS_ECCCFG0_DIS_SCRUB) ?
			AL_FALSE : AL_TRUE;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_ecc_corr_count_clear(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	al_reg_write32(CTRL_REG(eccclr),
			DWC_DDR_UMCTL2_REGS_ECCCLR_ECC_CLR_CORR_ERR_CNT);

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_ecc_corr_int_clear(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	al_reg_write32(CTRL_REG(eccclr),
			DWC_DDR_UMCTL2_REGS_ECCCLR_ECC_CLR_CORR_ERR);

	/* TODO : was it really necessary not to clear the interrupt in NB? */
	if (ddr_cfg->nb_regs_base) {
		struct al_nb_regs __iomem *nb_regs;

		al_data_memory_barrier();

		nb_regs = (struct al_nb_regs __iomem *)ddr_cfg->nb_regs_base;
		al_reg_write32(&nb_regs->global.nb_int_cause,
				~NB_GLOBAL_NB_INT_CAUSE_MCTL_ECC_CORR_ERR);
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_ecc_uncorr_count_clear(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	al_reg_write32(CTRL_REG(eccclr),
			DWC_DDR_UMCTL2_REGS_ECCCLR_ECC_CLR_UNCORR_ERR_CNT);

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_ecc_uncorr_int_clear(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	al_reg_write32(CTRL_REG(eccclr),
			DWC_DDR_UMCTL2_REGS_ECCCLR_ECC_CLR_UNCORR_ERR);

	/* TODO : was it really necessary not to clear the interrupt in NB? */
	if (ddr_cfg->nb_regs_base) {
		struct al_nb_regs __iomem *nb_regs;

		al_data_memory_barrier();

		nb_regs = (struct al_nb_regs __iomem *)ddr_cfg->nb_regs_base;
		al_reg_write32(&nb_regs->global.nb_int_cause,
				~NB_GLOBAL_NB_INT_CAUSE_MCTL_ECC_UNCORR_ERR);
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_ecc_data_poison_enable(
	struct al_ddr_cfg	*ddr_cfg,
	unsigned int		rank,
	unsigned int		bank,
	unsigned int		bg,
	unsigned int		col,
	unsigned int		row)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		/* Set data poison address */
		al_reg_write32(CTRL_REG(eccpoisonaddr0),
			(col <<
			DWC_DDR_UMCTL2_REGS_ECCPOISONADDR0_ECC_POISON_COL_SHIFT) |
			(rank <<
			DWC_DDR_UMCTL2_REGS_ECCPOISONADDR0_ECC_POISON_RANK_SHIFT));
		al_reg_write32(CTRL_REG(eccpoisonaddr1),
			(row <<
			DWC_DDR_UMCTL2_REGS_ECCPOISONADDR1_ECC_POISON_ROW_SHIFT) |
			(bank <<
			DWC_DDR_UMCTL2_REGS_ECCPOISONADDR1_ECC_POISON_BANK_SHIFT));

		/* Enable data poisoning */
		al_reg_write32(CTRL_REG(ecccfg1),
				DWC_DDR_UMCTL2_REGS_ECCCFG1_DATA_POISON_EN);
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		/* Set data poison address */
		al_reg_write32(CTRL_REG(eccpoisonaddr0),
			(col <<
			DWC_DDR_UMCTL2_REGS_ECCPOISONADDR0_ECC_POISON_COL_SHIFT) |
			(rank <<
			DWC_DDR_UMCTL2_REGS_ECCPOISONADDR0_ECC_POISON_RANK_SHIFT));
		al_reg_write32(CTRL_REG(eccpoisonaddr1),
			(row <<
			DWC_DDR_UMCTL2_REGS_ECCPOISONADDR1_ECC_POISON_ROW_SHIFT) |
			(bank <<
			DWC_DDR_UMCTL2_REGS_ECCPOISONADDR1_ECC_POISON_BANK_SHIFT) |
			(bg <<
			DWC_DDR_UMCTL2_REGS_ECCPOISONADDR1_ECC_POISON_BG_SHIFT));

		/* Enable data poisoning */
		al_reg_write32(CTRL_REG(ecccfg1),
				DWC_DDR_UMCTL2_REGS_ECCCFG1_DATA_POISON_EN);
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_ecc_data_poison_disable(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	/* Disable data poisoning */
	al_reg_write32(CTRL_REG(ecccfg1), 0);

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
unsigned int al_ddr_parity_count_get(
	struct al_ddr_cfg		*ddr_cfg)
{
	uint32_t reg_val;
	unsigned int parity_count;

	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		reg_val = al_reg_read32(&ctrl_regs->alpine_v1.umctl2_regs.parstat);
		parity_count = (reg_val &
			DWC_DDR_UMCTL2_REGS_CRCPARSTAT_DFI_ALERT_ERR_CNT_MASK)
			>> DWC_DDR_UMCTL2_REGS_CRCPARSTAT_DFI_ALERT_ERR_CNT_SHIFT;
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		reg_val = al_reg_read32(&ctrl_regs->alpine_v2.umctl2_regs.crcparstat);
		parity_count = (reg_val &
			DWC_DDR_UMCTL2_REGS_CRCPARSTAT_DFI_ALERT_ERR_CNT_MASK)
			>> DWC_DDR_UMCTL2_REGS_CRCPARSTAT_DFI_ALERT_ERR_CNT_SHIFT;
	} else {
		al_err("%s: Unknown revision: %d!\n", __func__, ddr_cfg->rev);
		al_assert(AL_FALSE);
		return 0;
	}

	return parity_count;
}

/*******************************************************************************
 ******************************************************************************/
void al_ddr_parity_count_clear(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		al_reg_write32_masked(&ctrl_regs->alpine_v1.umctl2_regs.parctl,
				DWC_DDR_UMCTL2_REGS_CRCPARCTL0_DFI_ALERT_ERR_CNT_CLR,
				DWC_DDR_UMCTL2_REGS_CRCPARCTL0_DFI_ALERT_ERR_CNT_CLR);
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		al_reg_write32_masked(&ctrl_regs->alpine_v2.umctl2_regs.crcparctl0,
				DWC_DDR_UMCTL2_REGS_CRCPARCTL0_DFI_ALERT_ERR_CNT_CLR,
				DWC_DDR_UMCTL2_REGS_CRCPARCTL0_DFI_ALERT_ERR_CNT_CLR);
	}
}

/*******************************************************************************
 ******************************************************************************/
void al_ddr_parity_int_clear(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	struct al_nb_regs __iomem *nb_regs =
		(struct al_nb_regs __iomem *)ddr_cfg->nb_regs_base;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		al_reg_write32_masked(&ctrl_regs->alpine_v1.umctl2_regs.parctl,
				DWC_DDR_UMCTL2_REGS_CRCPARCTL0_DFI_ALERT_ERR_INT_CLR,
				DWC_DDR_UMCTL2_REGS_CRCPARCTL0_DFI_ALERT_ERR_INT_CLR);
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		al_reg_write32_masked(&ctrl_regs->alpine_v2.umctl2_regs.crcparctl0,
				DWC_DDR_UMCTL2_REGS_CRCPARCTL0_DFI_ALERT_ERR_INT_CLR,
				DWC_DDR_UMCTL2_REGS_CRCPARCTL0_DFI_ALERT_ERR_INT_CLR);
	}

	al_data_memory_barrier();

	/* TODO : check if correct for ALPINE_V2 */
	al_reg_write32(&nb_regs->global.nb_int_cause,
			~NB_GLOBAL_NB_INT_CAUSE_MCTL_PARITY_ERR);
}

/*******************************************************************************
 ******************************************************************************/
static int al_ddr_address_map_get(
	struct al_ddr_cfg	*ddr_cfg,
	struct al_ddr_addrmap	*addrmap)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;

	struct al_nb_regs __iomem *nb_regs =
		(struct al_nb_regs __iomem *)ddr_cfg->nb_regs_base;

	uint32_t reg_val;
	int addrmap_col_b2_11[10];
	int addrmap_bank_b0_2[3];
	int addrmap_bg_b0_1[2];
	int addrmap_row_b0_17[18];
	int addrmap_cs_b0_1[2];

	unsigned int i;

	enum al_ddr_data_width data_width =
			al_ddr_data_width_get(ddr_cfg);

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		reg_val = al_reg_read32(&nb_regs->debug.cpus_general);
		addrmap->swap_14_16_with_17_19 = (reg_val &
			NB_DEBUG_CPUS_GENERAL_ADDR_MAP_ECO) ==
			NB_DEBUG_CPUS_GENERAL_ADDR_MAP_ECO ? AL_TRUE : AL_FALSE;
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		addrmap->swap_14_16_with_17_19 = 0;
	}

	/**
	* CS address mapping
	*/
	reg_val = al_reg_read32(CTRL_REG(addrmap0));
	addrmap_cs_b0_1[1] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP0_ADDRMAP_CS_BIT1_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP0_ADDRMAP_CS_BIT1_SHIFT;
	addrmap_cs_b0_1[0] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP0_ADDRMAP_CS_BIT0_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP0_ADDRMAP_CS_BIT0_SHIFT;

	/**
	* Bank address mapping
	*/
	reg_val = al_reg_read32(CTRL_REG(addrmap1));
	addrmap_bank_b0_2[2] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP1_ADDRMAP_BANK_B2_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP1_ADDRMAP_BANK_B2_SHIFT;
	addrmap_bank_b0_2[1] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP1_ADDRMAP_BANK_B1_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP1_ADDRMAP_BANK_B1_SHIFT;
	addrmap_bank_b0_2[0] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP1_ADDRMAP_BANK_B0_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP1_ADDRMAP_BANK_B0_SHIFT;

	/**
	* Bank Group address mapping
	*/
	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		reg_val = al_reg_read32(&ctrl_regs->alpine_v2.umctl2_regs.addrmap8);
		addrmap_bg_b0_1[1] = (reg_val &
			DWC_DDR_UMCTL2_REGS_ADDRMAP8_ADDRMAP_BG_B1_MASK)
			>> DWC_DDR_UMCTL2_REGS_ADDRMAP8_ADDRMAP_BG_B1_SHIFT;
		addrmap_bg_b0_1[0] = (reg_val &
			DWC_DDR_UMCTL2_REGS_ADDRMAP8_ADDRMAP_BG_B0_MASK)
			>> DWC_DDR_UMCTL2_REGS_ADDRMAP8_ADDRMAP_BG_B0_SHIFT;
	}

	/**
	* Column address mapping
	*/
	reg_val = al_reg_read32(CTRL_REG(addrmap2));
	addrmap_col_b2_11[3] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP2_ADDRMAP_COL_B5_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP2_ADDRMAP_COL_B5_SHIFT;
	addrmap_col_b2_11[2] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP2_ADDRMAP_COL_B4_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP2_ADDRMAP_COL_B4_SHIFT;
	addrmap_col_b2_11[1] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP2_ADDRMAP_COL_B3_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP2_ADDRMAP_COL_B3_SHIFT;
	addrmap_col_b2_11[0] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP2_ADDRMAP_COL_B2_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP2_ADDRMAP_COL_B2_SHIFT;

	reg_val = al_reg_read32(CTRL_REG(addrmap3));
	addrmap_col_b2_11[7] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP3_ADDRMAP_COL_B9_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP3_ADDRMAP_COL_B9_SHIFT;
	addrmap_col_b2_11[6] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP3_ADDRMAP_COL_B8_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP3_ADDRMAP_COL_B8_SHIFT;
	addrmap_col_b2_11[5] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP3_ADDRMAP_COL_B7_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP3_ADDRMAP_COL_B7_SHIFT;
	addrmap_col_b2_11[4] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP3_ADDRMAP_COL_B6_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP3_ADDRMAP_COL_B6_SHIFT;

	reg_val = al_reg_read32(CTRL_REG(addrmap4));
	addrmap_col_b2_11[9] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP4_ADDRMAP_COL_B11_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP4_ADDRMAP_COL_B11_SHIFT;
	addrmap_col_b2_11[8] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP4_ADDRMAP_COL_B10_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP4_ADDRMAP_COL_B10_SHIFT;

	/**
	* Row address mapping
	*/
	reg_val = al_reg_read32(CTRL_REG(addrmap5));
	addrmap_row_b0_17[11] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B11_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B11_SHIFT;
	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		addrmap_row_b0_17[2] = (reg_val &
			DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B2_10_MASK)
			>> DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B2_10_SHIFT;
		for (i = 3; i < 11; i++)
			addrmap_row_b0_17[i] = addrmap_row_b0_17[2];
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		addrmap_row_b0_17[6] = (reg_val &
			DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B2_10_MASK)
			>> DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B2_10_SHIFT;
		for (i = 7; i < 11; i++)
			addrmap_row_b0_17[i] = addrmap_row_b0_17[6];
	}
	addrmap_row_b0_17[1] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B1_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B1_SHIFT;
	addrmap_row_b0_17[0] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B0_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP5_ADDRMAP_ROW_B0_SHIFT;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		reg_val = al_reg_read32(&nb_regs->dram_channels.address_map);
		addrmap_row_b0_17[2] = (reg_val &
			NB_DRAM_CHANNELS_ADDRESS_MAP_ADDRMAP_ROW_B2_MASK)
			>> NB_DRAM_CHANNELS_ADDRESS_MAP_ADDRMAP_ROW_B2_SHIFT;
		addrmap_row_b0_17[3] = (reg_val &
			NB_DRAM_CHANNELS_ADDRESS_MAP_ADDRMAP_ROW_B3_MASK)
			>> NB_DRAM_CHANNELS_ADDRESS_MAP_ADDRMAP_ROW_B3_SHIFT;
		addrmap_row_b0_17[4] = (reg_val &
			NB_DRAM_CHANNELS_ADDRESS_MAP_ADDRMAP_ROW_B4_MASK)
			>> NB_DRAM_CHANNELS_ADDRESS_MAP_ADDRMAP_ROW_B4_SHIFT;
		addrmap_row_b0_17[5] = (reg_val &
			NB_DRAM_CHANNELS_ADDRESS_MAP_ADDRMAP_ROW_B5_MASK)
			>> NB_DRAM_CHANNELS_ADDRESS_MAP_ADDRMAP_ROW_B5_SHIFT;
	}

	reg_val = al_reg_read32(CTRL_REG(addrmap6));
	addrmap_row_b0_17[15] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP6_ADDRMAP_ROW_B15_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP6_ADDRMAP_ROW_B15_SHIFT;
	addrmap_row_b0_17[14] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP6_ADDRMAP_ROW_B14_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP6_ADDRMAP_ROW_B14_SHIFT;
	addrmap_row_b0_17[13] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP6_ADDRMAP_ROW_B13_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP6_ADDRMAP_ROW_B13_SHIFT;
	addrmap_row_b0_17[12] = (reg_val &
		DWC_DDR_UMCTL2_REGS_ADDRMAP6_ADDRMAP_ROW_B12_MASK)
		>> DWC_DDR_UMCTL2_REGS_ADDRMAP6_ADDRMAP_ROW_B12_SHIFT;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		addrmap_row_b0_17[17] = AL_DDR_ADDR_MAP_ROW_DISABLED;
		addrmap_row_b0_17[16] = AL_DDR_ADDR_MAP_ROW_DISABLED;
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		reg_val = al_reg_read32(&ctrl_regs->alpine_v2.umctl2_regs.addrmap7);
		addrmap_row_b0_17[17] = (reg_val &
			DWC_DDR_UMCTL2_REGS_ADDRMAP7_ADDRMAP_ROW_B17_MASK)
			>> DWC_DDR_UMCTL2_REGS_ADDRMAP7_ADDRMAP_ROW_B17_SHIFT;
		addrmap_row_b0_17[16] = (reg_val &
			DWC_DDR_UMCTL2_REGS_ADDRMAP7_ADDRMAP_ROW_B16_MASK)
			>> DWC_DDR_UMCTL2_REGS_ADDRMAP7_ADDRMAP_ROW_B16_SHIFT;
	}

	/* Column */
	for (i = 0; i < (AL_ARR_SIZE(addrmap->col_b3_9_b11_13) - 1); i++) {
		int user_val;

		user_val = addrmap_col_b2_11[i];

		if (data_width == AL_DDR_DATA_WIDTH_64_BITS)
			addrmap->col_b3_9_b11_13[i] =
				(user_val == AL_DDR_ADDR_MAP_COL_DISABLED) ?
				AL_DDR_ADDRMAP_NC :
				(user_val + AL_DDR_ADDR_MAP_OFFSET +
				AL_DDR_ADDR_MAP_COL_2_BASE + i);
		else
			addrmap->col_b3_9_b11_13[i + 1] =
				(user_val == AL_DDR_ADDR_MAP_COL_DISABLED) ?
				AL_DDR_ADDRMAP_NC :
				(user_val + AL_DDR_ADDR_MAP_OFFSET +
				AL_DDR_ADDR_MAP_COL_2_BASE + i);
	}

	if (data_width == AL_DDR_DATA_WIDTH_64_BITS)
		addrmap->col_b3_9_b11_13[i] = AL_DDR_ADDRMAP_NC;
	if (data_width == AL_DDR_DATA_WIDTH_32_BITS)
		addrmap->col_b3_9_b11_13[0] = 5;

	/* Bank */
	for (i = 0; i < AL_ARR_SIZE(addrmap->bank_b0_2); i++) {
		int user_val = addrmap_bank_b0_2[i];

		if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
			addrmap->bank_b0_2[i] =
				(user_val == AL_DDR_ADDR_MAP_BANK_DISABLED_ALPINE_V1) ?
				AL_DDR_ADDRMAP_NC :
				(user_val + AL_DDR_ADDR_MAP_OFFSET +
				AL_DDR_ADDR_MAP_BANK_0_BASE + i);
		} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
			addrmap->bank_b0_2[i] =
				(user_val == AL_DDR_ADDR_MAP_BANK_DISABLED_ALPINE_V2) ?
				AL_DDR_ADDRMAP_NC :
				(user_val + AL_DDR_ADDR_MAP_OFFSET +
				AL_DDR_ADDR_MAP_BANK_0_BASE + i);
		}
	}

	/* Bank Group */
	for (i = 0; i < AL_ARR_SIZE(addrmap->bg_b0_1); i++) {
		int user_val = addrmap_bg_b0_1[i];

		if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
			addrmap->bg_b0_1[i] = AL_DDR_ADDRMAP_NC;
		} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
			addrmap->bg_b0_1[i] =
				(user_val == AL_DDR_ADDR_MAP_BG_DISABLED) ?
				AL_DDR_ADDRMAP_NC :
				(user_val + AL_DDR_ADDR_MAP_OFFSET +
				AL_DDR_ADDR_MAP_BG_0_BASE + i);
		}
	}

	/* CS */
	for (i = 0; i < AL_ARR_SIZE(addrmap->cs_b0_1); i++) {
		int user_val = addrmap_cs_b0_1[i];

		addrmap->cs_b0_1[i] =
			(user_val == AL_DDR_ADDR_MAP_CS_DISABLED) ?
			AL_DDR_ADDRMAP_NC :
			(user_val + AL_DDR_ADDR_MAP_OFFSET +
			AL_DDR_ADDR_MAP_CS_0_BASE + i);
	}

	/* Row */
	for (i = 0; i < AL_ARR_SIZE(addrmap->row_b0_17); i++) {
		int user_val = addrmap_row_b0_17[i];

		addrmap->row_b0_17[i] =
			(user_val == AL_DDR_ADDR_MAP_ROW_DISABLED) ?
			AL_DDR_ADDRMAP_NC :
			(user_val + AL_DDR_ADDR_MAP_OFFSET +
			AL_DDR_ADDR_MAP_ROW_0_BASE + i);
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
static enum al_ddr_data_width _al_ddr_data_width_get(
	struct al_ddr_cfg		*ddr_cfg)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;
	uint32_t reg_val;
	enum al_ddr_data_width data_width;

	reg_val = al_reg_read32(CTRL_REG(mstr));
	reg_val &= DWC_DDR_UMCTL2_REGS_MSTR_DATA_BUS_WIDTH_MASK;

	data_width =
		(reg_val == DWC_DDR_UMCTL2_REGS_MSTR_DATA_BUS_WIDTH_64) ?
		AL_DDR_DATA_WIDTH_64_BITS :
		AL_DDR_DATA_WIDTH_32_BITS;

	return data_width;
}

/*******************************************************************************
 ******************************************************************************/
enum al_ddr_data_width al_ddr_data_width_get(
	struct al_ddr_cfg		*ddr_cfg)
{
	return _al_ddr_data_width_get(ddr_cfg);
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_address_translate_sys2dram(
	struct al_ddr_cfg		*ddr_cfg,
	al_phys_addr_t			sys_address,
	unsigned int			*rank,
	unsigned int			*bank,
	unsigned int			*bg,
	unsigned int			*col,
	unsigned int			*row)
{

	unsigned int i;
	unsigned int temp_rank = 0;
	unsigned int temp_bank = 0;
	unsigned int temp_bg = 0;
	unsigned int temp_col = 0;
	unsigned int temp_row = 0;
	struct al_ddr_addrmap addrmap;

	enum al_ddr_data_width data_width =
			al_ddr_data_width_get(ddr_cfg);

	al_ddr_address_map_get(ddr_cfg, &addrmap);

	if (addrmap.swap_14_16_with_17_19 == AL_TRUE) {
		/* swap sysaddr[16:14] with sysaddr[19:17] */
		AL_REG_CLEAR_AND_SET(sys_address,AL_FIELD_MASK(19,14),
			(AL_REG_FIELD_GET(sys_address,AL_FIELD_MASK(16,14),14) << 17) |
			(AL_REG_FIELD_GET(sys_address,AL_FIELD_MASK(19,17),17) << 14));
	}

	if (data_width == AL_DDR_DATA_WIDTH_64_BITS)
		temp_col += ((sys_address >> 3) & 0x7);
	else
		temp_col += ((sys_address >> 2) & 0x7);

	for (i = 0; i < 7; i++)
		if (addrmap.col_b3_9_b11_13[i] != AL_DDR_ADDRMAP_NC){
			temp_col += (((sys_address >> addrmap.col_b3_9_b11_13[i]) & 0x1) << (i + 3));
		}
	if (addrmap.col_b3_9_b11_13[7] != AL_DDR_ADDRMAP_NC)
		temp_col += (((sys_address >> addrmap.col_b3_9_b11_13[7]) & 0x1) << 11);
	if (addrmap.col_b3_9_b11_13[8] != AL_DDR_ADDRMAP_NC)
		temp_col += (((sys_address >> addrmap.col_b3_9_b11_13[8]) & 0x1) << 13);

	for (i = 0; i < 3; i++)
		if (addrmap.bank_b0_2[i] != AL_DDR_ADDRMAP_NC)
			temp_bank += (((sys_address >> addrmap.bank_b0_2[i]) & 0x1) << i);

	for (i = 0; i < 2; i++)
		if (addrmap.bg_b0_1[i] != AL_DDR_ADDRMAP_NC)
			temp_bg += (((sys_address >> addrmap.bg_b0_1[i]) & 0x1) << i);

	for (i = 0; i < AL_ARR_SIZE(addrmap.row_b0_17); i++)
		if (addrmap.row_b0_17[i] != AL_DDR_ADDRMAP_NC)
			temp_row += (((sys_address >> addrmap.row_b0_17[i]) & 0x1) << i);

	for (i = 0; i < 2; i++)
		if (addrmap.cs_b0_1[i] != AL_DDR_ADDRMAP_NC)
			temp_rank += (((sys_address >> addrmap.cs_b0_1[i]) & 0x1) << i);

	*rank = temp_rank;
	*bank = temp_bank;
	*bg = temp_bg;
	*col = temp_col;
	*row = temp_row;

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_address_translate_dram2sys(
	struct al_ddr_cfg		*ddr_cfg,
	al_phys_addr_t			*sys_address,
	unsigned int			rank,
	unsigned int			bank,
	unsigned int			bg,
	unsigned int			col,
	unsigned int			row)
{
	unsigned int i;
	struct al_ddr_addrmap addrmap;
	al_phys_addr_t address = 0;

	enum al_ddr_data_width data_width =
			al_ddr_data_width_get(ddr_cfg);
	al_ddr_address_map_get(ddr_cfg, &addrmap);

	if (data_width == AL_DDR_DATA_WIDTH_64_BITS)
		address += ((col & 0x7) << 3);
	else
		address += ((col & 0x7) << 2);

	for (i = 0; i < 7; i++)
		if (addrmap.col_b3_9_b11_13[i] != AL_DDR_ADDRMAP_NC)
			address += ((((al_phys_addr_t)col >> (i + 3)) & 0x1) << addrmap.col_b3_9_b11_13[i]);

	if (addrmap.col_b3_9_b11_13[7] != AL_DDR_ADDRMAP_NC)
		address += ((((al_phys_addr_t)col >> 11) & 0x1) << addrmap.col_b3_9_b11_13[7]);
	if (addrmap.col_b3_9_b11_13[8] != AL_DDR_ADDRMAP_NC)
		address += ((((al_phys_addr_t)col >> 13) & 0x1) << addrmap.col_b3_9_b11_13[8]);

	for (i = 0; i < 3; i++)
		if (addrmap.bank_b0_2[i] != AL_DDR_ADDRMAP_NC)
			address += ((((al_phys_addr_t)bank >> (i)) & 0x1) << addrmap.bank_b0_2[i]);

	for (i = 0; i < 2; i++)
		if (addrmap.bg_b0_1[i] != AL_DDR_ADDRMAP_NC)
			address += ((((al_phys_addr_t)bg >> (i)) & 0x1) << addrmap.bg_b0_1[i]);

	for (i = 0; i < AL_ARR_SIZE(addrmap.row_b0_17); i++)
		if (addrmap.row_b0_17[i] != AL_DDR_ADDRMAP_NC)
			address += ((((al_phys_addr_t)row >> (i)) & 0x1) << addrmap.row_b0_17[i]);

	for (i = 0; i < 2; i++)
		if (addrmap.cs_b0_1[i] != AL_DDR_ADDRMAP_NC)
			address += ((((al_phys_addr_t)rank >> (i)) & 0x1) << addrmap.cs_b0_1[i]);

	if (addrmap.swap_14_16_with_17_19 == AL_TRUE) {
		/* swap sysaddr[16:14] with sysaddr[19:17] */
		AL_REG_CLEAR_AND_SET(address,AL_FIELD_MASK(19,14),
			(AL_REG_FIELD_GET(address,AL_FIELD_MASK(16,14),14) << 17) |
			(AL_REG_FIELD_GET(address,AL_FIELD_MASK(19,17),17) << 14));
	}

	*sys_address = address;

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
unsigned int al_ddr_bits_per_rank_get(
	struct al_ddr_cfg		*ddr_cfg)
{
	unsigned int i, active_bits = 0;
	struct al_ddr_addrmap addrmap;

	enum al_ddr_data_width data_width =
			al_ddr_data_width_get(ddr_cfg);

	al_ddr_address_map_get(ddr_cfg, &addrmap);

	/* 64bit systems have a 6bit offset, 32bit systems have a 5bit offset */
	if (data_width == AL_DDR_DATA_WIDTH_64_BITS)
		active_bits += 6;
	else
		active_bits += 5;

	/* iterate over addrmap, count the amount of connected bits */
	for (i = 0; i < 9; i++)
		if (addrmap.col_b3_9_b11_13[i] != AL_DDR_ADDRMAP_NC)
			active_bits++;

	for (i = 0; i < 3; i++)
		if (addrmap.bank_b0_2[i] != AL_DDR_ADDRMAP_NC)
			active_bits++;

	for (i = 0; i < 2; i++)
		if (addrmap.bg_b0_1[i] != AL_DDR_ADDRMAP_NC)
			active_bits++;

	for (i = 0; i < AL_ARR_SIZE(addrmap.row_b0_17); i++)
		if (addrmap.row_b0_17[i] != AL_DDR_ADDRMAP_NC)
			active_bits++;

	return active_bits;
}

/*******************************************************************************
 ******************************************************************************/
void al_ddr_active_byte_lanes_get(
	struct al_ddr_cfg	*ddr_cfg,
	int		active_byte_lanes[AL_DDR_PHY_NUM_BYTE_LANES])
{
	unsigned int val_active_byte_lanes_64_bits =
		(_al_ddr_data_width_get(ddr_cfg) ==
		AL_DDR_DATA_WIDTH_64_BITS) ? 1 : 0;
	unsigned int val_active_byte_lane_ecc =
		_al_ddr_ecc_is_enabled(ddr_cfg) ? 1 : 0;
	int i;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		for (i = 0; i < AL_DDR_PHY_ECC_BYTE_LANE_INDEX_ALPINE_V1; i++)
			active_byte_lanes[i] = 1;

		active_byte_lanes[i++] = val_active_byte_lane_ecc;

		for (; i < AL_DDR_PHY_NUM_BYTE_LANES; i++)
			active_byte_lanes[i] = val_active_byte_lanes_64_bits;
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		for (i = 0; i < AL_DDR_PHY_ECC_BYTE_LANE_INDEX_ALPINE_V2; i++)
			active_byte_lanes[i] = 1;

		active_byte_lanes[i++] = val_active_byte_lane_ecc;

		for (; i < AL_DDR_PHY_NUM_BYTE_LANES; i++)
			active_byte_lanes[i] = val_active_byte_lanes_64_bits;
	}

}

/*******************************************************************************
 ******************************************************************************/
unsigned int al_ddr_mode_register_get(
	struct al_ddr_cfg	*ddr_cfg,
	unsigned int		mr_index)
{

	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)
		ddr_cfg->ddr_phy_regs_base;

	uint32_t reg_val;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		al_assert(mr_index < 4);
		reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.mr[mr_index]);
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		al_assert(mr_index < 8);
		reg_val = al_reg_read32(&phy_regs->alpine_v2.dwc_ddrphy_pub.mr[mr_index]);
	}  else {
		al_err("%s: Unknown revision: %d!\n", __func__, ddr_cfg->rev);
		al_assert(AL_FALSE);
		return 0;
	}

	return reg_val;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_mode_register_set(
	struct al_ddr_cfg	*ddr_cfg,
	unsigned int		ranks,
	unsigned int		mr_index,
	unsigned int		mr_value)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;
	int err;

	err = al_ddr_reg_poll32(
		CTRL_REG(mrstat),
		DWC_DDR_UMCTL2_REGS_MRSTAT_MR_WR_BUSY,
		0,
		DEFAULT_TIMEOUT);

	if (err) {
		al_err("%s: al_ddr_reg_poll32 failed!\n", __func__);
		return err;
	}

	al_reg_write32(
		CTRL_REG(mrctrl0),
		((ranks << DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_RANK_SHIFT) |
		(mr_index << DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_ADDR_SHIFT) |
		DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_TYPE_WRITE)
		);

	al_reg_write32(
		CTRL_REG(mrctrl1),
		(mr_value << DWC_DDR_UMCTL2_REGS_MRCTRL1_MR_DATA_SHIFT));

	/* Trigger MRS */
	al_reg_write32_masked(
		CTRL_REG(mrctrl0),
		DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_WR,
		DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_WR);

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_phy_zq_get(
	struct al_ddr_cfg	*ddr_cfg,
	unsigned int		zq_segment,
	enum al_ddr_phy_rout	*phy_rout,
	enum al_ddr_phy_odt	*phy_odt)
{
	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)
		ddr_cfg->ddr_phy_regs_base;

	uint32_t reg_val;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		al_assert(zq_segment < AL_DDR_PHY_NUM_ZQ_SEGMENTS_ALPINE_V1);
		reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.ZQ[zq_segment].PR);

		switch (reg_val & DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_MASK) {
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_80OHM:
			*phy_rout = AL_DDR_PHY_ROUT_80OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_68OHM:
			*phy_rout = AL_DDR_PHY_ROUT_68OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_60OHM:
			*phy_rout = AL_DDR_PHY_ROUT_60OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_53OHM:
			*phy_rout = AL_DDR_PHY_ROUT_53OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_48OHM:
			*phy_rout = AL_DDR_PHY_ROUT_48OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_44OHM:
			*phy_rout = AL_DDR_PHY_ROUT_44OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_40OHM:
			*phy_rout = AL_DDR_PHY_ROUT_40OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_37OHM:
			*phy_rout = AL_DDR_PHY_ROUT_37OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_34OHM:
			*phy_rout = AL_DDR_PHY_ROUT_34OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_32OHM:
			*phy_rout = AL_DDR_PHY_ROUT_32OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_30OHM:
			*phy_rout = AL_DDR_PHY_ROUT_30OHM;
			break;
		default:
			al_err("invalid PHY ROUT (%d)!\n", reg_val & DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_MASK);
			return -EINVAL;
		};

		switch (reg_val & DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_MASK) {
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_200OHM:
			*phy_odt = AL_DDR_PHY_ODT_200OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_133OHM:
			*phy_odt = AL_DDR_PHY_ODT_133OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_100OHM:
			*phy_odt = AL_DDR_PHY_ODT_100OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_77OHM:
			*phy_odt = AL_DDR_PHY_ODT_77OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_66OHM:
			*phy_odt = AL_DDR_PHY_ODT_66OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_56OHM:
			*phy_odt = AL_DDR_PHY_ODT_56OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_50OHM:
			*phy_odt = AL_DDR_PHY_ODT_50OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_44OHM:
			*phy_odt = AL_DDR_PHY_ODT_44OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_40OHM:
			*phy_odt = AL_DDR_PHY_ODT_40OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_36OHM:
			*phy_odt = AL_DDR_PHY_ODT_36OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_33OHM:
			*phy_odt = AL_DDR_PHY_ODT_33OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_30OHM:
			*phy_odt = AL_DDR_PHY_ODT_30OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_28OHM:
			*phy_odt = AL_DDR_PHY_ODT_28OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_26OHM:
			*phy_odt = AL_DDR_PHY_ODT_26OHM;
			break;
		case DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_25OHM:
			*phy_odt = AL_DDR_PHY_ODT_25OHM;
			break;
		default:
			al_err("invalid PHY ODT (%d)!\n", reg_val & DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_MASK);
			return -EINVAL;
		};

	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		al_assert(zq_segment < AL_DDR_PHY_NUM_ZQ_SEGMENTS_ALPINE_V2);
		reg_val = al_reg_read32(&phy_regs->alpine_v2.dwc_ddrphy_pub.zq[zq_segment].pr);

		switch (reg_val & ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_MASK) {
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_80OHM:
			*phy_rout = AL_DDR_PHY_ROUT_80OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_68OHM:
			*phy_rout = AL_DDR_PHY_ROUT_68OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_60OHM:
			*phy_rout = AL_DDR_PHY_ROUT_60OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_53OHM:
			*phy_rout = AL_DDR_PHY_ROUT_53OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_48OHM:
			*phy_rout = AL_DDR_PHY_ROUT_48OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_44OHM:
			*phy_rout = AL_DDR_PHY_ROUT_44OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_40OHM:
			*phy_rout = AL_DDR_PHY_ROUT_40OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_37OHM:
			*phy_rout = AL_DDR_PHY_ROUT_37OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_34OHM:
			*phy_rout = AL_DDR_PHY_ROUT_34OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_32OHM:
			*phy_rout = AL_DDR_PHY_ROUT_32OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_30OHM:
			*phy_rout = AL_DDR_PHY_ROUT_30OHM;
			break;
		default:
			al_err("invalid PHY ROUT (%d)!\n",
				reg_val & ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ROUT_MASK);
			return -EINVAL;
		};

		switch (reg_val & ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_MASK) {
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_200OHM:
			*phy_odt = AL_DDR_PHY_ODT_200OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_133OHM:
			*phy_odt = AL_DDR_PHY_ODT_133OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_100OHM:
			*phy_odt = AL_DDR_PHY_ODT_100OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_77OHM:
			*phy_odt = AL_DDR_PHY_ODT_77OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_66OHM:
			*phy_odt = AL_DDR_PHY_ODT_66OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_56OHM:
			*phy_odt = AL_DDR_PHY_ODT_56OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_50OHM:
			*phy_odt = AL_DDR_PHY_ODT_50OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_44OHM:
			*phy_odt = AL_DDR_PHY_ODT_44OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_40OHM:
			*phy_odt = AL_DDR_PHY_ODT_40OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_36OHM:
			*phy_odt = AL_DDR_PHY_ODT_36OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_33OHM:
			*phy_odt = AL_DDR_PHY_ODT_33OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_30OHM:
			*phy_odt = AL_DDR_PHY_ODT_30OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_28OHM:
			*phy_odt = AL_DDR_PHY_ODT_28OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_26OHM:
			*phy_odt = AL_DDR_PHY_ODT_26OHM;
			break;
		case ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_25OHM:
			*phy_odt = AL_DDR_PHY_ODT_25OHM;
			break;
		default:
			al_err("invalid PHY ODT (%d)!\n",
				reg_val & ALPINE_V2_DWC_DDR_PHY_REGS_ZQnPR_ZQDIV_ODT_MASK);
			return -EINVAL;
		};
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
static void al_ddr_phy_training_results_octet_get(
	struct al_ddr_cfg				*ddr_cfg,
	int						octet,
	struct al_ddr_phy_training_results_per_octet	*results)
{
	int rank;
	uint32_t reg_val;
	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)
		ddr_cfg->ddr_phy_regs_base;

	uint32_t gtr;

	if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V1) {
		gtr = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.datx8[octet].GTR);
		reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.datx8[octet].MDLR);
		results->dll_num_taps_init =
			(reg_val & DWC_DDR_PHY_REGS_DXNMDLR_IPRD_MASK) >>
			DWC_DDR_PHY_REGS_DXNMDLR_IPRD_SHIFT;
		results->dll_num_taps_curr =
			(reg_val & DWC_DDR_PHY_REGS_DXNMDLR_TPRD_MASK) >>
			DWC_DDR_PHY_REGS_DXNMDLR_TPRD_SHIFT;

		for (rank = 0; rank < AL_DDR_NUM_RANKS; rank++) {
			reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.datx8[octet].LCDLR[0]);
			results->wld[rank] = ((reg_val >>
				(DWC_DDR_PHY_REGS_DXNLCDLR0_RNWLD_NUM_BITS * rank)) &
				DWC_DDR_PHY_REGS_DXNLCDLR0_RNWLD_MASK) >>
				DWC_DDR_PHY_REGS_DXNLCDLR0_RNWLD_SHIFT;

			results->wld_extra[rank] =
				((gtr >> (DWC_DDR_PHY_REGS_DXNGTR_RNWLSL_NUM_BITS *
				rank)) & DWC_DDR_PHY_REGS_DXNGTR_RNWLSL_MASK) >>
				DWC_DDR_PHY_REGS_DXNGTR_RNWLSL_SHIFT;

			reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.datx8[octet].LCDLR[2]);
			results->rdqsgd[rank] = ((reg_val >>
				(DWC_DDR_PHY_REGS_DXNLCDLR2_RNDQSGD_NUM_BITS*rank)) &
				DWC_DDR_PHY_REGS_DXNLCDLR2_RNDQSGD_MASK) >>
				DWC_DDR_PHY_REGS_DXNLCDLR2_RNDQSGD_SHIFT;

			results->rdqsgd_extra[rank] = ((gtr >>
				(DWC_DDR_PHY_REGS_DXNGTR_RNDGSL_NUM_BITS*rank)) &
				DWC_DDR_PHY_REGS_DXNGTR_RNDGSL_MASK) >>
				DWC_DDR_PHY_REGS_DXNGTR_RNDGSL_SHIFT;
		}

		reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.datx8[octet].LCDLR[1]);
		results->wdqd = (reg_val & DWC_DDR_PHY_REGS_DXNLCDLR1_WDQD_MASK) >>
			DWC_DDR_PHY_REGS_DXNLCDLR1_WDQD_SHIFT;
		results->rdqsd = (reg_val & DWC_DDR_PHY_REGS_DXNLCDLR1_RDQSD_MASK) >>
			DWC_DDR_PHY_REGS_DXNLCDLR1_RDQSD_SHIFT;
		results->rdqsnd = (reg_val & DWC_DDR_PHY_REGS_DXNLCDLR1_RDQSND_MASK) >>
			DWC_DDR_PHY_REGS_DXNLCDLR1_RDQSND_SHIFT;

		al_reg_write32_masked(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.DTCR,
			DWC_DDR_PHY_REGS_DTCR_DTDBS_MASK,
			DWC_DDR_PHY_REGS_DTCR_DTDBS(octet));

		reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.DTEDR[0]);
		results->dtwlmn = (reg_val & DWC_DDR_PHY_REGS_DTEDR0_DTWLMN_MASK) >>
			DWC_DDR_PHY_REGS_DTEDR0_DTWLMN_SHIFT;
		results->dtwlmx = (reg_val & DWC_DDR_PHY_REGS_DTEDR0_DTWLMX_MASK) >>
			DWC_DDR_PHY_REGS_DTEDR0_DTWLMX_SHIFT;
		results->dtwbmn = (reg_val & DWC_DDR_PHY_REGS_DTEDR0_DTWBMN_MASK) >>
			DWC_DDR_PHY_REGS_DTEDR0_DTWBMN_SHIFT;
		results->dtwbmx = (reg_val & DWC_DDR_PHY_REGS_DTEDR0_DTWBMX_MASK) >>
			DWC_DDR_PHY_REGS_DTEDR0_DTWBMX_SHIFT;

		reg_val = al_reg_read32(&phy_regs->alpine_v1.dwc_ddrphy_pub.DTEDR[1]);
		results->dtrlmn = (reg_val & DWC_DDR_PHY_REGS_DTEDR1_DTRLMN_MASK) >>
			DWC_DDR_PHY_REGS_DTEDR1_DTRLMN_SHIFT;
		results->dtrlmx = (reg_val & DWC_DDR_PHY_REGS_DTEDR1_DTRLMX_MASK) >>
			DWC_DDR_PHY_REGS_DTEDR1_DTRLMX_SHIFT;
		results->dtrbmn = (reg_val & DWC_DDR_PHY_REGS_DTEDR1_DTRBMN_MASK) >>
			DWC_DDR_PHY_REGS_DTEDR1_DTRBMN_SHIFT;
		results->dtrbmx = (reg_val & DWC_DDR_PHY_REGS_DTEDR1_DTRBMX_MASK) >>
			DWC_DDR_PHY_REGS_DTEDR1_DTRBMX_SHIFT;
	} else if (ddr_cfg->rev == AL_DDR_REV_ID_ALPINE_V2) {
		/* TODO */
	}

}

/*******************************************************************************
 ******************************************************************************/
void al_ddr_phy_training_results_get(
	struct al_ddr_cfg			*ddr_cfg,
	struct al_ddr_phy_training_results	*results)
{
	int octet;

	for (octet = 0; octet < AL_DDR_PHY_NUM_BYTE_LANES; octet++) {
		al_ddr_phy_training_results_octet_get(
			ddr_cfg, octet, &results->octets[octet]);
	}
}

/*******************************************************************************
 ******************************************************************************/
static int al_ddr_rev_get(
	void __iomem		*ddr_phy_regs_base,
	enum al_ddr_revision	*rev)
{
	union al_ddr_phy_regs __iomem *phy_regs =
		(union al_ddr_phy_regs __iomem *)
		ddr_phy_regs_base;

	/* PHY version is always the first register in PHY */
	uint32_t phy_version = al_reg_read32(
			&phy_regs->alpine_v1.dwc_ddrphy_pub.RIDR);


	if (phy_version == AL_DDR_PHY_VERSION_ALPINE_V1)
		*rev = AL_DDR_REV_ID_ALPINE_V1;
	else if (phy_version == AL_DDR_PHY_VERSION_ALPINE_V2)
		*rev = AL_DDR_REV_ID_ALPINE_V2;
	else {
		al_err("%s: Unknown DDR rev %08x\n", __func__, phy_version);
		return -EIO;
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_cfg_init(
	void __iomem		*nb_regs_base,
	void __iomem		*ddr_ctrl_regs_base,
	void __iomem		*ddr_phy_regs_base,
	struct al_ddr_cfg	*ddr_cfg)
{
	int err;

	ddr_cfg->nb_regs_base = nb_regs_base;
	ddr_cfg->ddr_ctrl_regs_base = ddr_ctrl_regs_base;
	ddr_cfg->ddr_phy_regs_base = ddr_phy_regs_base;
	err = al_ddr_rev_get(ddr_phy_regs_base, &ddr_cfg->rev);
	if (err) {
		al_err(
			"%s:%d: al_ddr_cfg_init failed!\n",
			__func__,
			__LINE__);
		return err;
	}

	return 0;
}

/*******************************************************************************
 ******************************************************************************/
int al_ddr_mpr_get(
	struct al_ddr_cfg	*ddr_cfg,
	unsigned int		rank_num,
	unsigned int		device_num,
	unsigned int		page_num,
	unsigned int		*page_data)
{
	union al_ddr_ctrl_regs __iomem *ctrl_regs =
		(union al_ddr_ctrl_regs __iomem *)
		ddr_cfg->ddr_ctrl_regs_base;
	struct al_nb_regs __iomem *nb_regs =
		(struct al_nb_regs __iomem *)ddr_cfg->nb_regs_base;
	int i;
	int err;
	unsigned int mr3;
	unsigned int ranks = 1 << rank_num;
	unsigned int temp_reg;
	unsigned int byte_offset = device_num & 0x3;
	*page_data = 0;
	unsigned int crc_parity_retry_enable;

	/*
	* Addressing RMN: 5006
	*
	* RMN description:
	* In ECC Configurations uMCTL2 Incorrectly Returns MRR Data on HIF Read Data
	* Software flow:
	* before performing MRR/MPR (read) through the software, disable reads and writes
	* from being issued on the DFI by setting DBG1.dis_dq = 1. After finishing MPR,
	* re-enable reads and writes being issued on the DFI by setting DBG1.dis_dq = 0.
	*/

	/* disable reads/writes */
	err = al_ddr_ctrl_read_write_disable(ddr_cfg);
	if (err) {
		al_err("%s: al_ddr_ctrl_read_write_dis failed!\n", __func__);
		return err;
	}

	/* TODO : disable DBI */

	/* TODO : should poll STAT.cmd_in_err_window if CRC retry is
	enabled and not handling CRC error currently */


	/*
	* Addressing RMN: 5247
	*
	* RMN description:
	* DDR4 Retry: Controller can Issue Precharge-all in MPR Mode
	* Software flow:
	* Disable crc_retry before entering MPR mode
	*/
	crc_parity_retry_enable = al_reg_read32(&ctrl_regs->alpine_v2.umctl2_regs.crcparctl1) &
				DWC_DDR_UMCTL2_REGS_CRCPARCTL1_CRC_PARITY_RETRY_ENABLE;
	al_reg_write32_masked(
		&ctrl_regs->alpine_v2.umctl2_regs.crcparctl1,
		DWC_DDR_UMCTL2_REGS_CRCPARCTL1_CRC_PARITY_RETRY_ENABLE,
		0);

	/* Enable MPR access */
	mr3 = al_ddr_mode_register_get(ddr_cfg, 3);
	AL_REG_CLEAR_AND_SET(mr3,
			(AL_DDR_JEDEC_DDR4_MR3_MPR_PAGE_SEL_MASK |
			 AL_DDR_JEDEC_DDR4_MR3_MPR_OPER_MASK |
			 AL_DDR_JEDEC_DDR4_MR3_MPR_RD_FORMAT_MASK),
			((page_num << AL_DDR_JEDEC_DDR4_MR3_MPR_PAGE_SEL_SHIFT) |
			 AL_DDR_JEDEC_DDR4_MR3_MPR_OPER_MPR |
			 AL_DDR_JEDEC_DDR4_MR3_MPR_RD_FORMAT_SERIAL));

	err = al_ddr_mode_register_set(ddr_cfg, ranks, 3, mr3);
	if (err) {
		al_err("%s: al_ddr_mode_register_set failed!\n", __func__);
		return err;
	}

	/* Read MPR page */
	for (i = 0; i < 4; i++) {
		err = al_ddr_reg_poll32(
			CTRL_REG(mrstat),
			DWC_DDR_UMCTL2_REGS_MRSTAT_MR_WR_BUSY,
			0,
			DEFAULT_TIMEOUT);

		if (err) {
			al_err("%s: al_ddr_reg_poll32 failed!\n", __func__);
			return err;
		}

		al_reg_write32(
			CTRL_REG(mrctrl0),
			((ranks << DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_RANK_SHIFT) |
			(i << DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_ADDR_SHIFT) |
			DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_TYPE_READ |
			DWC_DDR_UMCTL2_REGS_MRCTRL0_MPR_EN));

		al_reg_write32(
			CTRL_REG(mrctrl1),
			((1 << 12) << DWC_DDR_UMCTL2_REGS_MRCTRL1_MR_DATA_SHIFT));

		/* Trigger MPR read */
		al_reg_write32_masked(
			CTRL_REG(mrctrl0),
			DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_WR,
			DWC_DDR_UMCTL2_REGS_MRCTRL0_MR_WR);

		/* read data from register */
		err = al_ddr_reg_poll32(
			&nb_regs->dram_channels.mrr_control_status,
			(1 << 0),
			(1 << 0),
			DEFAULT_TIMEOUT);

		if (err) {
			al_err("%s: al_ddr_reg_poll32 failed!\n", __func__);
			return err;
		}

		temp_reg = al_reg_read32(&nb_regs->ddr_0_mrr[device_num >> 2].val);
		temp_reg = AL_REG_FIELD_GET(temp_reg, 0xff << byte_offset, byte_offset*8);
		*page_data |= (temp_reg << 8*i);

		al_reg_write32(
			&nb_regs->dram_channels.mrr_control_status,
			(1 << 16));
		al_reg_write32(
			&nb_regs->dram_channels.mrr_control_status,
			(0 << 16));

		al_data_memory_barrier();
	}

	/* return DRAM access */
	AL_REG_CLEAR_AND_SET(
			mr3,
			AL_DDR_JEDEC_DDR4_MR3_MPR_OPER_MASK,
			AL_DDR_JEDEC_DDR4_MR3_MPR_OPER_NORM);
	err = al_ddr_mode_register_set(ddr_cfg, ranks, 3, mr3);
	if (err) {
		al_err("%s: al_ddr_mode_register_set failed!\n", __func__);
		return err;
	}

	al_data_memory_barrier();
	err = al_ddr_reg_poll32(
		CTRL_REG(mrstat),
		DWC_DDR_UMCTL2_REGS_MRSTAT_MR_WR_BUSY,
		0,
		DEFAULT_TIMEOUT);

	if (err) {
		al_err("%s: al_ddr_reg_poll32 failed!\n", __func__);
		return err;
	}

	/* re-enable crc retry */
	al_reg_write32_masked(
		&ctrl_regs->alpine_v2.umctl2_regs.crcparctl1,
		DWC_DDR_UMCTL2_REGS_CRCPARCTL1_CRC_PARITY_RETRY_ENABLE,
		crc_parity_retry_enable);

	/* TODO : re-enable DBI */

	/* re-enable reads/writes*/
	al_ddr_ctrl_read_write_enable(ddr_cfg);

	return 0;
}

/** @} end of DDR group */

