/*
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <sys/device_mmio.h>
#include <sys/util.h>
#include <drivers/pcie/pcie.h>
#include <soc.h>


#ifdef UART_NS16550_ACCESS_IOPORT
/* Legacy I/O Port Access to a NS16550 UART */
#define IN(reg)       sys_in8(reg + UART_NS16550_ACCESS_IOPORT)
#define OUT(reg, val) sys_out8(val, reg + UART_NS16550_ACCESS_IOPORT)
#elif defined(X86_SOC_EARLY_SERIAL_PCIDEV)
/* "Modern" mapping of a UART into a PCI MMIO device.  The registers
 * are still bytes, but spaced at a 32 bit stride instead of packed
 * together.
 */
static mm_reg_t mmio;
#define IN(reg)       (sys_read32(mmio + (reg) * 4U) & 0xffU)
#define OUT(reg, val) sys_write32((uint32_t)(val) & 0xffU, mmio + (reg) * 4U)
#elif defined(X86_SOC_EARLY_SERIAL_MMIO8_ADDR)
/* Still other devices use a MMIO region containing packed byte
 * registers
 */
#ifdef DEVICE_MMIO_IS_IN_RAM
static mm_reg_t mmio;
#define BASE mmio
#else
#define BASE X86_SOC_EARLY_SERIAL_MMIO8_ADDR
#endif /* DEVICE_MMIO_IS_IN_RAM */
#define IN(reg)       sys_read8(BASE + reg)
#define OUT(reg, val) sys_write8(val, BASE + reg)
#else
#error "Unsupported configuration"
#endif

#define REG_THR  0x00U  /* Transmitter holding reg. */
#define REG_IER  0x01U  /* Interrupt enable reg.    */
#define REG_FCR  0x02U  /* FIFO control reg.        */
#define REG_LCR  0x03U  /* Line control reg.        */
#define REG_MCR  0x04U  /* Modem control reg.       */
#define REG_LSR  0x05U  /* Line status reg.         */
#define REG_BRDL 0x00U  /* Baud rate divisor (LSB)  */
#define REG_BRDH 0x01U  /* Baud rate divisor (MSB)  */

#define IER_DISABLE     0x00U
#define LCR_8N1         (BIT(0) | BIT(1))
#define LCR_DLAB_SELECT BIT(7)
#define MCR_DTR         BIT(0)
#define MCR_RTS         BIT(1)
#define LSR_THRE        BIT(5)

#define FCR_FIFO    BIT(0)  /* enable XMIT and RCVR FIFO */
#define FCR_RCVRCLR BIT(1)  /* clear RCVR FIFO           */
#define FCR_XMITCLR BIT(2)  /* clear XMIT FIFO           */
#define FCR_FIFO_1  0x00U   /* 1 byte in RCVR FIFO       */

static bool early_serial_init_done;
static uint32_t suppressed_chars;

static void serout(uint8_t c)
{
	while ((IN(REG_LSR) & LSR_THRE) == 0U) {
	}
	OUT(REG_THR, c);
}

int arch_printk_char_out(int c)
{
	if (!early_serial_init_done) {
		suppressed_chars++;
		return c;
	}

	if (c == (int)'\n') {
		serout((uint8_t)'\r');
	}
	serout((uint8_t)c);
	return c;
}

void z_x86_early_serial_init(void)
{
#if defined(DEVICE_MMIO_IS_IN_RAM) && !defined(UART_NS16550_ACCESS_IOPORT)
#ifdef X86_SOC_EARLY_SERIAL_PCIDEV
	struct pcie_mbar mbar;
	pcie_get_mbar(X86_SOC_EARLY_SERIAL_PCIDEV, 0, &mbar);
	pcie_set_cmd(X86_SOC_EARLY_SERIAL_PCIDEV, PCIE_CONF_CMDSTAT_MEM, true);
	device_map(&mmio, mbar.phys_addr, mbar.size, K_MEM_CACHE_NONE);
#else
	device_map(&mmio, X86_SOC_EARLY_SERIAL_MMIO8_ADDR, 0x1000, K_MEM_CACHE_NONE);
#endif

#endif /* DEVICE_MMIO_IS_IN_RAM */

	OUT(REG_IER, IER_DISABLE);     /* Disable interrupts */
	OUT(REG_LCR, LCR_DLAB_SELECT); /* DLAB select */
	OUT(REG_BRDL, 1U);              /* Baud divisor = 1 */
	OUT(REG_BRDH, 0U);
	OUT(REG_LCR, LCR_8N1);         /* LCR = 8n1 + DLAB off */
	OUT(REG_MCR, MCR_DTR | MCR_RTS);

	/* Turn on FIFO. Some hardware needs this before transmitting */
	OUT(REG_FCR, FCR_FIFO | FCR_FIFO_1 | FCR_RCVRCLR | FCR_XMITCLR);

	early_serial_init_done = true;

	if (suppressed_chars != 0U) {
		printk("WARNING: %u chars lost before early serial init\n",
		       suppressed_chars);
	}
}
