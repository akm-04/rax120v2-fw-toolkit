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
 *  PCIe init
 *  @{
 * @file   al_init_pcie.c
 *
 * @brief  PCIe initialization
 */

#include "al_init_pcie_params.h"
#include "al_hal_iomap.h"

#define PORT_ENABLE_WAIT_US (500) /* time to wait after port enable */
#define LINK_FINAL_WAIT_MS (48) /* max time to wait for link up */

#define INIT_PCIE_CHECK_ADV(tear_down_label, print_err, ...)				\
	do {										\
		if (rc) {								\
			if (print_err) {							\
				al_err("%s: PCIe[%d] ", __func__, pcie_port->port_id);	\
				al_err(__VA_ARGS__);					\
				al_err("\n");						\
			}								\
			goto tear_down_label;						\
		}									\
	} while (0)

#define INIT_PCIE_CHECK(tear_down_label, ...)						\
	INIT_PCIE_CHECK_ADV(tear_down_label, AL_TRUE, __VA_ARGS__)

__attribute__((weak)) int al_init_pcie_ex(
	struct al_init_pcie_handle *init_pcie_handle __attribute__ ((unused)),
	struct al_init_pcie_params *init_pcie_params)
{
	return init_pcie_params->ex_params != NULL;
}

int al_init_pcie(struct al_init_pcie_handle *init_pcie_handle,
		struct al_init_pcie_params *init_pcie_params,
		struct al_pcie_link_status *link_status)
{
	int rc = 0;
	unsigned int i;
	struct al_pcie_port *pcie_port = &(init_pcie_handle->pcie_port);

	if (!init_pcie_params->pcie_reg_base)
		init_pcie_params->pcie_reg_base =
			(void *)AL_SB_PCIE_BASE((long)init_pcie_params->port_id);
	if (!init_pcie_params->pbs_reg_base)
		init_pcie_params->pbs_reg_base = (void *)AL_PBS_REGFILE_BASE;

	rc = al_pcie_port_handle_init(pcie_port,
		init_pcie_params->pcie_reg_base,
		init_pcie_params->pbs_reg_base,
		init_pcie_params->port_id);
	INIT_PCIE_CHECK(init_done, "could not initialize port handle");

	rc = al_pcie_port_enable(pcie_port);
	INIT_PCIE_CHECK(init_done, "could not enable port before shutdown");
	al_udelay(PORT_ENABLE_WAIT_US);

	al_data_memory_barrier();
	rc = al_pcie_port_memory_shutdown_set(pcie_port, AL_FALSE);
	INIT_PCIE_CHECK(init_port_disable, "could not disable shutdown memory");
	al_data_memory_barrier();

	al_pcie_port_disable(pcie_port);

	rc = al_pcie_port_operating_mode_config(pcie_port, init_pcie_params->mode);
	INIT_PCIE_CHECK(init_shutdown_mem, "could not set mode[%d]", init_pcie_params->mode);

	rc = al_pcie_port_max_lanes_set(pcie_port, init_pcie_params->max_lanes);
	INIT_PCIE_CHECK(init_shutdown_mem,
		"could not set max_lanes[%d]", init_pcie_params->max_lanes);

	rc = al_pcie_port_max_num_of_pfs_set(pcie_port, init_pcie_params->max_num_of_pfs);
	INIT_PCIE_CHECK(init_shutdown_mem,
		"could not set max_num_of_pfs[%d]", init_pcie_params->max_num_of_pfs);

	if (init_pcie_params->reads_config) {
		rc = al_pcie_port_ib_hcrd_os_ob_reads_config(pcie_port,
			init_pcie_params->reads_config);
		INIT_PCIE_CHECK(init_shutdown_mem, "could not configure IB/OB reads");
	}

	rc = al_pcie_port_enable(pcie_port);
	INIT_PCIE_CHECK(init_shutdown_mem, "could not enable port");

	/* Wait more than 2000 clock cycles */
	al_udelay(PORT_ENABLE_WAIT_US);

	rc = al_pcie_port_config(pcie_port, init_pcie_params->port_params);
	INIT_PCIE_CHECK(init_shutdown_mem, "could not configure port");

	if (init_pcie_params->mode == AL_PCIE_OPERATING_MODE_RC) {
		for (i = 0; i < AL_MAX_NUM_OF_PFS; i++) {
			rc = (init_pcie_params->pf_params[i] != NULL);
			INIT_PCIE_CHECK(init_shutdown_mem,
				"pf_params[%d] should be NULL in RC mode", i);
		}
		rc = (init_pcie_params->ex_params != NULL);
		INIT_PCIE_CHECK(init_shutdown_mem, "extended params are not supported in RC mode");
	} else if (init_pcie_params->mode == AL_PCIE_OPERATING_MODE_EP) {
		struct al_pcie_pf *pcie_pf;

		for (i = 0; i < init_pcie_params->max_num_of_pfs; i++) {
			al_assert(init_pcie_params->pf_params[i] != NULL);

			pcie_pf = &init_pcie_handle->pcie_pf[i];
			rc = al_pcie_pf_handle_init(pcie_pf, pcie_port, i);
			INIT_PCIE_CHECK(init_shutdown_mem, "could not initialize pf[%d] handle", i);

			rc = al_pcie_pf_config(pcie_pf, init_pcie_params->pf_params[i]);
			INIT_PCIE_CHECK(init_shutdown_mem, "could not configure pf[%d]", i);
		}

		rc = al_init_pcie_ex(init_pcie_handle, init_pcie_params);
		INIT_PCIE_CHECK(init_shutdown_mem, "could not perform extended init");
	} else {
		al_assert(0);
	}

	if (init_pcie_params->crrs_en)
		al_pcie_app_req_retry_set(pcie_port, AL_TRUE);

	if (!init_pcie_params->start_link)
		return rc;

	rc = al_pcie_link_start(pcie_port);
	INIT_PCIE_CHECK(init_shutdown_mem, "could not start link");

	if (pcie_port->port_id == 2)
	{
		gpio_direction_output(20, 0);
		mdelay(120);
		gpio_direction_output(20, 1);
	}

	if (!init_pcie_params->wait_for_link_timeout_ms)
		return rc;

	rc = al_pcie_link_up_wait(pcie_port, init_pcie_params->wait_for_link_timeout_ms);
	INIT_PCIE_CHECK_ADV(
		init_stop_link, !init_pcie_params->wait_for_link_silent_fail, "link is not up");

	/* Wait till link speed is the max speed */
	for (i = 0; i < LINK_FINAL_WAIT_MS; i++) {
		rc = al_pcie_link_status(pcie_port, link_status);
		INIT_PCIE_CHECK(init_stop_link, "could not check link status");

		if (link_status->speed == init_pcie_params->port_params->link_params->max_speed)
			break;
		else
			al_udelay(1000);
	}

	if (link_status->link_up) {
		return rc;
	} else {
		rc = -EINVAL;
		INIT_PCIE_CHECK(init_stop_link, "could not link up port");
	}

init_stop_link:
	al_pcie_link_stop(pcie_port);
init_shutdown_mem:
	al_pcie_port_memory_shutdown_set(pcie_port, AL_TRUE);
init_port_disable:
	al_pcie_port_disable(pcie_port);
init_done:
	return rc;
}
