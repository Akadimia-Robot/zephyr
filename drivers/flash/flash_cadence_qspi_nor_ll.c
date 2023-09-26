/*
 * Copyright (c) 2022 - 2023 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "flash_cadence_qspi_nor_ll.h"
#include <string.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(flash_cadence_ll, CONFIG_FLASH_LOG_LEVEL);

#define SET_L(x) ((uint64_t)(x) & BIT64_MASK(32))
#define SET_H(x) ((((uint64_t)x) & BIT64_MASK(32)) << 32)

#define CONVERTTOMHZ(x)          (x / 1000000)
#define WRITE_WATER_LEVEL_MARK   100

#ifdef CONFIG_CAD_QSPI_INTERRUPT_SUPPORT
struct transfer_params {
	uint32_t requested_bytes;
	uint32_t transferred_bytes;
	uint32_t offset;
	uint8_t *buffer;
};

struct transfer_params *read_req;
struct transfer_params *write_req;

int cad_qspi_indirect_page_bound_write_using_interrupt(struct cad_qspi_params *cad_params,
					uint32_t offset, uint8_t *buffer, uint32_t len);
int cad_qspi_read_bank_using_interrupt(struct cad_qspi_params *cad_params, uint8_t *buffer,
				uint32_t offset, uint32_t size);
#endif

int cad_qspi_idle(struct cad_qspi_params *cad_params)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	return (sys_read32(cad_params->reg_base + CAD_QSPI_CFG) & CAD_QSPI_CFG_IDLE) >> 31;
}

int cad_qspi_set_baudrate_div(struct cad_qspi_params *cad_params, uint32_t div)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (div > 0xf) {
		LOG_ERR("Invalid divider value div = %x\n", div);
		return CAD_INVALID;
	}

	sys_clear_bits(cad_params->reg_base + CAD_QSPI_CFG, ~CAD_QSPI_CFG_BAUDDIV_MSK);

	sys_set_bits(cad_params->reg_base + CAD_QSPI_CFG, CAD_QSPI_CFG_BAUDDIV(div));

	return 0;
}

int cad_qspi_configure_dev_size(struct cad_qspi_params *cad_params, uint32_t addr_bytes,
				uint32_t bytes_per_dev, uint32_t bytes_per_block)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(CAD_QSPI_DEVSZ_ADDR_BYTES(addr_bytes) |
			    CAD_QSPI_DEVSZ_BYTES_PER_PAGE(bytes_per_dev) |
			    CAD_QSPI_DEVSZ_BYTES_PER_BLOCK(bytes_per_block),
		    cad_params->reg_base + CAD_QSPI_DEVSZ);
	return 0;
}

int cad_qspi_set_read_config(struct cad_qspi_params *cad_params, uint32_t opcode,
			     uint32_t instr_type, uint32_t addr_type, uint32_t data_type,
			     uint32_t mode_bit, uint32_t dummy_clk_cycle)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(CAD_QSPI_DEV_OPCODE(opcode) | CAD_QSPI_DEV_INST_TYPE(instr_type) |
			    CAD_QSPI_DEV_ADDR_TYPE(addr_type) | CAD_QSPI_DEV_DATA_TYPE(data_type) |
			    CAD_QSPI_DEV_MODE_BIT(mode_bit) |
			    CAD_QSPI_DEV_DUMMY_CLK_CYCLE(dummy_clk_cycle),
		    cad_params->reg_base + CAD_QSPI_DEVRD);

	return 0;
}

int cad_qspi_set_write_config(struct cad_qspi_params *cad_params, uint32_t opcode,
			      uint32_t addr_type, uint32_t data_type, uint32_t dummy_clk_cycle)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(CAD_QSPI_DEV_OPCODE(opcode) | CAD_QSPI_DEV_ADDR_TYPE(addr_type) |
			    CAD_QSPI_DEV_DATA_TYPE(data_type) |
			    CAD_QSPI_DEV_DUMMY_CLK_CYCLE(dummy_clk_cycle),
		    cad_params->reg_base + CAD_QSPI_DEVWR);

	return 0;
}

void cad_qspi_disable_interrupt_mask(struct cad_qspi_params *cad_params, uint32_t irq_bitmask)
{
	uint32_t temp_mask = sys_read32(cad_params->reg_base + CAD_QSPI_INTRMSKREG);

	temp_mask &= (~irq_bitmask);
	sys_write32(temp_mask, cad_params->reg_base + CAD_QSPI_INTRMSKREG);
}

void cad_qspi_clr_int_mask(struct cad_qspi_params *cad_params, uint32_t cler_en_mask)
{
	uint32_t temp_mask = sys_read32(cad_params->reg_base + CAD_QSPI_INTRMSKREG);

	temp_mask &= cler_en_mask;
	sys_write32(temp_mask, cad_params->reg_base + CAD_QSPI_INTRMSKREG);
}

void cad_qspi_enable_interrupt_mask(struct cad_qspi_params *cad_params, uint32_t  irq_bitmask)
{
	uint32_t temp_mask = sys_read32(cad_params->reg_base + CAD_QSPI_INTRMSKREG);

	temp_mask |= irq_bitmask;
	sys_write32(temp_mask, cad_params->reg_base + CAD_QSPI_INTRMSKREG);
}

int cad_qspi_timing_config(struct cad_qspi_params *cad_params, uint32_t clkphase, uint32_t clkpol,
			   uint32_t csda, uint32_t csdads, uint32_t cseot, uint32_t cssot,
			   uint32_t rddatacap)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	uint32_t cfg = sys_read32(cad_params->reg_base + CAD_QSPI_CFG);

	cfg &= CAD_QSPI_CFG_SELCLKPHASE_CLR_MSK & CAD_QSPI_CFG_SELCLKPOL_CLR_MSK;
	cfg |= CAD_QSPI_SELCLKPHASE(clkphase) | CAD_QSPI_SELCLKPOL(clkpol);

	sys_write32(cfg, cad_params->reg_base + CAD_QSPI_CFG);

	sys_write32(CAD_QSPI_DELAY_CSSOT(cssot) | CAD_QSPI_DELAY_CSEOT(cseot) |
			    CAD_QSPI_DELAY_CSDADS(csdads) | CAD_QSPI_DELAY_CSDA(csda),
		    cad_params->reg_base + CAD_QSPI_DELAY);

	return 0;
}

int cad_qspi_stig_cmd_helper(struct cad_qspi_params *cad_params, int cs, uint32_t cmd)
{
	uint32_t count = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* Chip select */
	sys_write32((sys_read32(cad_params->reg_base + CAD_QSPI_CFG) & CAD_QSPI_CFG_CS_MSK) |
			    CAD_QSPI_CFG_CS(cs),
		    cad_params->reg_base + CAD_QSPI_CFG);

	sys_write32(cmd, cad_params->reg_base + CAD_QSPI_FLASHCMD);
	sys_write32(cmd | CAD_QSPI_FLASHCMD_EXECUTE, cad_params->reg_base + CAD_QSPI_FLASHCMD);

	do {
		uint32_t reg = sys_read32(cad_params->reg_base + CAD_QSPI_FLASHCMD);

		if (!(reg & CAD_QSPI_FLASHCMD_EXECUTE_STAT)) {
			break;
		}
		count++;
	} while (count < CAD_QSPI_COMMAND_TIMEOUT);

	if (count >= CAD_QSPI_COMMAND_TIMEOUT) {
		LOG_ERR("Error sending QSPI command %x, timed out\n", cmd);
		return CAD_QSPI_ERROR;
	}

	return 0;
}

int cad_qspi_stig_cmd(struct cad_qspi_params *cad_params, uint32_t opcode, uint32_t dummy)
{

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1)) {
		LOG_ERR("Faulty dummy bytes\n");
		return -1;
	}

	return cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs,
					CAD_QSPI_FLASHCMD_OPCODE(opcode) |
						CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES(dummy));
}

int cad_qspi_stig_read_cmd(struct cad_qspi_params *cad_params, uint32_t opcode, uint32_t dummy,
			   uint32_t num_bytes, uint64_t *output)
{
	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1)) {
		LOG_ERR("Faulty dummy byes\n");
		return -1;
	}

	if ((num_bytes > 8) || (num_bytes == 0)) {
		LOG_ERR("Invalid number of bytes\n");
		return -1;
	}

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	uint32_t cmd = CAD_QSPI_FLASHCMD_OPCODE(opcode) | CAD_QSPI_FLASHCMD_ENRDDATA(1) |
		       CAD_QSPI_FLASHCMD_NUMRDDATABYTES(num_bytes - 1) |
		       CAD_QSPI_FLASHCMD_ENCMDADDR(0) | CAD_QSPI_FLASHCMD_ENMODEBIT(0) |
		       CAD_QSPI_FLASHCMD_NUMADDRBYTES(0) | CAD_QSPI_FLASHCMD_ENWRDATA(0) |
		       CAD_QSPI_FLASHCMD_NUMWRDATABYTES(0) | CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(dummy);

	if (cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs, cmd)) {
		LOG_ERR("failed to send stig cmd\n");
		return -1;
	}

	*output = SET_L(sys_read32(cad_params->reg_base + CAD_QSPI_FLASHCMD_RDDATA0));

	if (num_bytes > 4) {
		*output |= SET_H(sys_read32(cad_params->reg_base + CAD_QSPI_FLASHCMD_RDDATA1));
	}

	return 0;
}

int cad_qspi_stig_wr_cmd(struct cad_qspi_params *cad_params, uint32_t opcode, uint32_t dummy,
			 uint32_t num_bytes, uint32_t *input)
{
	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1)) {
		LOG_ERR("Faulty dummy bytes\n");
		return -1;
	}

	if ((num_bytes > 8) || (num_bytes == 0)) {
		LOG_ERR("Invalid number of bytes\n");
		return -1;
	}

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	uint32_t cmd = CAD_QSPI_FLASHCMD_OPCODE(opcode) | CAD_QSPI_FLASHCMD_ENRDDATA(0) |
		       CAD_QSPI_FLASHCMD_NUMRDDATABYTES(0) | CAD_QSPI_FLASHCMD_ENCMDADDR(0) |
		       CAD_QSPI_FLASHCMD_ENMODEBIT(0) | CAD_QSPI_FLASHCMD_NUMADDRBYTES(0) |
		       CAD_QSPI_FLASHCMD_ENWRDATA(1) |
		       CAD_QSPI_FLASHCMD_NUMWRDATABYTES(num_bytes - 1) |
		       CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(dummy);

	sys_write32(input[0], cad_params->reg_base + CAD_QSPI_FLASHCMD_WRDATA0);

	if (num_bytes > 4) {
		sys_write32(input[1], cad_params->reg_base + CAD_QSPI_FLASHCMD_WRDATA1);
	}

	return cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs, cmd);
}

int cad_qspi_stig_addr_cmd(struct cad_qspi_params *cad_params, uint32_t opcode, uint32_t dummy,
			   uint32_t addr)
{
	uint32_t cmd;

	if (dummy > ((1 << CAD_QSPI_FLASHCMD_NUM_DUMMYBYTES_MAX) - 1)) {
		LOG_ERR("Faulty dummy bytes\n");
		return -1;
	}

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	cmd = CAD_QSPI_FLASHCMD_OPCODE(opcode) | CAD_QSPI_FLASHCMD_NUMDUMMYBYTES(dummy) |
	      CAD_QSPI_FLASHCMD_ENCMDADDR(1) | CAD_QSPI_FLASHCMD_NUMADDRBYTES(2);

	sys_write32(addr, cad_params->reg_base + CAD_QSPI_FLASHCMD_ADDR);

	return cad_qspi_stig_cmd_helper(cad_params, cad_params->cad_qspi_cs, cmd);
}

int cad_qspi_device_bank_select(struct cad_qspi_params *cad_params, uint32_t bank)
{
	int status = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN, 0);

	if (status != 0) {
		return status;
	}

	status = cad_qspi_stig_wr_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN_EXT_REG, 0, 1, &bank);

	if (status != 0) {
		return status;
	}

	return cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WRDIS, 0);
}

int cad_qspi_device_status(struct cad_qspi_params *cad_params, uint64_t *status)
{
	return cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDSR, 0, 1, status);
}

#if CAD_QSPI_MICRON_N25Q_SUPPORT
int cad_qspi_n25q_enable(struct cad_qspi_params *cad_params)
{
	cad_qspi_set_read_config(cad_params, QSPI_FAST_READ, CAD_QSPI_INST_SINGLE,
				 CAD_QSPI_ADDR_FASTREAD, CAT_QSPI_ADDR_SINGLE_IO, 1, 0);
	cad_qspi_set_write_config(cad_params, QSPI_WRITE, 0, 0, 0);

	return 0;
}

int cad_qspi_n25q_wait_for_program_and_erase(struct cad_qspi_params *cad_params, int program_only)
{
	uint64_t status, flag_sr;
	int count = 0;
	int ret = 0;

	while (count < CAD_QSPI_COMMAND_TIMEOUT) {
		ret = cad_qspi_device_status(cad_params, &status);
		if (ret != 0) {
			LOG_ERR("Error getting device status\n");
			return -1;
		}
		if (!CAD_QSPI_STIG_SR_BUSY(status)) {
			break;
		}
		count++;
	}

	if (count >= CAD_QSPI_COMMAND_TIMEOUT) {
		LOG_ERR("Timed out waiting for idle\n");
		return -1;
	}

	count = 0;

	while (count < CAD_QSPI_COMMAND_TIMEOUT) {
		ret = cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDFLGSR, 0, 1,
					     &flag_sr);
		if (ret != 0) {
			LOG_ERR("Error waiting program and erase.\n");
			return ret;
		}

		if ((program_only && CAD_QSPI_STIG_FLAGSR_PROGRAMREADY(flag_sr)) ||
		    (!program_only && CAD_QSPI_STIG_FLAGSR_ERASEREADY(flag_sr))) {
			break;
		}
	}

	if (count >= CAD_QSPI_COMMAND_TIMEOUT) {
		LOG_ERR("Timed out waiting for program and erase\n");
	}

	if ((program_only && CAD_QSPI_STIG_FLAGSR_PROGRAMERROR(flag_sr)) ||
	    (!program_only && CAD_QSPI_STIG_FLAGSR_ERASEERROR(flag_sr))) {
		LOG_ERR("Error programming/erasing flash\n");
		cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_CLFSR, 0);
		return -1;
	}

	return 0;
}
#endif

int cad_qspi_indirect_read_start_bank(struct cad_qspi_params *cad_params, uint32_t flash_addr,
				      uint32_t num_bytes)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(flash_addr, cad_params->reg_base + CAD_QSPI_INDRDSTADDR);
	sys_write32(num_bytes, cad_params->reg_base + CAD_QSPI_INDRDCNT);
#ifndef CONFIG_CAD_QSPI_INTERRUPT_SUPPORT
	sys_write32(CAD_QSPI_INDRD_START | CAD_QSPI_INDRD_IND_OPS_DONE,
		    cad_params->reg_base + CAD_QSPI_INDRD);
#endif
	return 0;
}

int cad_qspi_indirect_write_start_bank(struct cad_qspi_params *cad_params, uint32_t flash_addr,
				       uint32_t num_bytes)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_write32(flash_addr, cad_params->reg_base + CAD_QSPI_INDWRSTADDR);
	sys_write32(num_bytes, cad_params->reg_base + CAD_QSPI_INDWRCNT);
	sys_write32(CAD_QSPI_INDWR_START | CAD_QSPI_INDWR_INDDONE,
		    cad_params->reg_base + CAD_QSPI_INDWR);

	return 0;
}

int cad_qspi_indirect_write_finish(struct cad_qspi_params *cad_params)
{

	uint32_t status, count = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	while (count < CAD_QSPI_COMMAND_TIMEOUT) {
		status = CAD_QSPI_INDWR_RDSTAT(sys_read32(cad_params->reg_base + CAD_QSPI_INDWR));
		if ((status & 0x60) == 0) {
			sys_write32(0x60, cad_params->reg_base + CAD_QSPI_INDWR);
			break;
		}
		count++;
	}

	if (count >= CAD_QSPI_COMMAND_TIMEOUT) {
		LOG_ERR("Timed out waiting for idle\n");
		return -1;
	}

#if CAD_QSPI_MICRON_N25Q_SUPPORT
	return cad_qspi_n25q_wait_for_program_and_erase(cad_params, 1);
#else
	return 0;
#endif
}

int cad_qspi_enable(struct cad_qspi_params *cad_params)
{
	int status;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	sys_set_bits(cad_params->reg_base + CAD_QSPI_CFG, CAD_QSPI_CFG_ENABLE);

#if CAD_QSPI_MICRON_N25Q_SUPPORT
	status = cad_qspi_n25q_enable(cad_params);
	if (status != 0) {
		return status;
	}
#endif
	return 0;
}

int cad_qspi_enable_subsector_bank(struct cad_qspi_params *cad_params, uint32_t addr)
{
	int status = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN, 0);

	if (status != 0) {
		return status;
	}

	status = cad_qspi_stig_addr_cmd(cad_params, CAD_QSPI_STIG_OPCODE_SUBSEC_ERASE, 0, addr);
	if (status != 0) {
		return status;
	}

#if CAD_QSPI_MICRON_N25Q_SUPPORT
	status = cad_qspi_n25q_wait_for_program_and_erase(cad_params, 0);
#endif
	return status;
}

int cad_qspi_erase_subsector(struct cad_qspi_params *cad_params, uint32_t addr)
{
	int status = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_device_bank_select(cad_params, addr >> 24);
	if (status != 0) {
		return status;
	}

	return cad_qspi_enable_subsector_bank(cad_params, addr);
}

int cad_qspi_erase_sector(struct cad_qspi_params *cad_params, uint32_t addr)
{
	int status = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_device_bank_select(cad_params, addr >> 24);

	if (status != 0) {
		return status;
	}

	status = cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_WREN, 0);

	if (status != 0) {
		return status;
	}

	status = cad_qspi_stig_addr_cmd(cad_params, CAD_QSPI_STIG_OPCODE_SEC_ERASE, 0, addr);
	if (status != 0) {
		return status;
	}

#if CAD_QSPI_MICRON_N25Q_SUPPORT
	status = cad_qspi_n25q_wait_for_program_and_erase(cad_params, 0);
#endif
	return status;
}

void cad_qspi_calibration(struct cad_qspi_params *cad_params, uint32_t dev_clk,
			  uint32_t qspi_clk_mhz)
{
	int status;
	uint32_t dev_sclk_mhz = 27; /*min value to get biggest 0xF div factor*/
	uint32_t data_cap_delay;
	uint64_t sample_rdid;
	uint64_t rdid;
	uint32_t div_actual;
	uint32_t div_bits;
	int first_pass, last_pass;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return;
	}

	/*1.  Set divider to bigger value (slowest SCLK)
	 *2.  RDID and save the value
	 */
	div_actual = (qspi_clk_mhz + (dev_sclk_mhz - 1)) / dev_sclk_mhz;
	div_bits = (((div_actual + 1) / 2) - 1);
	status = cad_qspi_set_baudrate_div(cad_params, 0xf);

	status = cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDID, 0, 3, &sample_rdid);
	if (status != 0) {
		return;
	}

	/*3. Set divider to the intended frequency
	 *4.  Set the read delay = 0
	 *5.  RDID and check whether the value is same as item 2
	 *6.  Increase read delay and compared the value against item 2
	 *7.  Find the range of read delay that have same as
	 *    item 2 and divide it to 2
	 */
	div_actual = (CONVERTTOMHZ(qspi_clk_mhz) + (dev_clk - 1)) / dev_clk;
	div_bits = (((div_actual + 1) / 2) - 1);
	status = cad_qspi_set_baudrate_div(cad_params, div_bits);

	if (status != 0) {
		return;
	}

	data_cap_delay = 0;
	first_pass = -1;
	last_pass = -1;

	do {

		status = cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDID, 0, 3, &rdid);
		if (status != 0) {
			break;
		}

		if (rdid == sample_rdid) {
			if (first_pass == -1) {
				first_pass = data_cap_delay;
			} else {
				last_pass = data_cap_delay;
			}
		}

		data_cap_delay++;

		sys_write32(CAD_QSPI_RDDATACAP_BYP(1) | CAD_QSPI_RDDATACAP_DELAY(data_cap_delay),
			    cad_params->reg_base + CAD_QSPI_RDDATACAP);

	} while (data_cap_delay < 0x10);

	if (first_pass > 0) {
		int diff = first_pass - last_pass;

		data_cap_delay = first_pass + diff / 2;
	}

	sys_write32(CAD_QSPI_RDDATACAP_BYP(1) | CAD_QSPI_RDDATACAP_DELAY(data_cap_delay),
		    cad_params->reg_base + CAD_QSPI_RDDATACAP);
	status = cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDID, 0, 3, &rdid);

	if (status != 0) {
		return;
	}
}

int cad_qspi_int_disable(struct cad_qspi_params *cad_params, uint32_t mask)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if (cad_qspi_idle(cad_params) == 0) {
		LOG_ERR("QSPI is not idle\n");
		return -1;
	}

	if ((CAD_QSPI_INT_STATUS_ALL & mask) == 0) {
		return -1;
	}

	sys_write32(~mask, cad_params->reg_base + CAD_QSPI_IRQMSK);
	return 0;
}

void cad_qspi_set_chip_select(struct cad_qspi_params *cad_params, int cs)
{
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return;
	}

	cad_params->cad_qspi_cs = cs;
}

int cad_qspi_init(struct cad_qspi_params *cad_params, uint32_t clk_phase, uint32_t clk_pol,
		  uint32_t csda, uint32_t csdads, uint32_t cseot, uint32_t cssot,
		  uint32_t rddatacap)
{
	int status = 0;
	uint32_t qspi_desired_clk_freq;
	uint64_t rdid;
	uint32_t cap_code;

	LOG_INF("Initializing Qspi");
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter");
		return -EINVAL;
	}

	if (cad_qspi_idle(cad_params) == 0) {
		LOG_ERR("Device not idle");
		return -EBUSY;
	}

	status = cad_qspi_timing_config(cad_params, clk_phase, clk_pol, csda, csdads, cseot, cssot,
					rddatacap);

	if (status != 0) {
		LOG_ERR("Config set timing failure\n");
		return status;
	}

	sys_write32(CAD_QSPI_REMAPADDR_VALUE_SET(0), cad_params->reg_base + CAD_QSPI_REMAPADDR);

	status = cad_qspi_int_disable(cad_params, CAD_QSPI_INT_STATUS_ALL);
	if (status != 0) {
		LOG_ERR("Failed to disable Qspi\n");
		return status;
	}

	cad_qspi_set_baudrate_div(cad_params, 0xf);
	status = cad_qspi_enable(cad_params);
	if (status != 0) {
		LOG_ERR("failed to enable Qspi\n");
		return status;
	}

	qspi_desired_clk_freq = 90;
	cad_qspi_calibration(cad_params, qspi_desired_clk_freq, cad_params->clk_rate);

	status = cad_qspi_stig_read_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RDID, 0, 3, &rdid);

	if (status != 0) {
		LOG_ERR("Error reading RDID\n");
		return status;
	}

	/*
	 * NOTE: The Size code seems to be a form of BCD (binary coded decimal).
	 * The first nibble is the 10's digit and the second nibble is the 1's
	 * digit in the number of bytes.
	 *
	 * Capacity ID samples:
	 * 0x15 :   16 Mb =>   2 MiB => 1 << 21 ; BCD=15
	 * 0x16 :   32 Mb =>   4 MiB => 1 << 22 ; BCD=16
	 * 0x17 :   64 Mb =>   8 MiB => 1 << 23 ; BCD=17
	 * 0x18 :  128 Mb =>  16 MiB => 1 << 24 ; BCD=18
	 * 0x19 :  256 Mb =>  32 MiB => 1 << 25 ; BCD=19
	 * 0x1a
	 * 0x1b
	 * 0x1c
	 * 0x1d
	 * 0x1e
	 * 0x1f
	 * 0x20 :  512 Mb =>  64 MiB => 1 << 26 ; BCD=20
	 * 0x21 : 1024 Mb => 128 MiB => 1 << 27 ; BCD=21
	 */

	cap_code = CAD_QSPI_STIG_RDID_CAPACITYID(rdid);
	if (((cap_code >> 4) < 0x9) && ((cap_code & 0xf) < 0x9)) {
		uint32_t decoded_cap = (uint32_t)(((cap_code >> 4) * 10) + (cap_code & 0xf));

		decoded_cap = decoded_cap > 25?25:decoded_cap;
		cad_params->qspi_device_size = (uint32_t)(1 << (decoded_cap + 6));
		LOG_INF("QSPI Capacity: %x", cad_params->qspi_device_size);

	} else {
		LOG_ERR("Invalid CapacityID encountered: 0x%02x", cap_code);
		return -1;
	}

	status = cad_qspi_configure_dev_size(cad_params, cad_params->qspi_device_address_byte,
				    cad_params->qspi_device_page_size,
				    cad_params->qspi_device_bytes_per_block);
	if (status != 0) {
		LOG_ERR("Failed to configure device size\n");
		return status;
	}

	status = sys_read32(cad_params->reg_base + CAD_QSPI_INTRSTREG);
	sys_write32(status, cad_params->reg_base + CAD_QSPI_INTRSTREG);

	LOG_INF("Flash size: %d Bytes", cad_params->qspi_device_size);

	return status;
}

int cad_qspi_indirect_page_bound_write(struct cad_qspi_params *cad_params, uint32_t offset,
				       uint8_t *buffer, uint32_t len)
{
	int status = 0;
	uint32_t write_count, write_capacity, *write_data, space, write_fill_level, sram_partition;
	uint8_t *write_byte_data;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_indirect_write_start_bank(cad_params, offset, len);

	if (status != 0) {
		return status;
	}

	write_count = 0;
	sram_partition =
		CAD_QSPI_SRAMPART_ADDR(sys_read32(cad_params->reg_base + CAD_QSPI_SRAMPART));
	write_capacity = (uint32_t)CAD_QSPI_SRAM_FIFO_ENTRY_COUNT - sram_partition;

	while (write_count < len) {
		write_fill_level = CAD_QSPI_SRAMFILL_INDWRPART(
			sys_read32(cad_params->reg_base + CAD_QSPI_SRAMFILL));
		space = MIN(write_capacity - write_fill_level,
			    (len - write_count) / sizeof(uint32_t));
		write_data = (uint32_t *)(buffer + write_count);
		for (uint32_t i = 0; i < space; ++i) {
			sys_write32(*write_data++, cad_params->data_base);
		}
		write_count += space * sizeof(uint32_t);

		if ((len - write_count) < 4) {
			write_byte_data = (uint8_t *)write_data;
			while (len - write_count) {
				sys_write8(*write_byte_data++, cad_params->data_base);
				write_count++;
			}
		}
	}
	return cad_qspi_indirect_write_finish(cad_params);
}

int cad_qspi_read_bank(struct cad_qspi_params *cad_params, uint8_t *buffer, uint32_t offset,
		       uint32_t size)
{
	int status;
	uint32_t read_count = 0;
	uint8_t *read_data = buffer;
	int level = 1;
	uint32_t read_bytes = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_indirect_read_start_bank(cad_params, offset, size);

	if (status != 0) {
		return status;
	}

	while (read_count < size) {
		level = CAD_QSPI_SRAMFILL_INDRDPART(
			sys_read32(cad_params->reg_base + CAD_QSPI_SRAMFILL));
		read_bytes = MIN(level * 4, size);
		for (uint32_t i = 0; i < read_bytes; ++i) {
			*read_data++ = sys_read8(cad_params->data_base);
		}

		read_count += read_bytes;
	}

	return 0;
}

int cad_qspi_write_bank(struct cad_qspi_params *cad_params, uint32_t offset, uint8_t *buffer,
			uint32_t size)
{
	int status = 0;
#ifndef CONFIG_CAD_QSPI_INTERRUPT_SUPPORT
	uint32_t page_offset = offset & (CAD_QSPI_PAGE_SIZE - 1);
	uint32_t write_size = MIN(size, CAD_QSPI_PAGE_SIZE - page_offset);
#endif
	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}
#ifdef CONFIG_CAD_QSPI_INTERRUPT_SUPPORT
	status = cad_qspi_indirect_page_bound_write_using_interrupt(cad_params, offset,
			buffer, size);
	return status;
#else
	while (size) {
		status = cad_qspi_indirect_page_bound_write(cad_params, offset, buffer, write_size);
		if (status != 0) {
			break;
		}

		offset += write_size;
		buffer += write_size;
		size -= write_size;
		write_size = MIN(size, CAD_QSPI_PAGE_SIZE);
	}
	return status;
#endif
}

int cad_qspi_read(struct cad_qspi_params *cad_params, void *buffer, uint32_t offset, uint32_t size)
{
	uint32_t bank_count, bank_addr, bank_offset, copy_len;
	uint8_t *read_data;
	int status;

	status = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if ((offset >= cad_params->qspi_device_size) ||
	    (offset + size - 1 >= cad_params->qspi_device_size) || (size == 0)) {
		LOG_ERR("Invalid read parameter\n");
		return -EINVAL;
	}

	if ((offset >= cad_params->qspi_device_size) ||
	    (offset + size - 1 >= cad_params->qspi_device_size) || (size == 0)) {
		LOG_ERR("Invalid read parameter\n");
		return -EINVAL;
	}

	if (CAD_QSPI_INDRD_RD_STAT(sys_read32(cad_params->reg_base + CAD_QSPI_INDRD))) {
		LOG_ERR("Read in progress\n");
		return -ENOTBLK;
	}

	/*
	 * bank_count : Number of bank(s) affected, including partial banks.
	 * bank_addr  : Aligned address of the first bank,
	 *		including partial bank.
	 * bank_ofst  : The offset of the bank to read.
	 *		Only used when reading the first bank.
	 */
	bank_count = CAD_QSPI_BANK_ADDR(offset + size - 1) - CAD_QSPI_BANK_ADDR(offset) + 1;
	bank_addr = offset & CAD_QSPI_BANK_ADDR_MSK;
	bank_offset = offset & (CAD_QSPI_BANK_SIZE - 1);

	read_data = (uint8_t *)buffer;

	copy_len = MIN(size, CAD_QSPI_BANK_SIZE - bank_offset);

	for (uint32_t i = 0; i < bank_count; ++i) {
		status = cad_qspi_device_bank_select(cad_params, CAD_QSPI_BANK_ADDR(bank_addr));
		if (status != 0) {
			break;
		}
#ifdef CONFIG_CAD_QSPI_INTERRUPT_SUPPORT
		status = cad_qspi_read_bank_using_interrupt(cad_params, read_data,
					bank_offset, copy_len);
#else
		status = cad_qspi_read_bank(cad_params, read_data, bank_offset,
					copy_len);
#endif
		if (status != 0) {
			break;
		}

		bank_addr += CAD_QSPI_BANK_SIZE;
		read_data += copy_len;
		size -= copy_len;
		bank_offset = 0;
		copy_len = MIN(size, CAD_QSPI_BANK_SIZE);
	}

	return status;
}

int cad_qspi_erase(struct cad_qspi_params *cad_params, uint32_t offset, uint32_t size)
{
	int status = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	uint32_t subsector_offset = offset & (cad_params->qspi_device_subsector_size - 1);
	uint32_t erase_size = MIN(size, cad_params->qspi_device_subsector_size - subsector_offset);

	while (size) {
		status = cad_qspi_erase_subsector(cad_params, offset);

		if (status != 0) {
			break;
		}

		offset += erase_size;
		size -= erase_size;
		erase_size = MIN(size, cad_params->qspi_device_subsector_size);
	}
	return status;
}

int cad_qspi_write(struct cad_qspi_params *cad_params, void *buffer, uint32_t offset, uint32_t size)
{
	int status, i;
	uint32_t bank_count, bank_addr, bank_offset, copy_len;
	uint8_t *write_data;

	status = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	if ((offset >= cad_params->qspi_device_size) ||
	    (offset + size - 1 >= cad_params->qspi_device_size) || (size == 0)) {
		LOG_ERR("Invalid write parameter\n");
		return -EINVAL;
	}

	if (CAD_QSPI_INDWR_RDSTAT(sys_read32(cad_params->reg_base + CAD_QSPI_INDWR))) {
		LOG_ERR("QSPI Error: Write in progress\n");
		return -ENOTBLK;
	}

	bank_count = CAD_QSPI_BANK_ADDR(offset + size - 1) - CAD_QSPI_BANK_ADDR(offset) + 1;
	bank_addr = offset & CAD_QSPI_BANK_ADDR_MSK;
	bank_offset = offset & (CAD_QSPI_BANK_SIZE - 1);

	write_data = buffer;

	copy_len = MIN(size, CAD_QSPI_BANK_SIZE - bank_offset);

	for (i = 0; i < bank_count; ++i) {
		status = cad_qspi_device_bank_select(cad_params, CAD_QSPI_BANK_ADDR(bank_addr));

		if (status != 0) {
			break;
		}

		status = cad_qspi_write_bank(cad_params, bank_offset, write_data, copy_len);
		if (status != 0) {
			break;
		}

		bank_addr += CAD_QSPI_BANK_SIZE;
		write_data += copy_len;
		size -= copy_len;
		bank_offset = 0;

		copy_len = MIN(size, CAD_QSPI_BANK_SIZE);
	}
	return status;
}

int cad_qspi_update(struct cad_qspi_params *cad_params, void *Buffer, uint32_t offset,
		    uint32_t size)
{
	int status = 0;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	status = cad_qspi_erase(cad_params, offset, size);

	if (status != 0) {
		LOG_ERR("Erase Failed\n");
		return status;
	}

	return cad_qspi_write(cad_params, Buffer, offset, size);
}

void cad_qspi_reset(struct cad_qspi_params *cad_params)
{
	cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RESET_EN, 0);
	cad_qspi_stig_cmd(cad_params, CAD_QSPI_STIG_OPCODE_RESET_MEM, 0);
}

#ifdef CONFIG_CAD_QSPI_INTERRUPT_SUPPORT
uint32_t cad_qspi_read_data_using_interrupt(struct cad_qspi_params *cad_params, uint8_t *buffer,
			uint32_t offset, uint32_t size)
{
	uint8_t *read_data = buffer;
	uint32_t *read_word = (uint32_t *)buffer;
	uint32_t transferred_bytes = 0;
	volatile int level = 1;
	uint32_t read_bytes;

	if (size >= sizeof(uint32_t)) {
		level = CAD_QSPI_SRAMFILL_INDRDPART(
				sys_read32(cad_params->reg_base + CAD_QSPI_SRAMFILL));

		read_bytes = MIN(level, size / sizeof(uint32_t));
		for (uint32_t i = 0; i < read_bytes; ++i) {
			*read_word++ = sys_read32(cad_params->data_base);
		}
		transferred_bytes = (read_bytes * sizeof(uint32_t));
		read_data = (uint8_t *)read_word;
		size -= transferred_bytes;
	}

	if ((size < 4)) {
		level = CAD_QSPI_SRAMFILL_INDRDPART(
				sys_read32(cad_params->reg_base + CAD_QSPI_SRAMFILL));
		read_bytes = MIN(level*sizeof(uint32_t), size);

		for (uint32_t i = 0; i < read_bytes; ++i) {
			*read_data++ = sys_read8(cad_params->data_base);
		}

		transferred_bytes += read_bytes;
	}
	return transferred_bytes;
}

int cad_qspi_read_bank_using_interrupt(struct cad_qspi_params *cad_params, uint8_t *buffer,
				uint32_t offset, uint32_t size)
{
	int status;
	uint32_t byte_transferred, remaining_bytes, level;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* Disable RX interrupt */
	cad_qspi_disable_interrupt_mask(cad_params, CAD_QSPI_INDOPDONE|CAD_QSPI_INDXFRLVL);

	status = cad_qspi_indirect_read_start_bank(cad_params, offset, size);
	if (status != 0) {
		return status;
	}

	/* Store the calling parameter */
	read_req = k_malloc(sizeof(struct transfer_params));
	read_req->requested_bytes = size;
	read_req->transferred_bytes = 0;
	read_req->offset = offset;
	read_req->buffer = buffer;

	/* Read interrupt status */
	status = sys_read32(cad_params->reg_base + CAD_QSPI_INTRSTREG);

	/* Clear interrupt status*/
	sys_write32(status, cad_params->reg_base + CAD_QSPI_INTRSTREG);

	remaining_bytes = read_req->requested_bytes - read_req->transferred_bytes;

	level = sys_read32(cad_params->reg_base + CAD_QSPI_SRAMPART);
	sys_write32(level*4, cad_params->reg_base + CAD_QSPI_INDRDWATER);

	/* Enable RX interrupt */
	cad_qspi_enable_interrupt_mask(cad_params, CAD_QSPI_INDOPDONE | CAD_QSPI_INDXFRLVL);

	/* Trigger Indirect Read access */
	sys_write32(CAD_QSPI_INDRD_START | CAD_QSPI_INDRD_IND_OPS_DONE,
			cad_params->reg_base + CAD_QSPI_INDRD);
	while (remaining_bytes) {
		/* Wait to receive data */
		k_sem_take(&cad_params->qspi_intr_sem, K_FOREVER);

		/* Enable RX interrupt */
		cad_qspi_enable_interrupt_mask(cad_params, CAD_QSPI_INDOPDONE | CAD_QSPI_INDXFRLVL);

		/* Read the date form internal buffer*/
		byte_transferred = cad_qspi_read_data_using_interrupt(cad_params, read_req->buffer,
						read_req->offset, remaining_bytes);

		buffer += byte_transferred;
		offset += byte_transferred;
		read_req->transferred_bytes += byte_transferred;
		read_req->buffer = buffer;
		read_req->offset = offset;

		remaining_bytes = read_req->requested_bytes - read_req->transferred_bytes;
	}

	/* Disable RX interrupt */
	cad_qspi_disable_interrupt_mask(cad_params, CAD_QSPI_INDOPDONE|CAD_QSPI_INDXFRLVL|0x1000);
	free(read_req);

	return 0;
}

int cad_qspi_indirect_write_page_data(struct cad_qspi_params *cad_params,
						uint32_t offset, uint8_t *buffer, uint32_t len)
{
	int status = 0;
	uint32_t write_count, write_capacity, space, write_fill_level, sram_partition;
	uint8_t *write_byte_data;
	uint32_t *write_data;

	uint32_t page_offset = offset & (CAD_QSPI_PAGE_SIZE - 1);
	uint32_t write_size = MIN(len, CAD_QSPI_PAGE_SIZE - page_offset);

	status = cad_qspi_indirect_write_start_bank(cad_params, offset, write_size);
	if (status != 0) {
		LOG_ERR("Failed to set bank parameter\n");
		return status;
	}

	write_count = 0;
	sram_partition =
		CAD_QSPI_SRAMPART_ADDR(sys_read32(cad_params->reg_base + CAD_QSPI_SRAMPART));
	write_capacity = (uint32_t)CAD_QSPI_SRAM_FIFO_ENTRY_COUNT - sram_partition;

	while (write_count < write_size) {
		write_fill_level = CAD_QSPI_SRAMFILL_INDWRPART(
			sys_read32(cad_params->reg_base + CAD_QSPI_SRAMFILL));

		space = MIN(write_capacity - write_fill_level,
			    (write_size - write_count) / sizeof(uint32_t));

		write_data = (uint32_t *)(buffer + write_count);

		for (uint32_t i = 0; i < space; i++) {
			sys_write32(*write_data, cad_params->data_base);
			write_data++;
		}
		write_count += space * sizeof(uint32_t);

		if ((write_size - write_count) < 4) {
			write_byte_data = (uint8_t *)write_data;
			while (write_size - write_count) {
				sys_write8(*write_byte_data++, cad_params->data_base);
				write_count++;
			}
		}
	}

	return write_count;
}

int cad_qspi_indirect_page_bound_write_using_interrupt(struct cad_qspi_params *cad_params,
						uint32_t offset, uint8_t *buffer, uint32_t len)
{
	int status = 0;
	uint32_t byte_written;
	uint8_t *p_buffer = buffer;

	if (cad_params == NULL) {
		LOG_ERR("Wrong parameter\n");
		return -EINVAL;
	}

	/* Store the calling parameter */
	write_req = k_malloc(sizeof(struct transfer_params));
	write_req->requested_bytes = len;
	write_req->transferred_bytes = 0;
	write_req->offset = offset;
	write_req->buffer = buffer;

	/* Read interrupt status */
	status = sys_read32(cad_params->reg_base + CAD_QSPI_INTRSTREG);

	/* Clear interrupt status*/
	sys_write32(status, cad_params->reg_base + CAD_QSPI_INTRSTREG);

	/* Set the watermark level */
	sys_write32(WRITE_WATER_LEVEL_MARK, cad_params->reg_base + CAD_QSPI_INDWRWATER);

	if (len > WRITE_WATER_LEVEL_MARK) {
		cad_qspi_enable_interrupt_mask(cad_params, CAD_QSPI_INDOPDONE|
							CAD_QSPI_INDXFRLVL);
	} else {
		cad_qspi_enable_interrupt_mask(cad_params, CAD_QSPI_INDOPDONE);
	}
	while (write_req->requested_bytes > write_req->transferred_bytes) {
		byte_written = cad_qspi_indirect_write_page_data(cad_params,
				write_req->offset, write_req->buffer,
				(write_req->requested_bytes - write_req->transferred_bytes));

		write_req->transferred_bytes += byte_written;
		p_buffer += byte_written;
		write_req->offset += byte_written;
		write_req->buffer = p_buffer;

		cad_qspi_enable_interrupt_mask(cad_params, CAD_QSPI_INDOPDONE|
					CAD_QSPI_INDXFRLVL);
		k_sem_take(&cad_params->qspi_intr_sem, K_FOREVER);

	}
	free(write_req);

	return cad_qspi_indirect_write_finish(cad_params);
}

void cad_qspi_irq_handler_ll(struct cad_qspi_params *cad_params)
{
	volatile uint32_t status = 0;
	volatile uint32_t read_value = 0;

	/* Read interrupt status */
	status = sys_read32(cad_params->reg_base + CAD_QSPI_INTRSTREG);

	/* Clear interrupt status */
	sys_write32(status, cad_params->reg_base + CAD_QSPI_INTRSTREG);

	/* Check the watermark level reached for write FIFO or transfer done,
	 * then send the remaining bytes.
	 */
	if ((status & CAD_QSPI_INDWROPRCMP) || (status & CAD_QSPI_INDRDWATERLVLBRCH)) {
		/* Write Operation */
		/* Write operation is in progress and waterlevel mark reached */
		if ((sys_read32(cad_params->reg_base + CAD_QSPI_INDWR) & CAD_QSPI_INDWROPRCMP) &&
		   (status & CAD_QSPI_INDRDWATERLVLBRCH)) {
			cad_qspi_disable_interrupt_mask(cad_params,
				CAD_QSPI_INDOPDONE | CAD_QSPI_INDXFRLVL);
		}
		if (sys_read32(cad_params->reg_base + CAD_QSPI_INDWR) &
			CAD_QSPI_INDWRMULTIOPSCOMP) {
			sys_write32(CAD_QSPI_INDWRMULTIOPSCOMP,
						cad_params->reg_base + CAD_QSPI_INDWR);
			cad_qspi_disable_interrupt_mask(cad_params,
				CAD_QSPI_INDOPDONE | CAD_QSPI_INDXFRLVL);
		} else {
			/* Read Operation */
			/* Read operation is in progress and waterlevel mark reached */
			if ((sys_read32(cad_params->reg_base + CAD_QSPI_INDRD) &
				CAD_QSPI_INDRDOPRCMP) && (status & CAD_QSPI_INDRDWATERLVLBRCH)) {
				cad_qspi_disable_interrupt_mask(cad_params, CAD_QSPI_INDOPDONE |
				CAD_QSPI_INDXFRLVL | CAD_QSPI_INDSRAMFULL);
			} else {
				read_value = sys_read32(cad_params->reg_base + CAD_QSPI_INDRD);
				if (read_value & CAD_QSPI_INDRDMULTIOPSCOMP) {
					sys_write32(read_value,
						cad_params->reg_base + CAD_QSPI_INDRD);
					/* Disable interrupt */
					cad_qspi_disable_interrupt_mask(cad_params,
						CAD_QSPI_INDOPDONE | CAD_QSPI_INDXFRLVL);
				}
			}
		}
	} else if (status & CAD_QSPI_INDSRAMFULL) {
		/* SRAM is FULL */
		/* Disable interrupt */
		cad_qspi_disable_interrupt_mask(cad_params, CAD_QSPI_INDSRAMFULL);
	}
	k_sem_give(&cad_params->qspi_intr_sem);
}
#endif
