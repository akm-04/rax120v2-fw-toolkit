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
 * @defgroup group_tdm	TDM
 *  @{
 *
 * @file   al_hal_tdm.h
 * @brief Header file for the TDM HAL driver
 *
 */

#include "al_hal_tdm_regs.h"
#include "al_hal_tdm.h"
#include "al_hal_reg_utils.h"

#define FIFO_SIZE	4096 /* Bytes */

/******************************************************************************
 ******************************************************************************/
void al_tdm_handle_init(
	struct al_tdm	*tdm,
	void __iomem	*cfg_regs,
	void __iomem	*tx_fifo_regs,
	void __iomem	*rx_fifo_regs)
{
	al_dbg(
		"%s(%p, %p, %p, %p)\n", __func__,
		tdm, cfg_regs, tx_fifo_regs, rx_fifo_regs);

	al_assert(tdm);
	al_assert(cfg_regs);
	al_assert(tx_fifo_regs);
	al_assert(rx_fifo_regs);

	tdm->cfg_regs = cfg_regs;
	tdm->tx_fifo_regs = tx_fifo_regs;
	tdm->rx_fifo_regs = rx_fifo_regs;

	tdm->tx_fifo_head = 0;
	tdm->tx_fifo_head_pending_samples = 0;
	tdm->rx_fifo_tail = 0;
	tdm->rx_fifo_tail_pending_samples = 0;
}

/******************************************************************************
 ******************************************************************************/
static int al_tdm_config_xmit_ctrl(
	struct al_tdm_cfg_xmit_ctrl	*cfg,
	uint32_t			*ctl_reg,
	uint32_t			*sa0_reg,
	uint32_t			*sa1_reg,
	uint32_t			*sa2_reg,
	uint32_t			*sa3_reg,
	uint32_t			*limits_reg,
	unsigned int			*sample_num_bytes)
{
	switch (cfg->sample_size) {
	case AL_TDM_SAMPLE_SIZE_8_BITS:
		*sample_num_bytes = 1;
		break;
	case AL_TDM_SAMPLE_SIZE_16_BITS:
		*sample_num_bytes = 2;
		break;
	case AL_TDM_SAMPLE_SIZE_24_BITS:
		*sample_num_bytes = 3;
		break;
	case AL_TDM_SAMPLE_SIZE_32_BITS:
		*sample_num_bytes = 4;
		break;
	default:
		al_err("%s: invalid sample size!\n", __func__);
		return -EINVAL;
	}

	al_assert(cfg->en_len > 0);

	al_reg_write32(
		ctl_reg,
		(cfg->data_delay << TDM_TDM_CFG_TX_CTL_TXD_DELAY_SHIFT) |
		((cfg->data_inv) ? TDM_TDM_CFG_TX_CTL_TXD_INV : 0) |
		((cfg->data_edge == AL_TDM_EDGE_RISING) ? TDM_TDM_CFG_TX_CTL_TXD_EDGE : 0) |
		(cfg->en_delay << TDM_TDM_CFG_TX_CTL_TX_EN_DELAY_SHIFT) |
		((cfg->en_len - 1) << TDM_TDM_CFG_TX_CTL_TX_EN_LEN_SHIFT) |
		((cfg->en_inv) ? TDM_TDM_CFG_TX_CTL_TX_EN_INV : 0) |
		((cfg->en_edge == AL_TDM_EDGE_RISING) ? TDM_TDM_CFG_TX_CTL_TX_EN_EDGE : 0) |
		(cfg->sample_size << TDM_TDM_CFG_TX_CTL_TX_SMPL_SIZE_SHIFT) |
		((cfg->sample_alignment == AL_TDM_SAMPLE_ALIGNMENT_START) ? TDM_TDM_CFG_TX_CTL_TX_SMPL_ALIGN : 0) |
		((cfg->data_is_padded) ? TDM_TDM_CFG_TX_CTL_TX_HOST_PADDING : 0) |
		((cfg->data_alignment == AL_TDM_DATA_ALIGNMENT_MSB) ? TDM_TDM_CFG_TX_CTL_TX_HOST_ALIGNMENT : 0) |
		((cfg->en_special = AL_TDM_CHAN_EN_SPECIAL_11) ? TDM_TDM_CFG_TX_CTL_TX_EN_SPECIAL : 0) |
		((cfg->auto_track) ? TDM_TDM_CFG_TX_CTL_TX_AUTO_TRACK : 0));
	al_reg_write32(
		sa0_reg,
		cfg->active_slots_31_0);
	al_reg_write32(
		sa1_reg,
		cfg->active_slots_63_32);
	al_reg_write32(
		sa2_reg,
		cfg->active_slots_95_64);
	al_reg_write32(
		sa3_reg,
		cfg->active_slots_127_96);
	al_reg_write32(
		limits_reg,
		((FIFO_SIZE / (*sample_num_bytes)) << TDM_TDM_CFG_TX_LIMITS_MAX_SHIFT) |
		(cfg->fifo_empty_threshold << TDM_TDM_CFG_TX_LIMITS_ALMOST_EMPTY_SHIFT));

	return 0;
}

/******************************************************************************
 ******************************************************************************/
int al_tdm_config(
	struct al_tdm		*tdm,
	struct al_tdm_cfg	*cfg)
{
	int err;

	al_dbg("%s(%p, %p)\n", __func__, tdm, cfg);

	al_assert(tdm);
	al_assert(cfg);

	al_reg_write32(
		&tdm->cfg_regs->slot,
		(cfg->num_slots_log2 << TDM_TDM_CFG_SLOT_NUM_SHIFT) |
		(cfg->slot_size << TDM_TDM_CFG_SLOT_SIZE_SHIFT) |
		((cfg->zsi_en) ? TDM_TDM_CFG_SLOT_ZSI_EN : 0));

	al_reg_write32(
		&tdm->cfg_regs->pcm_clk_gen,
		((cfg->pcm_clk_src == AL_TDM_PCM_CLK_SRC_INTERNAL) ? TDM_TDM_CFG_PCM_CLK_GEN_PCM_CLK_SOURCE : 0) |
		((cfg->pcm_tune_dynamic) ? TDM_TDM_CFG_PCM_CLK_GEN_PCM_TUNE_DYNAMIC : 0) |
		(cfg->pcm_tune_period << TDM_TDM_CFG_PCM_CLK_GEN_PCM_TUNE_PERIOD_SHIFT) |
		(cfg->pcm_clk_ratio << TDM_TDM_CFG_PCM_CLK_GEN_PCM_CLK_RATIO_SHIFT) |
		(cfg->pcm_tune_delta << TDM_TDM_CFG_PCM_CLK_GEN_PCM_TUNE_DELTA_SHIFT) |
		(cfg->pcm_tune_rnd_up ? TDM_TDM_CFG_PCM_CLK_GEN_PCM_TUNE_RND_DIR : 0) |
		(cfg->pcm_tune_keep ? TDM_TDM_CFG_PCM_CLK_GEN_PCM_TUNE_KEEP : 0));

	al_reg_write32(
		&tdm->cfg_regs->pcm_window,
		(cfg->window_frame_ratio << TDM_TDM_CFG_PCM_WINDOW_WINDOW_FRAME_RATIO_SHIFT) |
		(cfg->window_hyst_start << TDM_TDM_CFG_PCM_WINDOW_WINDOW_HYST_START_SHIFT) |
		(cfg->window_hyst_level << TDM_TDM_CFG_PCM_WINDOW_WINDOW_HYST_LEVEL_SHIFT));

	al_assert(cfg->fsync_len > 0);
	al_reg_write32(
		&tdm->cfg_regs->fsync_ctl,
		((cfg->fsync_edge == AL_TDM_EDGE_RISING) ? TDM_TDM_CFG_FSYNC_CTL_FSYNC_EDGE : 0) |
		((cfg->fsync_inv) ? TDM_TDM_CFG_FSYNC_CTL_INV : 0) |
		((cfg->fsync_len - 1) << TDM_TDM_CFG_FSYNC_CTL_LEN_SHIFT));

	err = al_tdm_config_xmit_ctrl(
		&cfg->tx_ctrl,
		&tdm->cfg_regs->tx_ctl,
		&tdm->cfg_regs->tx_sa_0,
		&tdm->cfg_regs->tx_sa_1,
		&tdm->cfg_regs->tx_sa_2,
		&tdm->cfg_regs->tx_sa_3,
		&tdm->cfg_regs->tx_limits,
		&tdm->tx_sample_num_bytes);
	if (err) {
		al_err("%s: al_tdm_config_xmit_ctrl failed!\n", __func__);
		return err;
	}

	al_reg_write32_masked(
		&tdm->cfg_regs->tx_ctl,
		TDM_TDM_CFG_TX_CTL_TX_PADDING_DATA |
		TDM_TDM_CFG_TX_CTL_TXD_DRV_INACTIVE,
		((cfg->tx_specific_ctrl.tx_padding_data ==
		  AL_TDM_TX_PADDING_DATA_0) ?
		 0 : TDM_TDM_CFG_TX_CTL_TX_PADDING_DATA) |
		((cfg->tx_specific_ctrl.tx_inactive_data ==
		  AL_TDM_TX_INACTIVE_DATA_USER) ?
		 TDM_TDM_CFG_TX_CTL_TXD_DRV_INACTIVE : 0));

	al_reg_write32(
		&tdm->cfg_regs->tx_uf_data,
		cfg->tx_specific_ctrl.tx_underflow_data_user_val);

	al_reg_write32(
		&tdm->cfg_regs->tx_na_data,
		cfg->tx_specific_ctrl.tx_inactive_data_user_val);

	err = al_tdm_config_xmit_ctrl(
		&cfg->rx_ctrl,
		&tdm->cfg_regs->rx_ctl,
		&tdm->cfg_regs->rx_sa_0,
		&tdm->cfg_regs->rx_sa_1,
		&tdm->cfg_regs->rx_sa_2,
		&tdm->cfg_regs->rx_sa_3,
		&tdm->cfg_regs->rx_limits,
		&tdm->rx_sample_num_bytes);
	if (err) {
		al_err("%s: al_tdm_config_xmit_ctrl failed!\n", __func__);
		return err;
	}

	al_reg_write32(
		&tdm->cfg_regs->misc,
		((cfg->parity_en) ? TDM_TDM_CFG_MISC_PARITY_EN : 0) |
		((cfg->slverr_en) ? TDM_TDM_CFG_MISC_SLVERR_EN : 0));

	tdm->tx_fifo_auto_track = cfg->tx_ctrl.auto_track;
	tdm->rx_fifo_auto_track = cfg->rx_ctrl.auto_track;

	return 0;
}

/******************************************************************************
 ******************************************************************************/
void al_tdm_set_enables(
	struct al_tdm	*tdm,
	al_bool		enable,
	al_bool		stream_enable,
	al_bool		rx_stream_enable,
	al_bool		tx_stream_enable)
{
	al_dbg(
		"%s(%p, %d, %d, %d)\n", __func__,
		tdm, stream_enable, rx_stream_enable, tx_stream_enable);

	al_assert(tdm);

	al_reg_write32(
		&tdm->cfg_regs->tdmenable,
		((enable) ? TDM_TDM_CFG_TDMENABLE_ENABLE : 0) |
		((stream_enable) ? TDM_TDM_CFG_TDMENABLE_STREAM_ENABLE : 0) |
		((rx_stream_enable) ? TDM_TDM_CFG_TDMENABLE_RX_STREAM_EN : 0) |
		((tx_stream_enable) ? TDM_TDM_CFG_TDMENABLE_TX_STREAM_EN : 0));
}

/******************************************************************************
 ******************************************************************************/
void al_tdm_tx_fifo_status_get(
	struct al_tdm			*tdm,
	struct al_tdm_tx_fifo_status	*status)
{
	uint32_t reg_val;

	al_dbg("%s(%p, %p)\n", __func__, tdm, status);

	al_assert(tdm);
	al_assert(status);

	reg_val = al_reg_read32(&tdm->cfg_regs->txstatus);
	status->underrun = (reg_val & TDM_TDM_CFG_TXSTATUS_TX_UNDERRUN) ?
		AL_TRUE : AL_FALSE;
	status->almost_empty = (reg_val & TDM_TDM_CFG_TXSTATUS_TX_AEMPTY) ?
		AL_TRUE : AL_FALSE;
	status->num_samples_vacant = (FIFO_SIZE / tdm->tx_sample_num_bytes) -
		((reg_val & TDM_TDM_CFG_TXSTATUS_TX_INUSE_MASK) >>
		 TDM_TDM_CFG_TXSTATUS_TX_INUSE_SHIFT);

	al_dbg(
		"%s(%p, %p) --> underrun = %d, almost_empty = %d, num_samples_vacant = %d\n",
		__func__, tdm, status, status->underrun, status->almost_empty,
		status->num_samples_vacant);
}

/******************************************************************************
 ******************************************************************************/
void al_tdm_tx_fifo_clear(
	struct al_tdm *tdm)
{
	al_dbg("%s(%p)\n", __func__, tdm);

	al_assert(tdm);

	al_reg_write32(&tdm->cfg_regs->tdmclear, TDM_TDM_CFG_TDMCLEAR_TX_CLEAR);
}

/******************************************************************************
 ******************************************************************************/
void al_tdm_rx_fifo_status_get(
	struct al_tdm			*tdm,
	struct al_tdm_rx_fifo_status	*status)
{
	uint32_t reg_val;

	al_dbg("%s(%p, %p)\n", __func__, tdm, status);

	al_assert(tdm);
	al_assert(status);

	reg_val = al_reg_read32(&tdm->cfg_regs->rxstatus);
	status->overrun = (reg_val & TDM_TDM_CFG_RXSTATUS_RX_OVERRUN) ?
		AL_TRUE : AL_FALSE;
	status->enough_avail = (reg_val & TDM_TDM_CFG_RXSTATUS_RX_AFULL) ?
		AL_TRUE : AL_FALSE;
	status->num_samples_avail =
		(reg_val & TDM_TDM_CFG_RXSTATUS_TX_INUSE_MASK) >>
		TDM_TDM_CFG_RXSTATUS_TX_INUSE_SHIFT;

	al_dbg(
		"%s(%p, %p) --> overrun = %d, enough_avail = %d, num_samples_avail = %d\n",
		__func__, tdm, status, status->overrun, status->enough_avail,
		status->num_samples_avail);
}

/******************************************************************************
 ******************************************************************************/
void al_tdm_rx_fifo_clear(
	struct al_tdm *tdm)
{
	al_dbg("%s(%p)\n", __func__, tdm);

	al_assert(tdm);

	al_reg_write32(&tdm->cfg_regs->tdmclear, TDM_TDM_CFG_TDMCLEAR_RX_CLEAR);
}

/******************************************************************************
 ******************************************************************************/
void al_tdm_tx_fifo_samples_add(
	struct al_tdm	*tdm,
	void		*sample_data,
	unsigned int	num_samples,
	al_bool		head_inc)
{
	unsigned int size_remaining;
	unsigned int size_till_wrap_around;
	unsigned int size_current;

	al_dbg("%s(%p, %p, %u, %d)\n", __func__,
		tdm, sample_data, num_samples, head_inc);

	al_assert(tdm);
	al_assert(sample_data);
	al_assert(num_samples > 0);

	size_remaining = num_samples * tdm->tx_sample_num_bytes;

	al_assert(size_remaining <= FIFO_SIZE);

	size_till_wrap_around = FIFO_SIZE - tdm->tx_fifo_head;
	size_current = (size_remaining <= size_till_wrap_around) ?
		size_remaining : size_till_wrap_around;

	al_memcpy(tdm->tx_fifo_regs + tdm->tx_fifo_head, sample_data,
		size_current);
	tdm->tx_fifo_head = (tdm->tx_fifo_head + size_current) % FIFO_SIZE;
	size_remaining -= size_current;

	if (size_remaining) {
		al_memcpy(tdm->tx_fifo_regs, sample_data + size_current,
			size_remaining);
		tdm->tx_fifo_head = size_remaining;
	}

	if (!tdm->tx_fifo_auto_track) {
		tdm->tx_fifo_head_pending_samples += num_samples;
		if (head_inc) {
			al_reg_write32(&tdm->cfg_regs->tx_inc,
				tdm->tx_fifo_head_pending_samples);
			tdm->tx_fifo_head_pending_samples = 0;
		}
	}
}

/******************************************************************************
 ******************************************************************************/
void al_tdm_rx_fifo_samples_fetch(
	struct al_tdm	*tdm,
	void		*sample_data,
	unsigned int	num_samples,
	al_bool		tail_inc)
{
	unsigned int size_remaining;
	unsigned int size_till_wrap_around;
	unsigned int size_current;

	al_dbg("%s(%p, %p, %u, %d)\n", __func__,
		tdm, sample_data, num_samples, tail_inc);

	al_assert(tdm);
	al_assert(sample_data);
	al_assert(num_samples > 0);

	size_remaining = num_samples * tdm->rx_sample_num_bytes;

	al_assert(size_remaining <= FIFO_SIZE);

	size_till_wrap_around = FIFO_SIZE - tdm->rx_fifo_tail;
	size_current = (size_remaining <= size_till_wrap_around) ?
		size_remaining : size_till_wrap_around;

	al_memcpy(sample_data, tdm->rx_fifo_regs + tdm->rx_fifo_tail,
		size_current);
	tdm->rx_fifo_tail = (tdm->rx_fifo_tail + size_current) % FIFO_SIZE;
	size_remaining -= size_current;

	if (size_remaining) {
		al_memcpy(sample_data + size_current, tdm->rx_fifo_regs,
			size_remaining);
		tdm->rx_fifo_tail = size_remaining;
	}

	if (!tdm->tx_fifo_auto_track) {
		tdm->rx_fifo_tail_pending_samples += num_samples;
		if (tail_inc) {
			al_reg_write32(&tdm->cfg_regs->rx_inc,
				tdm->rx_fifo_tail_pending_samples);
			tdm->rx_fifo_tail_pending_samples = 0;
		}
	}
}

/** @} end of TDM group */
