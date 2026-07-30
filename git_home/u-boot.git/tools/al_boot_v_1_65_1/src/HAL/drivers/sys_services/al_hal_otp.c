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

#include "al_hal_otp.h"
#include "al_hal_otp_regs.h"
#include "al_hal_pbs_regs.h"

#define OTP_MAGIC_NUM_VAL	0xAFBEAFBE

#define OTP_LOCK_WORD_INDEX	31

/******************************************************************************
 ******************************************************************************/
int al_otp_handle_init(
	struct al_otp_handle	*otp_handle,
	void __iomem		*otp_regs_base,
	void __iomem		*pbs_regs_base)
{
	al_assert(otp_handle);
	al_assert(otp_regs_base);
	al_assert(pbs_regs_base);

	otp_handle->otp_regs_base = otp_regs_base;
	otp_handle->pbs_regs_base = pbs_regs_base;

	return 0;
}

/******************************************************************************
 ******************************************************************************/
uint32_t al_otp_read_word(
	struct al_otp_handle	*otp_handle,
	unsigned int		word_idx)
{
	struct al_otp_regs __iomem *otp_regs;

	al_assert(otp_handle);
	al_assert(word_idx < AL_OTP_WORD_IDX_LOCK);

	otp_regs = (struct al_otp_regs __iomem *)otp_handle->otp_regs_base;

	return al_reg_read32(&otp_regs->otpr[word_idx]);
}

/******************************************************************************
 ******************************************************************************/
uint32_t al_otp_read_word_direct(
	struct al_otp_handle	*otp_handle,
	unsigned int		word_idx)
{
	struct al_otp_regs __iomem *otp_regs;

	al_assert(otp_handle);
	al_assert(word_idx < AL_OTP_WORD_IDX_LOCK);

	otp_regs = (struct al_otp_regs __iomem *)otp_handle->otp_regs_base;

	return al_reg_read32(&otp_regs->otpw[word_idx]);
}

/******************************************************************************
 ******************************************************************************/
void al_otp_write_enable(
	struct al_otp_handle	*otp_handle)
{
	struct al_pbs_regs __iomem *pbs_regs;

	al_assert(otp_handle);

	pbs_regs = (struct al_pbs_regs __iomem *)otp_handle->pbs_regs_base;

	al_reg_write32(&pbs_regs->unit.otp_magic_num, OTP_MAGIC_NUM_VAL);
}

/******************************************************************************
 ******************************************************************************/
void al_otp_write_disable(
	struct al_otp_handle	*otp_handle)
{
	struct al_pbs_regs __iomem *pbs_regs;

	al_assert(otp_handle);

	pbs_regs = (struct al_pbs_regs __iomem *)otp_handle->pbs_regs_base;

	al_reg_write32(&pbs_regs->unit.otp_magic_num, 0);
}

/******************************************************************************
 ******************************************************************************/
int al_otp_write_word(
	struct al_otp_handle	*otp_handle,
	unsigned int		word_idx,
	uint32_t		val)
{
	struct al_otp_regs __iomem *otp_regs;
	struct al_pbs_regs __iomem *pbs_regs;
	uint32_t read_val;

	al_assert(otp_handle);
	al_assert(word_idx < AL_OTP_WORD_IDX_LOCK);

	otp_regs = (struct al_otp_regs __iomem *)otp_handle->otp_regs_base;
	pbs_regs = (struct al_pbs_regs __iomem *)otp_handle->pbs_regs_base;

	al_reg_write32(&otp_regs->otpw[word_idx], val);

	while (0 != (al_reg_read32(&pbs_regs->unit.otp_cntl)
				& PBS_UNIT_OTP_CNTL_OTP_BUSY))
		;

	read_val = al_reg_read32(&otp_regs->otpw[word_idx]);
	if (read_val != val) {
		al_err("%s(%u, %08x) failed! (read %08x)\n",
				__func__, word_idx, val, read_val);
		return -EIO;
	}

	return 0;
}

/******************************************************************************
 ******************************************************************************/
int al_otp_write_word_shadow(
	struct al_otp_handle	*otp_handle,
	unsigned int		word_idx,
	uint32_t		val)
{
	struct al_otp_regs __iomem *otp_regs;
	uint32_t read_val;

	al_assert(otp_handle);
	al_assert(word_idx < AL_OTP_WORD_IDX_LOCK);

	otp_regs = (struct al_otp_regs __iomem *)otp_handle->otp_regs_base;

	al_reg_write32(&otp_regs->otpr[word_idx], val);

	read_val = al_reg_read32(&otp_regs->otpr[word_idx]);
	if (read_val != val) {
		al_err("%s(%u, %08x) failed! (read %08x)\n",
				__func__, word_idx, val, read_val);
		return -EIO;
	}

	return 0;
}

/******************************************************************************
 ******************************************************************************/
int al_otp_lock_word(
	struct al_otp_handle	*otp_handle,
	unsigned int		word_idx)
{
	struct al_otp_regs __iomem *otp_regs;
	struct al_pbs_regs __iomem *pbs_regs;
	uint32_t val;
	uint32_t read_val;

	al_assert(otp_handle);
	al_assert(word_idx < AL_OTP_WORD_IDX_LOCK);

	otp_regs = (struct al_otp_regs __iomem *)otp_handle->otp_regs_base;
	pbs_regs = (struct al_pbs_regs __iomem *)otp_handle->pbs_regs_base;

	val = al_reg_read32(&otp_regs->otpw[AL_OTP_WORD_IDX_LOCK]);
	val |= (1 << word_idx);

	al_reg_write32(&otp_regs->otpw[AL_OTP_WORD_IDX_LOCK], val);

	while (0 != (al_reg_read32(&pbs_regs->unit.otp_cntl)
				& PBS_UNIT_OTP_CNTL_OTP_BUSY))
		;

	read_val = al_reg_read32(&otp_regs->otpw[AL_OTP_WORD_IDX_LOCK]);
	if (read_val != val) {
		al_err("%s(%u) failed! (read %08x, expected %08x)\n",
				__func__, word_idx, read_val, val);
		return -EIO;
	}

	return 0;
}

/******************************************************************************
 ******************************************************************************/
al_bool al_otp_word_is_locked(
		struct al_otp_handle	*otp_handle,
		unsigned int		word_idx)
{
	struct al_otp_regs __iomem *otp_regs;
	uint32_t read_val;
	al_bool ret_val;

	al_assert(otp_handle);
	al_assert(word_idx < AL_OTP_WORD_IDX_LOCK);

	otp_regs = (struct al_otp_regs __iomem *)otp_handle->otp_regs_base;

	read_val = al_reg_read32(&otp_regs->otpr[AL_OTP_WORD_IDX_LOCK]);

	ret_val = ((read_val & (1 << word_idx)) ? AL_TRUE : AL_FALSE);

	return ret_val;
}

