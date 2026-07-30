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
 * @defgroup group_pcie_init PCIe Initialization
 * @ingroup grouppcieinit
 *  @{
 *  The PCIe Initialization HAL can be used in order to enable/configure/start
 *  PCIe port and physical functions.
 *
 * Common operation example:
 * @code
 *
 * // create init_pcie_handle
 * struct al_init_pcie_handle init_pcie_handle;
 *
 * // create link status for
 * struct al_pcie_link_status link_status = {0};
 *
 * // do some changes to the init_pcie_params defaults...
 * default_init_pcie_params.port_id = 0;
 * default_init_pcie_params.mode = AL_PCIE_OPERATING_MODE_EP;
 * default_init_pcie_params.max_lanes = 4;
 * default_init_pcie_params.max_num_of_pfs = 1;
 * default_init_pcie_params.port_params->link_params->max_speed =
 *							AL_PCIE_LINK_SPEED_GEN3;
 * // print the parameters
 * al_init_pcie_print_params(&default_init_pcie_params);
 *
 * // init the pcie port according to the init_pcie_params
 * al_init_pcie(&init_pcie_handle, &default_init_pcie_params, &link_status);
 *
 * @endcode
 *
 * @file   al_init_pcie.h
 * @brief C Header file for the PCIe initialization
 */

#ifndef AL_INIT_PCIE_H__
#define AL_INIT_PCIE_H__

#include "al_hal_pcie.h"

/**
 * @brief	Init PCIe parameters include all parameters that can be
 *		configured as part of the enable/configure/start PCIe port and
 *		physical/virtual functions
 *
 * @field	port_id: the port id
 *			(default: 0)
 * @field	pcie_reg_base: pcie base registers
 *			(default: AL_SB_PCIE_BASE(port_id))
 * @field	pbs_reg_base: pbs base registers
 *			(default: AL_PBS_REGFILE_BASE)
 * @field	mode: operating mode
 *			(default: EP)
 * @field	max_lanes: number of max lanes supported
 *			(default: 8)
 * @field	max_num_of_pfs: number of PFs supported
 *			(default: 1)
 * @field	reads_config: inbound header credit and outstanding outbound reads
 *			(default: NULL - does no read configuration)
 * @field	port_params: port configuration params used to config the port
 *			(default: see al_init_pcie_params.h)
 * @field	pf_params: an array of PF params to config each PF
 *			(default: see al_init_pcie_params.h)
 * @field	start_link: AL_TRUE to start link after configuration
 *			(default: AL_TRUE)
 * @field	wait_for_link_timeout_ms: milliseconds to wait after link is started
 *			(default: 1ms)
 * @field	wait_for_link_silent_fail: don't print an error upon failure to establish link
 *			(default: AL_FALSE)
 * @field	crrs_en: enable deferring incoming configuration requests
 *			(default: AL_FALSE)
 * @field	ex_params: extended params
 * 			(default: NULL)
*/
struct al_init_pcie_params {
	unsigned int					port_id;
	void						*pcie_reg_base;
	void						*pbs_reg_base;
	enum al_pcie_operating_mode			mode;
	unsigned int					max_lanes;
	unsigned int					max_num_of_pfs;
	struct al_pcie_ib_hcrd_os_ob_reads_config	*reads_config;
	struct al_pcie_port_config_params		*port_params;
	struct al_pcie_pf_config_params			*pf_params[AL_MAX_NUM_OF_PFS];
	al_bool						start_link;
	unsigned int					wait_for_link_timeout_ms;
	al_bool						wait_for_link_silent_fail;
	al_bool						crrs_en;
	void						*ex_params;
};

/**
 * A single structure that has both port and physical functions handles
 */
struct al_init_pcie_handle {
	struct al_pcie_port pcie_port;
	struct al_pcie_pf pcie_pf[AL_MAX_NUM_OF_PFS];
};

/**
 * The default initialization parameters
 */
extern struct al_init_pcie_params default_init_pcie_params;

/**
 * Initialize PCIe Port and PFs
 * @param  init_pcie_handle	the pcie init handle
 * @param  init_pcie_params	pcie init Port and PFs params
 * @param  link_status		the link status result
 * @return			0 if no error found
 */
int al_init_pcie(struct al_init_pcie_handle *init_pcie_handle,
		struct al_init_pcie_params *init_pcie_params,
		struct al_pcie_link_status *link_status);

#endif /* AL_INIT_PCIE_H__ */
