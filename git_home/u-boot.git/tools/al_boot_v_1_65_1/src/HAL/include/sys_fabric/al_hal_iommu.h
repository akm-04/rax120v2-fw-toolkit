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
 * @defgroup group_iommu IOMMU
 * @ingroup group_sys_fabric
 *  @{
 * @section overview Overview
 * The IOMMU enabling static and dynamic
 * Address Translation processes of SB masters
 *
 * @subsection cfg Page Table format
 * level 1 page table entry format (64 bit):
 *	[0]	Valid
 *	[1]	Type - BLOCK_DESCRIPTOR = 0, TABLE_DESCRIPTOR = 1
 *	[5:2]	MemAttr - Stage 2 memory attributes. 4'b1111 leaves the stage1 attribute
 *	[7:6]	HAP - Stage 2 Access Permissions bits : NO_ACCESS = 0, READ_ONLY = 1, WRITE_ONLY = 2, READ_WRITE = 3
 *	[9:8]	SH - Shareability field : NON_SHAREABLE = 0, OUTER_SHAREABLE = 2, INNER_SHAREABLE = 3
 *	[10]	AF - Access flag
 *	[11:29]	Reserved
 *	[39:30]	OutputAddress - bits [39:30] of output address
 *	[40:51]	Reserved
 *	[52]	ContiguousHint - A hint bit indicating that 16 adjacent translation table entries point to contiguous memory regions
 *	[53]	Reserved
 *	[54]	XN - Execute-never bit. Determines whether the region is executable
 *	[55:63]	Reserved
 *
 * The page table entry is decided using bits [39:30] of the input address
 *
 * @subsection init Initialization
 * @code
 *	int main()
 *	{
 *		struct al_iommu_obj iommu_obj[2];
 *
 *		// Showing sequence for IOMMU0, the same should be done for IOMMU1
 *
 *		// Initialize IOMMU
 *		al_iommu_init(&iommu_obj[0], AL_NB_SMMU0_OFFSET);
 *
 *		// Go over all contexts (VMIDs) and set page table start address
 *		al_iommu_init_context(&iommu_obj[0], VMID_NUM, page_table_addr);
 *
 *		// Connect between the Master ID (which should be set in the ADPATER/GDMA/PCIE) to the context (VMID)
 *		// Can connect several MASTER_IDs to the same context (VMID)
 *		// The STREAM_NUM should be unique to each id added
 *		al_iommu_connect_id_to_context(&iommu_obj[0], MASTER_ID, ID_MASK, VMID_NUM, STREAM_NUM);
 *
 *		// Enable IOMMU after setting all VMIDs
 *		void al_iommu_enable(&iommu_obj[0]);
 *
 *	}
 * @endcode
 * @file   al_hal_iommu.h
 *
 * @brief Header file for the iommu HAL
 */


#ifndef __AL_HAL_IOMMU_H_
#define __AL_HAL_IOMMU_H_

#include "al_hal_common.h"

/* Width of context VMID attribute */
#define AL_IOMMU_VMID_WIDTH 8

/* Number of contexts (Virtual Machines) supported simultanously by the IOMMU */
#define AL_IOMMU_CONTEXT_NUM 8

/* Number of streams (different IDs) supported simultanously by the IOMMU */
#define AL_IOMMU_STREAM_NUM 32

/* ID width for matching ID to context */
#define AL_IOMMU_STREAM_ID_WIDTH 12

struct al_iommu_obj {
	struct al_iommu_regs __iomem	*iommu_regs_base;
};

/**
 * Context bank fault status
 * Bit field description of gfs field of al_iommu_global_fault_status
*/
enum al_iommu_gfs_t {
	/**
	 * Invalid Context Fault
	 * The possible values of this bit are:
	 * 0 Invalid context fault
	 * 1 No Invalid context fault
	*/
	AL_IOMMU_GFS_ICF = (1 << 0),
	/**
	 * Unidentified Stream Fault
	 * The possible values of this bit are:
	 * 0 No Unidentified stream fault
	 * 1 Unidentified stream fault
	*/
	AL_IOMMU_GFS_USF = (1 << 1),
	/**
	 * Stream Match Conflict Fault
	 * The possible values of this bit are:
	 * 0 No Stream match conflict fault
	 * 1 Stream match conflict fault
	*/
	AL_IOMMU_GFS_SMCF = (1 << 2),
	/**
	 * Unimplemented Context Bank Fault
	 * The possible values of this bit are:
	 * 0 No Unimplemented context bank fault
	 * 1 Unimplemented context bank fault
	*/
	AL_IOMMU_GFS_UCBF = (1 << 3),
	/**
	 * Unimplemented Context Interrupt Fault
	 * The possible values of this bit are:
	 * 0 No Unimplemented context interrupt fault
	 * 1 Unimplemented context interrupt fault
	*/
	AL_IOMMU_GFS_UCIF = (1 << 4),
	/**
	 * Configuration Access Fault
	 * The possible values of this bit are:
	 * 0 No Configuration access fault
	 * 1 Configuration access fault
	*/
	AL_IOMMU_GFS_CAF = (1 << 5),
	/**
	 * External Fault caused by an external abort
	 * The possible values of this bit are:
	 * 0 No external fault
	 * 1 External fault
	*/
	AL_IOMMU_GFS_EF = (1 << 6),
	/**
	 * Permission Fault
	 * For non-secure access, this bit is reserved.
	 * The possible values of this bit are:
	 * 0 No permission fault
	 * 1 Permission fault
	 *
	 * Note : If a transaction is associated with a particular Translation context bank,
	 * faults are recorded in cb_fault_status
	*/
	AL_IOMMU_GFS_PF = (1 << 7),
	/**
	 * Multiple error condition
	 * The possible values of this bit are:
	 * 0 No multiple error condition was encountered
	 * 1 multiple error condition was encountered
	*/
	AL_IOMMU_GFS_MULTI = (1 << 31),
};

/**
 * Context bank fault syndrome
 * Bit field description of gfsyn field of al_iommu_global_fault_status
*/
enum al_iommu_gfsyn_t {
	/**
	 * Nested fault
	 * For secure access, this bit is reserved.
	 *
	 * The possible values of this bit are:
	 * 0 The fault occurred in the initial stream context
	 * 1 The fault occurred in a nested context
	*/
	AL_IOMMU_GFSYN_NESTED = (1 << 0),
	/**
	 * Write Not Read
	 *
	 * The possible values of this bit are:
	 * 0 The faulty transaction is a read
	 * 1 The faulty transaction is a write
	*/
	AL_IOMMU_GFSYN_WNR = (1 << 1),
	/**
	 * Privileged Not Unprivileged
	 *
	 * The possible values of this bit are:
	 * 0 The faulty transaction has the unprivileged attribute
	 * 1 The faulty transaction is privileged
	*/
	AL_IOMMU_GFSYN_PNU = (1 << 2),
	/**
	 * Instruction Not Data
	 *
	 * The possible values of this bit are:
	 * 0 The faulty transaction has the data access attribute
	 * 1 The faulty transaction has the instruction access attribute
	*/
	AL_IOMMU_GFSYN_IND = (1 << 3),
	/**
	 * Non-Secure State
	 * For non-secure access, this bit is reserved.
	 *
	 * The possible values of this bit are:
	 * 0 The faulty transaction is associated with a Secure device
	 * 1 The faulty transaction is associated with a Non-secure device
	*/
	AL_IOMMU_GFSYN_NSSTATE = (1 << 4),
	/**
	 * Non-Secure Attribute
	 * For non-secure access, this bit is reserved.
	 *
	 * The possible values of this bit are:
	 * 0 The faulty transaction has the Secure attribute
	 * 1 The faulty transaction has the Non-secure attribute
	*/
	AL_IOMMU_GFSYN_NSATTR = (1 << 5),
	/**
	 * Address translation operation fault
	 * For secure access, this bit is reserved.
	 *
	 * The possible values of this bit are:
	 * 0 The fault was not caused by the processing of an CB operation
	 * 	initiated in a Stage 1 Translation context bank
	 * 1 The fault was caused by the processing of an CB operation
	 *	initiated in a Stage 1 Translation context bank
	*/
	AL_IOMMU_GFSYN_ATS = (1 << 6),
};

/* IOMMU global fault */
struct al_iommu_global_fault_status {
	/* Fault address */
	al_phys_addr_t fault_address;

	/* Fault status */
	unsigned int gfs; /* see al_iommu_gfs_t for bit description */
	unsigned int gfsyn; /* see al_iommu_gfsyn_t for bit description */

	/**
	 * SSD_Index of the transaction that caused the fault
	 * For non-secure access, this bit is reserved.
	 *
	 * Currently not supported
	*/
	unsigned int ssd_index;

	/**
	 * StreamID of the transaction that caused the fault
	*/
	unsigned int stream_id;
};

/**
 * Context bank fault status
 * Bit field description of cb_fs field of al_iommu_context_fault_status
*/
enum al_iommu_cb_fs_t {
	/**
	 * Translation Fault
	 * The possible values of this bit are:
	 * 0 There is no Translation fault
	 * 1 A Translation fault has occurred. The mapping for the address being accessed is invalid
	*/
	AL_IOMMU_CB_FS_TF = (1 << 1),
	/**
	 * Access Flag Fault
	 * The possible values of this bit are:
	 * 0 There is no Access flag fault
	 * 1 A fault caused by the access flag being set for the address being accessed has occurred
	*/
	AL_IOMMU_CB_FS_AFF = (1 << 2),
	/**
	 * Permission Fault
	 * The possible values of this bit are:
	 * 0 There is no Permission fault
	 * 1 A fault caused by insufficient permission to complete a memory access has occurred
	*/
	AL_IOMMU_CB_FS_PF = (1 << 3),
	/**
	 * External Fault
	 * The possible values of this bit are:
	 * 0 There is no External fault
	 * 1 An External fault has occurred
	*/
	AL_IOMMU_CB_FS_EF = (1 << 4),
	/**
	 * TLB Match Conflict Fault
	 * The possible values of this bit are:
	 * 0 There is no TLB Match conflict fault
	 * 1 A fault caused by multiple matches was detected in the TLB
	*/
	AL_IOMMU_CB_FS_TLBMCF = (1 << 5),
	/**
	 * TLB Lock Fault
	 * The possible values of this bit are:
	 * 0 There is no TLB lock fault
	 * 1 A TLB lock fault has occurred
	*/
	AL_IOMMU_CB_FS_TLBLKF = (1 << 6),
	/**
	 * Stalled Status
	 * The possible values of this bit are:
	 * 0 The context is not stalled
	 * 1 The context is stalled because of an exception in the context bank
	 *
	 * Cleared by using al_iommu_cb_resume (NOT IMPLEMENTED)
	*/
	AL_IOMMU_CB_FS_SS = (1 << 30),
	/**
	 * Multiple error condition
	 * The possible values of this bit are:
	 * 0 No multiple error condition was encountered
	 * 1 multiple error condition was encountered
	*/
	AL_IOMMU_CB_FS_MULTI = (1 << 31),
};

/**
 * Context bank fault syndrome
 * Bit field description of cb_fsyn field of al_iommu_context_fault_status
*/
enum al_iommu_cb_fsyn_t {
	/**
	 * Write Not Read
	 * The possible values of this bit are:
	 * 0 Read
	 * 1 Write
	*/
	AL_IOMMU_CB_FSYN_WNR = (1 << 4),
	/**
	 * Privileged Not Unprivileged
	 * The possible values of this bit are:
	 * 0 Unprivileged
	 * 1 Privileged
	*/
	AL_IOMMU_CB_FSYN_PNU = (1 << 5),
	/**
	 * Instruction Not Data
	 * The possible values of this bit are:
	 * 0 Data
	 * 1 Instruction
	*/
	AL_IOMMU_CB_FSYN_IND = (1 << 6),
	/**
	 * Non-secure Attribute
	 * The possible values of this bit are:
	 * 0 The input transaction has a Secure attribute
	 * 1 The input transaction has a Non-secure attribute
	 *
	 * In a Non-secure context bank, this bit is reserved
	*/
	AL_IOMMU_CB_FSYN_NSATTR = (1 << 8),
	/**
	 * A walk fault on a translation table access
	 * The possible values of this bit are:
	 * 0 A walk fault did not occur
	 * 1 A fault occurred during processing of a translation table walk
	*/
	AL_IOMMU_CB_FSYN_PTWF = (1 << 10),
	/**
	 * Asynchronous Fault Recorded
	 * The possible values of this bit are:
	 * 0 A fault was recorded synchronously
	 * 1 A fault was recorded asynchronously
	*/
	AL_IOMMU_CB_FSYN_AFR = (1 << 11),
};

/* IOMMU context fault */
struct al_iommu_context_fault_status {
	/* Fault address */
	al_phys_addr_t fault_address;

	/* Fault status */
	unsigned int cb_fs; /* see al_iommu_cb_fs_t for bit description */
	unsigned int cb_fsyn; /* see al_iommu_cb_fsyn_t for bit description */

	/**
	 * Translation Table Level. the level in the translation table walk
	 * that the fault is associated with (1/2/3)
	*/
	unsigned int plvl;
};


/**
 * initialization for IOMMU
 *
 * This function must be called once only per init.
 *
 * @param[out] obj
 *	iommu context object.
 * @param[in] iommu_regs_base
 *	base address of the iommu
 */
void al_iommu_init(struct al_iommu_obj *obj, void __iomem *iommu_regs_base);

/**
 * initialization iommu context for specific VMID
 *
 * @param[in] obj
 *	iommu context object.
 * @param[in] context_num
 *	the context to be configured (one per VMID)
 * @param[in] page_table_addr
 *	the page table address for this VMID
 * @param[in] vmid
 *	the context VMID
 * @param[in] page_table_shareable
 *	AL_TRUE if page table is shareable
 */
void al_iommu_init_context(struct al_iommu_obj *obj, unsigned int context_num, al_phys_addr_t page_table_addr, unsigned int vmid, al_bool page_table_shareable);

/**
 * connect SB master stream id (AxUser+AxId) to context (VMID)
 *
 * @param[in] obj
 *	iommu context object.
 * @param[in] id
 *	stream id to match
 * @param[in] mask
 *	mask[i] == 1 => id[i] is ignored
 * @param[in] context_num
 *	the context to be used by this id
 * @param[in] stream_num
 *	the stream number [0..31] to be used by this id. should choose a unique number for each id
 */
void al_iommu_connect_id_to_context(struct al_iommu_obj *obj, unsigned int id, unsigned int mask, unsigned int context_num, unsigned int stream_num);

/**
 * Enable IOMMU
 *
 * @param[in] obj
 *	iommu context object.
 */
void al_iommu_enable(struct al_iommu_obj *obj);

/**
 * Invalidate all IOMMU non-secure non-hypervisor TLB entries
 *
 * @param[in] obj
 *	iommu context object.
 */
void al_iommu_tlb_invalidate_all_ns_nh(struct al_iommu_obj *obj);

/**
 * Invalidate all IOMMU non-secure non-hypervisor TLB entries having the specified VMID
 *
 * @param[in] obj
 *	iommu context object.
 * @param[in] vmid
 *	VMID for which TLB entries will be invalidated
 */
void al_iommu_tlb_invalidate_by_vmid(struct al_iommu_obj *obj, unsigned int vmid);

/**
 * Starts a global synchronization operation that ensures
 * the completion of any previously accepted TLB Invalidate operation
 * For secure access - apply to secure invalidate operation
 * For non-secure access - apply to non-secure invalidate operation
 *
 * @param[in] obj
 *	iommu context object.
 */
void al_iommu_tlb_invalidate_global_sync(struct al_iommu_obj *obj);

/**
 * Returns global fault status
 *
 * @param[in] obj
 *	iommu context object.
 * @param[out] status
 *	iommu global fault status object.
 */
void al_iommu_global_fault_status_get(struct al_iommu_obj *obj, struct al_iommu_global_fault_status *status);

/**
 * Returns context fault status
 *
 * @param[in] obj
 *	iommu context object.
 * @param[out] status
 *	iommu context fault status object.
 * @param[in] context_num
 *	chooses which context bank status is returned
 */
void al_iommu_context_fault_status_get(struct al_iommu_obj *obj, struct al_iommu_context_fault_status *status, unsigned int context_num);

/**
 * Clears global fault status
 * Clearing all status bits will deassert the interrupt
 *
 * @param[in] obj
 *	iommu context object.
 * @param[in] mask
 *	mask[i] == 1 => bit will be cleared from fault status
 *	see al_iommu_gfs_t for available bits
 */
void al_iommu_global_fault_status_clear(struct al_iommu_obj *obj, uint32_t mask);

/**
 * Clears context fault status
 * Clearing all status bits will deassert the interrupt
 *
 * @param[in] obj
 *	iommu context object.
 * @param[in] context_num
 *	chooses which context bank status to clear
 * @param[in] mask
 *	mask[i] == 1 => bit will be cleared from fault status
 *	see al_iommu_cb_fs_t for available bits
 */
void al_iommu_context_fault_status_clear(struct al_iommu_obj *obj, unsigned int context_num, uint32_t mask);

#endif /* __AL_HAL_IOMMU_H_ */

/** @} */
