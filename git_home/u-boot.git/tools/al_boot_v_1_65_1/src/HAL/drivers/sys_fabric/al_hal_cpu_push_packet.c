/*******************************************************************************
Copyright (C) 2014 Annapurna Labs Ltd.

This file is licensed under the terms of the Annapurna Labs' Commercial License
Agreement distributed with the file or available on the software download site.
Recipient shall use the content of this file only on semiconductor devices or
systems developed by or for Annapurna Labs.

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
* @{
* @file   al_hal_cpu_push_packet.c
*
* @brief  includes Push Packet HAL implementation
*
*/

#include "al_hal_cpu_push_packet.h"

int al_cpu_push_packet_common_init(
		struct al_cpu_push_packet_common	*common,
		void __iomem				*nb_regs_base,
		al_phys_addr_t				phy_base,
		al_bool					decerr_en,
		al_bool					slverr_en,
		al_bool					par_gen_en,
		al_bool					sel_8k,
		uint32_t				sel_awuser)
{
	struct al_nb_regs __iomem *nb_regs = (struct al_nb_regs __iomem *)nb_regs_base;
	uint32_t reg;
	uint32_t phy_base_high = (uint32_t)((phy_base >> 32) & 0xffffffff);
	uint32_t phy_base_low = (uint32_t)(phy_base & 0xffffffff);

	common->nb_regs = nb_regs;
	common->phy_base = phy_base;
	common->decerr_en = decerr_en;
	common->slverr_en = slverr_en;
	common->par_gen_en = par_gen_en;
	common->sel_8k = sel_8k;
	common->sel_awuser = sel_awuser;

	reg = al_reg_read32(&nb_regs->push_packet.pp_config);

	/* Remove bypass configuration */
	AL_REG_MASK_CLEAR(reg, NB_PUSH_PACKET_PP_CONFIG_FM_BYPASS);
	AL_REG_MASK_CLEAR(reg, NB_PUSH_PACKET_PP_CONFIG_BYPASS);

	if (common->decerr_en)
		AL_REG_MASK_SET(reg, NB_PUSH_PACKET_PP_CONFIG_DECERR_EN);
	else
		AL_REG_MASK_CLEAR(reg, NB_PUSH_PACKET_PP_CONFIG_DECERR_EN);

	if (common->slverr_en)
		AL_REG_MASK_SET(reg, NB_PUSH_PACKET_PP_CONFIG_SLVERR_EN);
	else
		AL_REG_MASK_CLEAR(reg, NB_PUSH_PACKET_PP_CONFIG_SLVERR_EN);

	if (common->par_gen_en)
		AL_REG_MASK_SET(reg, NB_PUSH_PACKET_PP_CONFIG_PAR_GEN_EN);
	else
		AL_REG_MASK_CLEAR(reg, NB_PUSH_PACKET_PP_CONFIG_PAR_GEN_EN);

	if (common->sel_8k)
		AL_REG_MASK_SET(reg, NB_PUSH_PACKET_PP_CONFIG_SEL_8K);
	else
		AL_REG_MASK_CLEAR(reg, NB_PUSH_PACKET_PP_CONFIG_SEL_8K);


	/* Disable channels */
	AL_REG_MASK_CLEAR(reg, NB_PUSH_PACKET_PP_CONFIG_CHANNEL_ENABLE_MASK);

	al_reg_write32(&nb_regs->push_packet.pp_config, reg);

	/* Set Push Packet base address */
	al_reg_write32(&nb_regs->push_packet.pp_base_low, phy_base_low);
	al_reg_write32(&nb_regs->push_packet.pp_base_high, phy_base_high);

	/* Set Push Packet Awuser select */
	al_reg_write32(&nb_regs->push_packet.pp_sel_awuser, sel_awuser);

	return 0;
}

int al_cpu_push_packet_init(
		struct al_cpu_push_packet		*push_packet,
		struct al_cpu_push_packet_common	*common,
		unsigned int				channel_id)
{
	enum al_cpu_push_packet_channel_id max_channel_id;
	al_phys_addr_t reg_phy_base;
	al_phys_addr_t data_phy_base;

	max_channel_id = AL_PUSH_PACKET_CHANNEL_ID_1;

	if (channel_id > max_channel_id) {
		al_err("%s: invalid channel id %d\n", __func__, channel_id);
		return -EINVAL;
	}

	reg_phy_base = common->phy_base +
			(common->sel_8k ? (channel_id << 13) : (channel_id << 17));
	data_phy_base = reg_phy_base +
			(common->sel_8k ? (1 << 12) : (1 << 16));

	push_packet->common = common;
	push_packet->nb_regs = common->nb_regs;
	push_packet->channel_id = channel_id;
	push_packet->reg_phy_base = reg_phy_base;
	push_packet->data_phy_base = data_phy_base;


	/* disable push packet, in case it's currently enabled */
	al_cpu_push_packet_enable(push_packet, AL_FALSE);

	al_dbg("push packet handle initialized. channel_id id: %d\n",
	       channel_id);

	return 0;
}

void al_cpu_push_packet_virt_addr_set(
		struct al_cpu_push_packet	*push_packet,
		void				*reg_addr)
{
	struct al_cpu_push_packet_channel __iomem *channel_regs =
		(struct al_cpu_push_packet_channel __iomem *)reg_addr;

	push_packet->channel_regs = channel_regs;
}


int al_cpu_push_packet_enable(
		struct al_cpu_push_packet	*push_packet,
		al_bool				enable)
{
	al_reg_write32_masked(&push_packet->nb_regs->push_packet.pp_config,
		NB_PUSH_PACKET_PP_CONFIG_CHANNEL_ENABLE(push_packet->channel_id),
		(enable ?
		 NB_PUSH_PACKET_PP_CONFIG_CHANNEL_ENABLE(push_packet->channel_id) :
		 0));

	return 0;
}
