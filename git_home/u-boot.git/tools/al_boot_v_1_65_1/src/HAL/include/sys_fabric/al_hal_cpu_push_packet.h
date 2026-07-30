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
 * @defgroup group_cpu_push_packet Push Packet
 * @ingroup group_sys_fabric
 *  @{
 *  The Push Packet HAL can be used to configure the Push Packet feature
 *  in the system fabric.
 *  Push packet may be used by the CPU as a pseudo DMA. It allows controlling
 *  different transactions parameters like size, VMID, etc., allowing creating
 *  big packet transactions from CPU to a certain function in PCIe.
 *
 * Common operation example:
 * @code
 * struct al_cpu_push_packet_common common;
 * struct al_cpu_push_packet push_packet;
 * al_phys_addr_t phy_base;
 * void *base;
 * void* data_ptr;
 * void* data;
 * uint32_t attr;
 *
 * int init()
 * {
 *	// Initialization of common parameters for all Push packet channels.
 *	al_cpu_push_packet_common_init(
 *		&common,
 *		nb_regs_base,
 *		phy_base,
 *		base,
 *		AL_FALSE,
 *		AL_TRUE,
 *		AL_TRUE,
 *		AL_FALSE,
 *		0x0);
 *
 *	// Initialize handle for channel 0
 *	al_cpu_push_packet_init(
 *		&push_packet,
 *		&common,
 *		AL_PUSH_PACKET_CHANNEL_ID_0);
 *
 *	// Set push packet channel registers virtual address
 *	void al_cpu_push_packet_virt_addr_set(
 *		&push_packet,
 *		phy_to_virt(push_packet.reg_phy_base));
 *
 *	// Enable channel
 *	al_cpu_push_packet_enable(
 *		&push_packet,
 *		AL_TRUE);
 *
 *	// Create packet (data) and get physical address
 *	// for transmission (phy_addr)
 *
 *	// Get address for data buffer
 *	data_ptr = al_cpu_push_packet_data_buffer_addr_get(
 *		&push_packet,
 *		phy_addr,
 *		num_bytes)
 *
 *	// Copy packet data to data buffer
 *	memcpy(data_ptr,data,num_bytes);
 *
 *	// Set packet attribute (if different from previous packet)
 *	al_cpu_push_packet_attr_set(
 *		&push_packet,
 *		attr)
 *
 *	// Trigger packet transmission
 *	al_cpu_push_packet_send(
 *		&push_packet,
 *		phy_addr)
 * }
 * @endcode
 * @file   al_hal_cpu_push_packet.h
 *
 * @brief  includes Push Packet API
 *
 */

#ifndef __AL_HAL_PUSH_PACKET_H__
#define __AL_HAL_PUSH_PACKET_H__

#include "al_hal_common.h"
#include "al_hal_nb_regs.h"

enum al_cpu_push_packet_channel_id {
	AL_PUSH_PACKET_CHANNEL_ID_0 = 0,
	AL_PUSH_PACKET_CHANNEL_ID_1,
};

struct al_cpu_push_packet_common {
	struct al_nb_regs __iomem *nb_regs;
	al_phys_addr_t phy_base;
	void *base;
	al_bool decerr_en;
	al_bool slverr_en;
	al_bool par_gen_en;
	al_bool sel_8k;
	uint32_t sel_awuser;
};

struct al_cpu_push_packet {
	struct al_cpu_push_packet_common *common;
	struct al_nb_regs __iomem *nb_regs;
	struct al_cpu_push_packet_channel __iomem *channel_regs;
	unsigned int channel_id;
	al_phys_addr_t reg_phy_base;
	al_phys_addr_t data_phy_base;
};

/* Struct definitions */
struct al_cpu_push_packet_channel {
	uint32_t addr_low;
	uint32_t addr_high;
	uint32_t attr;
	uint32_t trig;
};

/**
 * Init common Push Packet handle
 *
 * @param	common
 *		Push Packet common data
 * @param	nb_regs_base
 *		The address of the System Fabric Regfile
 * @param	phy_base
 *		Push Packet base physical address
 *		Addressing RMN: 5341
 *		phy_base memory space (phy_base + (sel_8k ? 8KB : 128KB)*num_channels)
 *		must be of type DEVICE
 * @param	decerr_en
 *		Allow forwarding of DECERR indication for the packet write
 *		Currently not supported - should be AL_FALSE
 * @param	slverr_en
 *		Allow forwarding of SLVERR indication for the packet write
 * @param	par_gen_en
 *		Allow forwarding of parity error indication for the packet write
 * @param	sel_8k
 *		Choose channels offset seperation.
 *		If SET - Channels are seperated by 8KB
 *		If NOT set - Channels are seperated by 128KB
 * @param	sel_awuser
 *		Select whether to use addr[63:48] or PP attr as transaction attribute.
 *		Each bit if set to 1 selects the corresponding address bit.
 *		Otherwise, selects the corersponding attr bit.
 *
 * @return	0 if finished successfully
 *		<0 if an error occurred
 */
int al_cpu_push_packet_common_init(
		struct al_cpu_push_packet_common	*common,
		void __iomem				*nb_regs_base,
		al_phys_addr_t				phy_base,
		al_bool					decerr_en,
		al_bool					slverr_en,
		al_bool					par_gen_en,
		al_bool					sel_8k,
		uint32_t				sel_awuser);

/**
 * Init specific Push Packet handle
 *
 * @param	push_packet
 *		Push packet handle
 * @param	common
 *		Push packet common data
 * @param	channel_id
 *		Push packet Channel ID
 *
 * @return	0 if finished successfully
 *		<0 if an error occurred
 */
int al_cpu_push_packet_init(
		struct al_cpu_push_packet		*push_packet,
		struct al_cpu_push_packet_common	*common,
		unsigned int				channel_id);

/**
 * Set Push Packet virtual address
 *
 * @param	push_packet
 *		Push packet handle
 * @param	reg_addr
 *		Push Packet registers address
 *		This value can be computed using
 *		phy_to_virt(push_packet.reg_phy_base) after calling al_cpu_push_packet_init
 *
 * @return	0 if finished successfully
 *		<0 if an error occurred
 */
void al_cpu_push_packet_virt_addr_set(
		struct al_cpu_push_packet	*push_packet,
		void				*reg_addr);

/**
 * Enable specific Push Packet channel
 *
 * @param	push_packet
 *		Push packet handle
 * @param	enable
 *		Enable Push packet channel
 *
 * @return	0 if finished successfully
 *		<0 if an error occurred
 */
int al_cpu_push_packet_enable(
		struct al_cpu_push_packet	*push_packet,
		al_bool				enable);

/**
 * Get data buffer address
 *
 * @param	push_packet
 *		Push packet handle
 * @param	phy_addr
 *		Physical address of the transmitted packet
 * @param	num_bytes
 *		Number of bytes to send
 *		(phy_addr module 16) + num_bytes must not cross a 128B boundry
 *
 * @return	Address to start writing packet data to
 */
static INLINE al_phys_addr_t al_cpu_push_packet_data_buffer_addr_get(
		struct al_cpu_push_packet	*push_packet,
		al_phys_addr_t			phy_addr,
		unsigned int			num_bytes)
{
	/* Write first data according to 16byte alignment */
	unsigned int addr_offset = (unsigned int)(phy_addr & 0xF);

	/* Packet can not cross a 128B boundry */
	al_assert(addr_offset + num_bytes <= 128);

	return push_packet->data_phy_base + addr_offset;
}

/**
 * Set packet attribute
 *
 * @param	push_packet
 *		Push packet handle
 * @param	attr
 *		Attribue of the transmitted packet (Awuser)
 */
static INLINE void al_cpu_push_packet_attr_set(
		struct al_cpu_push_packet	*push_packet,
		uint32_t			attr)
{
	al_reg_write32(&push_packet->channel_regs->attr, attr);
}

/**
 * Send packet
 * User must first fill the data buffer before calling this function
 *
 * @param	push_packet
 *		Push packet handle
 * @param	phy_addr
 *		Physical address of the transmitted packet
 */
static INLINE void al_cpu_push_packet_send(
		struct al_cpu_push_packet	*push_packet,
		al_phys_addr_t			phy_addr)
{
	uint32_t phy_addr_high = (uint32_t)((phy_addr >> 32) & 0xffffffff);
	uint32_t phy_addr_low = (uint32_t)(phy_addr & 0xffffffff);

	/* Writing the high address triggers the write */
	al_reg_write32(&push_packet->channel_regs->addr_low, phy_addr_low);
	al_reg_write32(&push_packet->channel_regs->addr_high, phy_addr_high);

	/*
	* Addressing RMN: 5341
	*
	* RMN description:
	* data corruption when next packet is starting to build before last packet completed send
	* Software flow:
	* Send DMB after triggering push packet transaction transmission
	*/
	al_data_memory_barrier();
}

#endif /* __AL_HAL_PUSH_PACKET_H__ */

/** @} end of Push Packet group */
