/*
 * Copyright (c) 2023 Realtek, SIBG-SD7
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * @brief GPIO Controller (GPIO)
 */

typedef struct {
	volatile uint32_t GCR[132];
} GPIO_Type;

/* GCR */
#define GPIO_GCR_DIR_Pos      (0UL)
#define GPIO_GCR_DIR_Msk      BIT(GPIO_GCR_DIR_Pos)
#define GPIO_GCR_INDETEN_Pos  (1UL)
#define GPIO_GCR_INDETEN_Msk  BIT(GPIO_GCR_INDETEN_Pos)
#define GPIO_GCR_INVOLMD_Pos  (2UL)
#define GPIO_GCR_INVOLMD_Msk  BIT(GPIO_GCR_INVOLMD_Pos)
#define GPIO_GCR_PINSTS_Pos   (3UL)
#define GPIO_GCR_PINSTS_Msk   BIT(GPIO_GCR_PINSTS_Pos)
#define GPIO_GCR_MFCTRL_Pos   (8UL)
#define GPIO_GCR_MFCTRL_Msk   GENMASK(10, 8)
#define GPIO_GCR_OUTDRV_Pos   (11UL)
#define GPIO_GCR_OUTDRV_Msk   BIT(GPIO_GCR_OUTDRV_Pos)
#define GPIO_GCR_SLEWRATE_Pos (12UL)
#define GPIO_GCR_SLEWRATE_Msk BIT(GPIO_GCR_SLEWRATE_Pos)
#define GPIO_GCR_PULLDWEN_Pos (13UL)
#define GPIO_GCR_PULLDWEN_Msk BIT(GPIO_GCR_PULLDWEN_Pos)
#define GPIO_GCR_PULLUPEN_Pos (14UL)
#define GPIO_GCR_PULLUPEN_Msk BIT(GPIO_GCR_PULLUPEN_Pos)
#define GPIO_GCR_SCHEN_Pos    (15UL)
#define GPIO_GCR_SCHEN_Msk    BIT(GPIO_GCR_SCHEN_Pos)
#define GPIO_GCR_OUTMD_Pos    (16UL)
#define GPIO_GCR_OUTMD_Msk    BIT(GPIO_GCR_OUTMD_Pos)
#define GPIO_GCR_OUTCTRL_Pos  (17UL)
#define GPIO_GCR_OUTCTRL_Msk  BIT(GPIO_GCR_OUTCTRL_Pos)
#define GPIO_GCR_INTCTRL_Pos  (24UL)
#define GPIO_GCR_INTCTRL_Msk  GENMASK(26, 24)
#define GPIO_GCR_INTEN_Pos    (28UL)
#define GPIO_GCR_INTEN_Msk    BIT(GPIO_GCR_INTEN_Pos)
#define GPIO_GCR_INTSTS_Pos   (31UL)
#define GPIO_GCR_INTSTS_Msk   BIT(GPIO_GCR_INTSTS_Pos)
