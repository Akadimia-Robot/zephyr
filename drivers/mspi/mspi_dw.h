/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_MSPI_MSPI_DW_H_
#define ZEPHYR_DRIVERS_MSPI_MSPI_DW_H_

#ifdef __cplusplus
extern "C" {
#endif

#define CTRLR0_SPI_FRF_MASK	GENMASK(23, 22)
#define CTRLR0_SPI_FRF_STANDARD	0UL
#define CTRLR0_SPI_FRF_DUAL	1UL
#define CTRLR0_SPI_FRF_QUAD	2UL
#define CTRLR0_SPI_FRF_OCTAL	3UL
#define CTRLR0_TMOD_MASK	GENMASK(11, 10)
#define CTRLR0_TMOD_TX_RX	0UL
#define CTRLR0_TMOD_TX		1UL
#define CTRLR0_TMOD_RX		2UL
#define CTRLR0_TMOD_EEPROM	3UL
#define CTRLR0_SCPOL_BIT	BIT(9)
#define CTRLR0_SCPH_BIT		BIT(8)
#define CTRLR0_FRF_MASK		GENMASK(7, 6)
#define CTRLR0_FRF_SPI		0UL
#define CTRLR0_FRF_SSP		1UL
#define CTRLR0_FRF_MICROWIRE	2UL
#define CTRLR0_DFS_MASK		GENMASK(4, 0)

#define CTRLR1_NDF_MASK		GENMASK(15, 0)

#define SSIENR_SSIC_EN_BIT	BIT(0)

#define TXFTLR_TXFTHR_MASK	GENMASK(23, 16)
#define TXFTLR_TFT_MASK		GENMASK(7, 0)

#define RXFTLR_RFT_MASK		GENMASK(7, 0)

#define TXFLR_TXTFL_MASK	GENMASK(7, 0)

#define RXFLR_RXTFL_MASK	GENMASK(7, 0)

#define SR_BUSY_BIT	        BIT(0)

#define IMR_TXEIM_BIT		BIT(0)
#define IMR_TXOIM_BIT		BIT(1)
#define IMR_RXUIM_BIT		BIT(2)
#define IMR_RXOIM_BIT		BIT(3)
#define IMR_RXFIM_BIT		BIT(4)
#define IMR_MSTIM_BIT		BIT(5)

#define ISR_TXEIS_BIT		BIT(0)
#define ISR_TXOIS_BIT		BIT(1)
#define ISR_RXUIS_BIT		BIT(2)
#define ISR_RXOIS_BIT		BIT(3)
#define ISR_RXFIS_BIT		BIT(4)
#define ISR_MSTIS_BIT		BIT(5)

#define SPI_CTRLR0_CLK_STRETCH_EN_MASK		GENMASK(30, 30)
#define SPI_CTRLR0_XIP_PREFETCH_EN_MASK		GENMASK(29, 29)
#define SPI_CTRLR0_XIP_MBL_MASK			GENMASK(27, 26)
#define SPI_CTRLR0_SPI_RXDS_SIG_EN_MASK		GENMASK(25, 25)
#define SPI_CTRLR0_SPI_DM_EN_MASK		GENMASK(24, 24)
#define SPI_CTRLR0_RXDS_VL_EN_MASK		GENMASK(23, 23)
#define SPI_CTRLR0_SSIC_XIP_CONT_XFER_EN_MASK	GENMASK(21, 21)
#define SPI_CTRLR0_XIP_INST_EN_MASK		GENMASK(20, 20)
#define SPI_CTRLR0_XIP_DFS_HC_MASK		GENMASK(19, 19)
#define SPI_CTRLR0_SPI_RXDS_EN_MASK		GENMASK(18, 18)
#define SPI_CTRLR0_INST_DDR_EN_MASK		GENMASK(17, 17)
#define SPI_CTRLR0_SPI_DDR_EN_MASK		GENMASK(16, 16)
#define SPI_CTRLR0_WAIT_CYCLES_MASK		GENMASK(15, 11)
#define SPI_CTRLR0_WAIT_CYCLES_MAX		BIT_MASK(5)
#define SPI_CTRLR0_INST_L_MASK			GENMASK(9, 8)
#define SPI_CTRLR0_INST_L0			0UL
#define SPI_CTRLR0_INST_L4			1UL
#define SPI_CTRLR0_INST_L8			2UL
#define SPI_CTRLR0_INST_L16			3UL
#define SPI_CTRLR0_XIP_MD_BIT_EN_MASK		GENMASK(7, 7)
#define SPI_CTRLR0_ADDR_L_MASK			GENMASK(5, 2)
#define SPI_CTRLR0_TRANS_TYPE_MASK		GENMASK(1, 0)
#define SPI_CTRLR0_TRANS_TYPE_TT0		0UL
#define SPI_CTRLR0_TRANS_TYPE_TT1		1UL
#define SPI_CTRLR0_TRANS_TYPE_TT2		2UL
#define SPI_CTRLR0_TRANS_TYPE_TT3		3UL

/* Register access helpers. */
#define USES_AUX_REG(inst) + DT_INST_PROP(inst, aux_reg)
#define AUX_REG_INSTANCES (0 DT_INST_FOREACH_STATUS_OKAY(USES_AUX_REG))
#define BASE_ADDR(dev) (mm_reg_t)DEVICE_MMIO_GET(dev)

#if AUX_REG_INSTANCES != 0
static uint32_t aux_reg_read(const struct device *dev, uint32_t off)
{
	return sys_in32(BASE_ADDR(dev) + off/4);
}
static void aux_reg_write(uint32_t data, const struct device *dev, uint32_t off)
{
	sys_out32(data, BASE_ADDR(dev) + off/4);
}
#endif

#if AUX_REG_INSTANCES != DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)
static uint32_t reg_read(const struct device *dev, uint32_t off)
{
	return sys_read32(BASE_ADDR(dev) + off);
}
static void reg_write(uint32_t data, const struct device *dev, uint32_t off)
{
	sys_write32(data, BASE_ADDR(dev) + off);
}
#endif

#if AUX_REG_INSTANCES == 0
/* If no instance uses aux-reg access. */
#define DECLARE_REG_ACCESS()
#define DEFINE_REG_ACCESS(inst)
#define DEFINE_MM_REG_RD(reg, off) \
	static inline uint32_t read_##reg(const struct device *dev) \
	{ return reg_read(dev, off); }
#define DEFINE_MM_REG_WR(reg, off) \
	static inline void write_##reg(const struct device *dev, uint32_t data) \
	{ reg_write(data, dev, off); }

#elif AUX_REG_INSTANCES == DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)
/* If all instances use aux-reg access. */
#define DECLARE_REG_ACCESS()
#define DEFINE_REG_ACCESS(inst)
#define DEFINE_MM_REG_RD(reg, off) \
	static inline uint32_t read_##reg(const struct device *dev) \
	{ return aux_reg_read(dev, off); }
#define DEFINE_MM_REG_WR(reg, off) \
	static inline void write_##reg(const struct device *dev, uint32_t data) \
	{ aux_reg_write(data, dev, off); }

#else
/* If register access varies by instance. */
#define DECLARE_REG_ACCESS() \
	uint32_t (*read)(const struct device *dev, uint32_t off); \
	void (*write)(uint32_t data, const struct device *dev, uint32_t off)
#define DEFINE_REG_ACCESS(inst) \
	COND_CODE_1(DT_INST_PROP(inst, aux_reg), \
		(.read = aux_reg_read, \
		.write = aux_reg_write,), \
		(.read = reg_read, \
		.write = reg_write,))
#define DEFINE_MM_REG_RD(reg, off) \
	static inline uint32_t read_##reg(const struct device *dev) \
	{ \
		const struct mspi_dw_config *dev_config = dev->config; \
		return dev_config->read(dev, off); \
	}
#define DEFINE_MM_REG_WR(reg, off) \
	static inline void write_##reg(const struct device *dev, uint32_t data) \
	{ \
		const struct mspi_dw_config *dev_config = dev->config; \
		dev_config->write(data, dev, off); \
	}
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_MSPI_MSPI_DW_H_ */
