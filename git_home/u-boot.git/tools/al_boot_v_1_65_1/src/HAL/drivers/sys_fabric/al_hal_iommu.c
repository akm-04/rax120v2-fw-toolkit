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

#include <al_hal_iommu.h>
#include <al_hal_iommu_regs.h>

void al_iommu_init(struct al_iommu_obj *obj, void __iomem *iommu_regs_base)
{
	obj->iommu_regs_base = iommu_regs_base;

	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	/* Enable TLB for bypass transactions */
	al_reg_write32_masked(
		&iommu_regs->iommu_acr,
		IOMMU_NON_SECURE_REGISTERS_IOMMU_ACR_SMTNMC_BPTLBEN |	/* Update TLB with the Stream Match Table No Match bypassed transaction details */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_ACR_MMUDIS_BPTLBEN |	/* Update TLB with the MMU-400 disabled transaction details */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_ACR_S2CR_BPTLBEN,	/* Update TLB with the S2CR bypassed transaction details */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_ACR_SMTNMC_BPTLBEN |
		IOMMU_NON_SECURE_REGISTERS_IOMMU_ACR_MMUDIS_BPTLBEN |
		IOMMU_NON_SECURE_REGISTERS_IOMMU_ACR_S2CR_BPTLBEN);
}

void al_iommu_init_context(struct al_iommu_obj *obj, unsigned int context_num, al_phys_addr_t page_table_addr, unsigned int vmid, al_bool page_table_shareable)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	uint32_t page_table_addr_low = (uint32_t)page_table_addr;
	uint32_t page_table_addr_high = (uint32_t)(page_table_addr >> 32);

	al_assert(context_num < AL_IOMMU_CONTEXT_NUM);
	al_assert(vmid < (1 << AL_IOMMU_VMID_WIDTH));

	al_dbg("page_table_address : 0x%08x%08x",
			page_table_addr_low, page_table_addr_high);

	/* Set page table attributes */
	al_reg_write32(
		&iommu_regs->cb[context_num].ttbcr,
		(IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_TTBCR_T0SZ_MINUS_3 |
		 IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_TTBCR_S |			/* Must be equal to t0sz[3] */
		 IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_TTBCR_SL0_LEVEL1 |		/* starting level for table is 1 */
		 IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_TTBCR_IRGN0_WRITEBACK_NO_WRITE_ALLOCATE |	/* table walks inner cacheability */
		 IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_TTBCR_ORGN0_WRITEBACK_NO_WRITE_ALLOCATE |	/* table walks outer cacheability */
		 ((page_table_shareable == AL_TRUE) ?				/* tables walks sharability */
		  IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_TTBCR_SH0_OUTER_SHAREABLE :
		  IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_TTBCR_SH0_NON_SHAREABLE)));


	al_reg_write32(
		&iommu_regs->cb[context_num].ttbr0_low,
		page_table_addr_low);
	al_reg_write32(
		&iommu_regs->cb[context_num].ttbr0_high,
		page_table_addr_high);

	/* Associate context bank with VMID */
	al_reg_write32(
		&iommu_regs->iommu_cbar[context_num],
		(vmid << IOMMU_NON_SECURE_REGISTERS_IOMMU_CBAR_VMID_SHIFT));

	/* Context system control register */
	/*
	 * Force broadcast off
	 * Hit-under-Miss - enable
	 * Terminate transaction on context fault
	 * Raise interrupt on context fault
	 * Little Endian
	 * Context faults are reported
	 * MMU enabled
	*/
	al_reg_write32(
		&iommu_regs->cb[context_num].sctlr,
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_SCTLR_CFRE |	/* Context faults are reported */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_SCTLR_CFIE |	/* Raise interrupt on context fault */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_SCTLR_HUPCF |	/* Hit-under-Miss enable */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_SCTLR_MMU);		/* MMU enabled */

}


void al_iommu_connect_id_to_context(struct al_iommu_obj *obj, unsigned int id, unsigned int mask, unsigned int context_num, unsigned int stream_num)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	al_assert(context_num < AL_IOMMU_CONTEXT_NUM);
	al_assert(stream_num < AL_IOMMU_STREAM_NUM);
	al_assert(id < (1 << AL_IOMMU_STREAM_ID_WIDTH));
	al_assert(mask < (1 << AL_IOMMU_STREAM_ID_WIDTH));

	al_reg_write32(&iommu_regs->iommu_smr[stream_num],
			((id << IOMMU_NON_SECURE_REGISTERS_IOMMU_SMR_ID_SHIFT) |
			 (mask << IOMMU_NON_SECURE_REGISTERS_IOMMU_SMR_MASK_SHIFT) |
			 IOMMU_NON_SECURE_REGISTERS_IOMMU_SMR_VALID));

	/* S2CR */
	/* Type 0 (translation context) with no attribute overide */
	al_reg_write32(&iommu_regs->iommu_s2cr[stream_num],
			((context_num << IOMMU_NON_SECURE_REGISTERS_IOMMU_S2CR_CBNDX_SHIFT) |
			 IOMMU_NON_SECURE_REGISTERS_IOMMU_S2CR_TYPE_TRANSLATION));

}

void al_iommu_enable(struct al_iommu_obj *obj)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	/* Context system control register */
	al_reg_write32_masked(
		&iommu_regs->iommu_cr0,
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CR0_USFCFG |		/* Raise an unidentified stream fault on transactions that do not match any entry */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CR0_GCFGFIE |		/* Global Configuration Fault Interrupt Enable */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CR0_GFIE |		/* Global Fault Interrupt Enable */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CR0_CLIENTPD,		/* Client Port Enable */
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CR0_USFCFG |
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CR0_GCFGFIE |
		IOMMU_NON_SECURE_REGISTERS_IOMMU_CR0_GFIE);

}

void al_iommu_tlb_invalidate_all_ns_nh(struct al_iommu_obj *obj)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	al_reg_write32(&iommu_regs->iommu_tlbiallnsnh,0);
}

void al_iommu_tlb_invalidate_by_vmid(struct al_iommu_obj *obj, unsigned int vmid)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	al_assert(vmid < (1 << AL_IOMMU_VMID_WIDTH));

	al_reg_write32(
		&iommu_regs->iommu_tlbivmid,
		(vmid << IOMMU_NON_SECURE_REGISTERS_IOMMU_TLBIVMID_VMID_SHIFT));
}

void al_iommu_tlb_invalidate_global_sync(struct al_iommu_obj *obj)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	al_reg_write32(&iommu_regs->iommu_tlbgsync,0);
}

void al_iommu_global_fault_status_get(struct al_iommu_obj *obj, struct al_iommu_global_fault_status *status)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	uint32_t reg_val;

	/* Get global fault address */
	reg_val = al_reg_read32(&iommu_regs->iommu_gfar_low);
	status->fault_address = reg_val;
	reg_val = al_reg_read32(&iommu_regs->iommu_gfar_high);
	status->fault_address |= (al_phys_addr_t)reg_val << 32;

	/* Get global fault status register */
	status->gfs = al_reg_read32(&iommu_regs->iommu_gfsr);
	status->gfsyn = al_reg_read32(&iommu_regs->iommu_gfsyndr0);
	reg_val = al_reg_read32(&iommu_regs->iommu_gfsyndr1);
	status->stream_id = (reg_val & IOMMU_NON_SECURE_REGISTERS_IOMMU_GFSYNDR1_STREAM_ID_MASK)
				>> IOMMU_NON_SECURE_REGISTERS_IOMMU_GFSYNDR1_STREAM_ID_SHIFT;

	// TODO : add support for ssd_index for secure access
}

void al_iommu_context_fault_status_get(struct al_iommu_obj *obj, struct al_iommu_context_fault_status *status, unsigned int context_num)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	uint32_t reg_val;

	/* Get context fault address */
	reg_val = al_reg_read32(&iommu_regs->cb[context_num].far_low);
	status->fault_address = reg_val;
	reg_val = al_reg_read32(&iommu_regs->cb[context_num].far_high);
	status->fault_address |= (al_phys_addr_t)reg_val << 32;

	/* Get context fault status register */
	status->cb_fs = al_reg_read32(&iommu_regs->cb[context_num].fsr);
	reg_val = al_reg_read32(&iommu_regs->cb[context_num].fsynr0);
	status->cb_fsyn = reg_val;
	status->plvl = (reg_val & IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_FSYNR0_PLVL_MASK)
				>> IOMMU_NON_SECURE_REGISTERS_IOMMU_CB_FSYNR0_PLVL_SHIFT;
}

void al_iommu_global_fault_status_clear(struct al_iommu_obj *obj, uint32_t mask)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	al_reg_write32(&iommu_regs->iommu_gfsr, mask);

}

void al_iommu_context_fault_status_clear(struct al_iommu_obj *obj, unsigned int context_num, uint32_t mask)
{
	struct al_iommu_non_secure_registers __iomem *iommu_regs =
		&((struct al_iommu_regs __iomem *)
		obj->iommu_regs_base)->non_secure_registers;

	al_reg_write32(&iommu_regs->cb[context_num].fsr, mask);

}

