/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/byteorder.h>
#include <bluetooth/bluetooth.h>

#include "hal/cpu.h"
#include "hal/ccm.h"
#include "hal/ticker.h"

#include "util/util.h"
#include "util/mem.h"
#include "util/memq.h"
#include "util/mfifo.h"
#include "util/mayfly.h"

#include "ticker/ticker.h"

#include "pdu.h"

#include "lll.h"
#include "lll/lll_vendor.h"
#include "lll/lll_adv_types.h"
#include "lll_adv.h"
#include "lll/lll_adv_pdu.h"
#include "lll_conn.h"
#include "lll_adv_iso.h"

#include "ull_internal.h"
#include "ull_adv_types.h"
#include "ull_adv_internal.h"
#include "ull_chan_internal.h"

#include "ll.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#define LOG_MODULE_NAME bt_ctlr_ull_adv_iso
#include "common/log.h"
#include "hal/debug.h"

static uint32_t ull_adv_iso_start(struct ll_adv_iso_set *adv_iso,
				  uint32_t ticks_anchor);
static void mfy_iso_offset_get(void *param);
static inline struct pdu_big_info *big_info_get(struct pdu_adv *pdu);
static inline void big_info_offset_fill(struct pdu_big_info *bi,
					uint32_t ticks_offset,
					uint32_t start_us);
static int init_reset(void);
static inline struct ll_adv_iso_set *ull_adv_iso_get(uint8_t handle);
static void ticker_cb(uint32_t ticks_at_expire, uint32_t remainder,
		      uint16_t lazy, uint8_t force, void *param);
static void ticker_op_cb(uint32_t status, void *param);
static void tx_lll_flush(void *param);
static void ticker_op_stop_cb(uint32_t status, void *param);

static memq_link_t link_lll_prepare;
static struct mayfly mfy_lll_prepare = {0, 0, &link_lll_prepare, NULL, NULL};

static struct ll_adv_iso_set ll_adv_iso[BT_CTLR_ADV_SET];
static void *adv_iso_free;

uint8_t ll_big_create(uint8_t big_handle, uint8_t adv_handle, uint8_t num_bis,
		      uint32_t sdu_interval, uint16_t max_sdu,
		      uint16_t max_latency, uint8_t rtn, uint8_t phy,
		      uint8_t packing, uint8_t framing, uint8_t encryption,
		      uint8_t *bcode)
{
	uint8_t hdr_data[1 + sizeof(uint8_t *)];
	struct lll_adv_sync *lll_adv_sync;
	struct lll_adv_iso *lll_adv_iso;
	struct ll_adv_iso_set *adv_iso;
	struct pdu_adv *pdu_prev, *pdu;
	struct pdu_big_info *big_info;
	uint8_t pdu_big_info_size;
	uint32_t ticks_anchor_iso;
	memq_link_t *link_cmplt;
	memq_link_t *link_term;
	struct ll_adv_set *adv;
	uint16_t iso_interval;
	uint8_t ter_idx;
	uint8_t *acad;
	uint32_t ret;
	uint8_t err;

	adv_iso = ull_adv_iso_get(big_handle);

	/* Already created */
	if (!adv_iso || adv_iso->lll.adv) {
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	/* No advertising set created */
	adv = ull_adv_is_created_get(adv_handle);
	if (!adv) {
		return BT_HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
	}

	/* Does not identify a periodic advertising train or
	 * the periodic advertising trains is already associated
	 * with another BIG.
	 */
	lll_adv_sync = adv->lll.sync;
	if (!lll_adv_sync || lll_adv_sync->iso) {
		return BT_HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
	}

	if (IS_ENABLED(CONFIG_BT_CTLR_PARAM_CHECK)) {
		if (num_bis == 0 || num_bis > 0x1F) {
			return BT_HCI_ERR_INVALID_PARAM;
		}

		if (sdu_interval < 0x000100 || sdu_interval > 0x0FFFFF) {
			return BT_HCI_ERR_INVALID_PARAM;
		}

		if (max_sdu < 0x0001 || max_sdu > 0x0FFF) {
			return BT_HCI_ERR_INVALID_PARAM;
		}

		if (max_latency > 0x0FA0) {
			return BT_HCI_ERR_INVALID_PARAM;
		}

		if (rtn > 0x0F) {
			return BT_HCI_ERR_INVALID_PARAM;
		}

		if (phy > (BT_HCI_LE_EXT_SCAN_PHY_1M |
			   BT_HCI_LE_EXT_SCAN_PHY_2M |
			   BT_HCI_LE_EXT_SCAN_PHY_CODED)) {
			return BT_HCI_ERR_INVALID_PARAM;
		}

		if (packing > 1) {
			return BT_HCI_ERR_INVALID_PARAM;
		}

		if (framing > 1) {
			return BT_HCI_ERR_INVALID_PARAM;
		}

		if (encryption > 1) {
			return BT_HCI_ERR_INVALID_PARAM;
		}
	}

	link_cmplt = ll_rx_link_alloc();
	if (!link_cmplt) {
		return BT_HCI_ERR_MEM_CAPACITY_EXCEEDED;
	}

	link_term = ll_rx_link_alloc();
	if (!link_term) {
		ll_rx_link_release(link_cmplt);

		return BT_HCI_ERR_MEM_CAPACITY_EXCEEDED;
	}

	/* Store parameters in LLL context */
	/* TODO: parameters to ULL if only accessed by ULL */
	lll_adv_iso = &adv_iso->lll;
	lll_adv_iso->handle = big_handle;
	/* Mandatory Num_BIS = 1 */
	lll_adv_iso->num_bis = num_bis;
	/* TODO: Calculate NSE (No. of Sub Events), Mandatory NSE = 1 */
	lll_adv_iso->nse = 1;
	/* TODO: Calculate BN (Burst Count), Mandatory BN = 1 */
	lll_adv_iso->bn = 1;
	/* TODO: Immediate Repetation Count (IRC), Mandatory IRC = 1 */
	lll_adv_iso->irc = 1;
	/* TODO: Group count, GC = NSE / BN; PTO = GC - IRC;
	 */
	lll_adv_iso->pto = 0;
	lll_adv_iso->max_pdu = CONFIG_BT_CTLR_ADV_ISO_PDU_LEN_MAX;
	/* TODO: Calculate sub_interval, if interleaved then it is Num_BIS x
	 *       BIS_Spacing (by BT Spec.)
	 *       else if sequential, then by our implementation, lets keep it
	 *       max_tx_time for Max_PDU + tMSS.
	 */
	/* TODO: BIS_Spacing, if interleaved, then by our implementation, lets
	 *       keep it max_rx_time for Max_PDU + tMSS.
	 */
	/* TODO: packing support, sequential or interleaved */
	if (encryption) {
		lll_adv_iso->sub_interval = PKT_BIS_US(lll_adv_iso->max_pdu,
						       PDU_MIC_SIZE, phy) +
					    EVENT_MSS_US;
		lll_adv_iso->bis_spacing = PKT_BIS_US(lll_adv_iso->max_pdu,
						      PDU_MIC_SIZE, phy) +
					   EVENT_MSS_US;
	} else {
		lll_adv_iso->sub_interval = PKT_BIS_US(lll_adv_iso->max_pdu, 0,
						       phy) +
					    EVENT_MSS_US;
		lll_adv_iso->bis_spacing = PKT_BIS_US(lll_adv_iso->max_pdu, 0,
						      phy) +
					   EVENT_MSS_US;
	}

	util_saa_le32(lll_adv_iso->seed_access_addr, big_handle);

	lll_adv_iso->sdu_interval = sdu_interval;
	lll_adv_iso->max_sdu = max_sdu;
	lll_csrand_get(lll_adv_iso->base_crc_init,
		       sizeof(lll_adv_iso->base_crc_init));
	lll_adv_iso->data_chan_count =
		ull_chan_map_get(lll_adv_iso->data_chan_map);
	lll_adv_iso->phy = phy;
	lll_adv_iso->latency_prepare = 0;
	lll_adv_iso->latency_event = 0;

	/* TODO: framing support */
	lll_adv_iso->framing = framing;

	/* TODO: Calculate ISO interval */
	iso_interval = sdu_interval / 1250U;

	/* Allocate next PDU */
	err = ull_adv_sync_pdu_alloc(adv, ULL_ADV_PDU_EXTRA_DATA_ALLOC_IF_EXIST, &pdu_prev, &pdu,
				     NULL, NULL, &ter_idx);
	if (err) {
		return err;
	}

	/* Add ACAD to AUX_SYNC_IND */
	if (encryption) {
		pdu_big_info_size = PDU_BIG_INFO_ENCRYPTED_SIZE;
	} else {
		pdu_big_info_size = PDU_BIG_INFO_CLEARTEXT_SIZE;
	}
	hdr_data[0] = pdu_big_info_size + 2;
	err = ull_adv_sync_pdu_set_clear(lll_adv_sync, pdu_prev, pdu,
					 ULL_ADV_PDU_HDR_FIELD_ACAD, 0U,
					 &hdr_data);
	if (err) {
		ll_rx_link_release(link_cmplt);
		ll_rx_link_release(link_term);

		return err;
	}

	memcpy(&acad, &hdr_data[1], sizeof(acad));
	acad[0] = pdu_big_info_size + 1;
	acad[1] = BT_DATA_BIG_INFO;
	big_info = (void *)&acad[2];

	/* big_info->offset and big_info->offset_units will be filled by LLL
	 */

	big_info->iso_interval = iso_interval;
	big_info->num_bis = lll_adv_iso->num_bis;
	big_info->nse = lll_adv_iso->nse;
	big_info->bn = lll_adv_iso->bn;
	big_info->sub_interval = lll_adv_iso->sub_interval;
	big_info->pto = lll_adv_iso->pto;
	big_info->spacing = lll_adv_iso->bis_spacing;
	big_info->irc = lll_adv_iso->irc;
	big_info->max_pdu = lll_adv_iso->max_pdu;
	memcpy(&big_info->seed_access_addr, lll_adv_iso->seed_access_addr,
	       sizeof(big_info->seed_access_addr));
	big_info->sdu_interval = sdu_interval;
	big_info->max_sdu = max_sdu;
	memcpy(&big_info->base_crc_init, lll_adv_iso->base_crc_init,
	       sizeof(big_info->base_crc_init));
	memcpy(big_info->chm_phy, lll_adv_iso->data_chan_map,
	       sizeof(big_info->chm_phy));
	big_info->chm_phy[4] &= 0x1F;
	big_info->chm_phy[4] |= ((find_lsb_set(phy) - 1) << 5);
	big_info->payload_count_framing[0] = lll_adv_iso->payload_count;
	big_info->payload_count_framing[1] = lll_adv_iso->payload_count >> 8;
	big_info->payload_count_framing[2] = lll_adv_iso->payload_count >> 16;
	big_info->payload_count_framing[3] = lll_adv_iso->payload_count >> 24;
	big_info->payload_count_framing[4] = lll_adv_iso->payload_count >> 32;
	big_info->payload_count_framing[4] &= 0x7F;
	big_info->payload_count_framing[4] |= ((framing & 0x01) << 7);

	/* Associate the ISO instance with an Extended Advertising instance */
	lll_adv_iso->adv = &adv->lll;

	/* Store the link buffer for ISO create and terminate complete event */
	adv_iso->node_rx_complete.hdr.link = link_cmplt;
	adv_iso->node_rx_terminate.hdr.link = link_term;

	/* Initialise LLL header members */
	lll_hdr_init(lll_adv_iso, adv_iso);

	/* Start sending BIS empty data packet for each BIS */
	ticks_anchor_iso = ticker_ticks_now_get();
	ret = ull_adv_iso_start(adv_iso, ticks_anchor_iso);
	if (ret) {
		/* FIXME: release resources */
		return BT_HCI_ERR_CMD_DISALLOWED;
	}

	/* Associate the ISO instance with a Periodic Advertising */
	lll_adv_sync->iso = lll_adv_iso;

	/* Commit the BIGInfo in the ACAD field of Periodic Advertising */
	lll_adv_sync_data_enqueue(lll_adv_sync, ter_idx);

	return BT_HCI_ERR_SUCCESS;
}

uint8_t ll_big_test_create(uint8_t big_handle, uint8_t adv_handle,
			   uint8_t num_bis, uint32_t sdu_interval,
			   uint16_t iso_interval, uint8_t nse, uint16_t max_sdu,
			   uint16_t max_pdu, uint8_t phy, uint8_t packing,
			   uint8_t framing, uint8_t bn, uint8_t irc,
			   uint8_t pto, uint8_t encryption, uint8_t *bcode)
{
	/* TODO: Implement */
	ARG_UNUSED(big_handle);
	ARG_UNUSED(adv_handle);
	ARG_UNUSED(num_bis);
	ARG_UNUSED(sdu_interval);
	ARG_UNUSED(iso_interval);
	ARG_UNUSED(nse);
	ARG_UNUSED(max_sdu);
	ARG_UNUSED(max_pdu);
	ARG_UNUSED(phy);
	ARG_UNUSED(packing);
	ARG_UNUSED(framing);
	ARG_UNUSED(bn);
	ARG_UNUSED(irc);
	ARG_UNUSED(pto);
	ARG_UNUSED(encryption);
	ARG_UNUSED(bcode);

	return BT_HCI_ERR_CMD_DISALLOWED;
}

uint8_t ll_big_terminate(uint8_t big_handle, uint8_t reason)
{
	struct lll_adv_sync *lll_adv_sync;
	struct lll_adv_iso *lll_adv_iso;
	struct ll_adv_iso_set *adv_iso;
	struct pdu_adv *pdu_prev, *pdu;
	struct node_rx_pdu *node_rx;
	struct lll_adv *lll_adv;
	struct ll_adv_set *adv;
	uint8_t ter_idx;
	uint32_t ret;
	uint8_t err;

	adv_iso = ull_adv_iso_get(big_handle);
	if (!adv_iso) {
		return BT_HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
	}

	lll_adv_iso = &adv_iso->lll;
	lll_adv = lll_adv_iso->adv;
	if (!lll_adv) {
		return BT_HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
	}

	lll_adv_sync = lll_adv->sync;
	adv = HDR_LLL2ULL(lll_adv);

	/* Allocate next PDU */
	err = ull_adv_sync_pdu_alloc(adv, ULL_ADV_PDU_EXTRA_DATA_ALLOC_IF_EXIST, &pdu_prev, &pdu,
				     NULL, NULL, &ter_idx);
	if (err) {
		return err;
	}

	/* Remove ACAD to AUX_SYNC_IND */
	err = ull_adv_sync_pdu_set_clear(lll_adv_sync, pdu_prev, pdu,
					 0U, ULL_ADV_PDU_HDR_FIELD_ACAD, NULL);
	if (err) {
		return err;
	}

	lll_adv_sync_data_enqueue(lll_adv_sync, ter_idx);

	/* TODO: Terminate all BIS data paths */

	/* FIXME: Generate after terminate procedure completes */
	/* Prepare BIG terminate event */
	node_rx = (void *)&adv_iso->node_rx_terminate;
	node_rx->hdr.type = NODE_RX_TYPE_BIG_TERMINATE;
	node_rx->hdr.handle = big_handle;
	node_rx->hdr.rx_ftr.param = adv_iso;
	*((uint8_t *)node_rx->pdu) = reason;

	ret = ticker_stop(TICKER_INSTANCE_ID_CTLR, TICKER_USER_ID_THREAD,
			  (TICKER_ID_ADV_ISO_BASE + lll_adv_iso->handle),
			  ticker_op_stop_cb, adv_iso);

	lll_adv_iso->adv = NULL;
	lll_adv_sync->iso = NULL;

	return BT_HCI_ERR_SUCCESS;
}

int ull_adv_iso_init(void)
{
	int err;

	err = init_reset();
	if (err) {
		return err;
	}

	return 0;
}

int ull_adv_iso_reset(void)
{
	int err;

	err = init_reset();
	if (err) {
		return err;
	}

	return 0;
}

#if defined(CONFIG_BT_CTLR_HCI_ADV_HANDLE_MAPPING)
uint8_t ll_adv_iso_by_hci_handle_get(uint8_t hci_handle, uint8_t *handle)
{
	struct ll_adv_iso_set *adv_iso;
	uint8_t idx;

	adv_iso =  &ll_adv_iso[0];

	for (idx = 0U; idx < BT_CTLR_ADV_SET; idx++, adv_iso++) {
		if (adv_iso->lll.adv &&
		    (adv_iso->hci_handle == hci_handle)) {
			*handle = idx;
			return 0;
		}
	}

	return BT_HCI_ERR_UNKNOWN_ADV_IDENTIFIER;
}

uint8_t ll_adv_iso_by_hci_handle_new(uint8_t hci_handle, uint8_t *handle)
{
	struct ll_adv_iso_set *adv_iso, *adv_iso_empty;
	uint8_t idx;

	adv_iso = &ll_adv_iso[0];
	adv_iso_empty = NULL;

	for (idx = 0U; idx < BT_CTLR_ADV_SET; idx++, adv_iso++) {
		if (adv_iso->lll.adv) {
			if (adv_iso->hci_handle == hci_handle) {
				return BT_HCI_ERR_CMD_DISALLOWED;
			}
		} else if (!adv_iso_empty) {
			adv_iso_empty = adv_iso;
			*handle = idx;
		}
	}

	if (adv_iso_empty) {
		memset(adv_iso_empty, 0, sizeof(*adv_iso_empty));
		adv_iso_empty->hci_handle = hci_handle;
		return 0;
	}

	return BT_HCI_ERR_MEM_CAPACITY_EXCEEDED;
}
#endif /* CONFIG_BT_CTLR_HCI_ADV_HANDLE_MAPPING */

void ull_adv_iso_offset_get(struct ll_adv_sync_set *sync)
{
	static memq_link_t link;
	static struct mayfly mfy = {0, 0, &link, NULL, mfy_iso_offset_get};
	uint32_t ret;

	mfy.param = sync;
	ret = mayfly_enqueue(TICKER_USER_ID_ULL_HIGH, TICKER_USER_ID_ULL_LOW, 1,
			     &mfy);
	LL_ASSERT(!ret);
}

void ull_adv_iso_done(struct node_rx_event_done *done)
{
	struct ll_adv_iso_set *adv_iso;
	struct lll_adv_iso *lll;
	struct node_rx_hdr *rx;
	memq_link_t *link;

	/* switch to normal prepare */
	mfy_lll_prepare.fp = lll_adv_iso_prepare;

	/* Get reference to ULL context */
	adv_iso = CONTAINER_OF(done->param, struct ll_adv_iso_set, ull);
	lll = &adv_iso->lll;

	/* Prepare BIG complete event */
	rx = (void *)&adv_iso->node_rx_complete;
	link = rx->link;
	LL_ASSERT(link);
	rx->link = NULL;
	rx->type = NODE_RX_TYPE_BIG_COMPLETE;
	rx->handle = lll->handle;
	rx->rx_ftr.param = adv_iso;

	ll_rx_put(link, rx);
	ll_rx_sched();
}

static int init_reset(void)
{
	/* Initialize pool. */
	mem_init(ll_adv_iso, sizeof(struct ll_adv_iso_set),
		 sizeof(ll_adv_iso) / sizeof(struct ll_adv_iso_set),
		 &adv_iso_free);

	return 0;
}

static inline struct ll_adv_iso_set *ull_adv_iso_get(uint8_t handle)
{
	if (handle >= CONFIG_BT_CTLR_ADV_SET) {
		return NULL;
	}

	return &ll_adv_iso[handle];
}

static uint32_t ull_adv_iso_start(struct ll_adv_iso_set *adv_iso,
				  uint32_t ticks_anchor)
{
	uint32_t ticks_slot_overhead;
	uint32_t volatile ret_cb;
	uint32_t iso_interval_us;
	uint32_t slot_us;
	uint32_t ret;

	ull_hdr_init(&adv_iso->ull);

	/* TODO: Calc slot_us */
	slot_us = EVENT_OVERHEAD_START_US + EVENT_OVERHEAD_END_US;
	slot_us += 1000;

	adv_iso->ull.ticks_active_to_start = 0;
	adv_iso->ull.ticks_prepare_to_start =
		HAL_TICKER_US_TO_TICKS(EVENT_OVERHEAD_XTAL_US);
	adv_iso->ull.ticks_preempt_to_start =
		HAL_TICKER_US_TO_TICKS(EVENT_OVERHEAD_PREEMPT_MIN_US);
	adv_iso->ull.ticks_slot = HAL_TICKER_US_TO_TICKS(slot_us);

	if (IS_ENABLED(CONFIG_BT_CTLR_LOW_LAT)) {
		ticks_slot_overhead = MAX(adv_iso->ull.ticks_active_to_start,
					  adv_iso->ull.ticks_prepare_to_start);
	} else {
		ticks_slot_overhead = 0;
	}

	/* setup to use ISO create prepare function for first radio event */
	mfy_lll_prepare.fp = lll_adv_iso_create_prepare;

	/* TODO: Calculate ISO interval */
	/* iso_interval shall be at least SDU interval,
	 * or integer multiple of SDU interval for unframed PDUs
	 */
	iso_interval_us = adv_iso->lll.sdu_interval;

	ret_cb = TICKER_STATUS_BUSY;
	ret = ticker_start(TICKER_INSTANCE_ID_CTLR, TICKER_USER_ID_THREAD,
			   (TICKER_ID_ADV_ISO_BASE + adv_iso->lll.handle),
			   ticks_anchor, 0,
			   HAL_TICKER_US_TO_TICKS(iso_interval_us),
			   HAL_TICKER_REMAINDER(iso_interval_us),
			   TICKER_NULL_LAZY,
			   (adv_iso->ull.ticks_slot + ticks_slot_overhead),
			   ticker_cb, adv_iso,
			   ull_ticker_status_give, (void *)&ret_cb);
	ret = ull_ticker_status_take(ret, &ret_cb);

	return ret;
}

static void mfy_iso_offset_get(void *param)
{
	struct ll_adv_sync_set *sync;
	struct lll_adv_iso *lll_iso;
	uint32_t ticks_to_expire;
	struct pdu_big_info *bi;
	uint32_t ticks_current;
	struct pdu_adv *pdu;
	uint8_t ticker_id;
	uint8_t retry;
	uint8_t id;

	sync = param;
	lll_iso = sync->lll.iso;
	ticker_id = TICKER_ID_ADV_ISO_BASE + lll_iso->handle;

	id = TICKER_NULL;
	ticks_to_expire = 0U;
	ticks_current = 0U;
	retry = 4U;
	do {
		uint32_t volatile ret_cb;
		uint32_t ticks_previous;
		uint32_t ret;

		ticks_previous = ticks_current;

		ret_cb = TICKER_STATUS_BUSY;
		ret = ticker_next_slot_get(TICKER_INSTANCE_ID_CTLR,
					   TICKER_USER_ID_ULL_LOW,
					   &id,
					   &ticks_current, &ticks_to_expire,
					   ticker_op_cb, (void *)&ret_cb);
		if (ret == TICKER_STATUS_BUSY) {
			while (ret_cb == TICKER_STATUS_BUSY) {
				ticker_job_sched(TICKER_INSTANCE_ID_CTLR,
						 TICKER_USER_ID_ULL_LOW);
			}
		}

		LL_ASSERT(ret_cb == TICKER_STATUS_SUCCESS);

		LL_ASSERT((ticks_current == ticks_previous) || retry--);

		LL_ASSERT(id != TICKER_NULL);
	} while (id != ticker_id);

	pdu = lll_adv_sync_data_latest_peek(&sync->lll);
	bi = big_info_get(pdu);
	big_info_offset_fill(bi, ticks_to_expire, 0);
	bi->payload_count_framing[0] = lll_iso->payload_count;
	bi->payload_count_framing[1] = lll_iso->payload_count >> 8;
	bi->payload_count_framing[2] = lll_iso->payload_count >> 16;
	bi->payload_count_framing[3] = lll_iso->payload_count >> 24;
	bi->payload_count_framing[4] = lll_iso->payload_count >> 32;
	bi->payload_count_framing[4] &= 0x7F;
	bi->payload_count_framing[4] |= ((lll_iso->framing & 0x01) << 7);
}

static inline struct pdu_big_info *big_info_get(struct pdu_adv *pdu)
{
	struct pdu_adv_com_ext_adv *p;
	struct pdu_adv_ext_hdr *h;
	uint8_t *ptr;

	p = (void *)&pdu->adv_ext_ind;
	h = (void *)p->ext_hdr_adv_data;
	ptr = h->data;

	/* FIXME: CTEInfo */

	if (h->aux_ptr) {
		ptr += sizeof(struct pdu_adv_aux_ptr);
	}

	if (h->tx_pwr) {
		ptr++;
	}

	/* Length encoded AD Format */
	ptr += 2;

	return (void *)ptr;
}

static inline void big_info_offset_fill(struct pdu_big_info *bi,
					uint32_t ticks_offset,
					uint32_t start_us)
{
	uint32_t offs;

	offs = HAL_TICKER_TICKS_TO_US(ticks_offset) - start_us;
	offs = offs / OFFS_UNIT_30_US;
	if (!!(offs >> 13)) {
		bi->offs = offs / (OFFS_UNIT_300_US / OFFS_UNIT_30_US);
		bi->offs_units = 1U;
	} else {
		bi->offs = offs;
		bi->offs_units = 0U;
	}
}

static void ticker_cb(uint32_t ticks_at_expire, uint32_t remainder,
		      uint16_t lazy, uint8_t force, void *param)
{
	static struct lll_prepare_param p;
	struct ll_adv_iso_set *adv_iso = param;
	uint32_t ret;
	uint8_t ref;

	DEBUG_RADIO_PREPARE_A(1);

	/* Increment prepare reference count */
	ref = ull_ref_inc(&adv_iso->ull);
	LL_ASSERT(ref);

	/* Append timing parameters */
	p.ticks_at_expire = ticks_at_expire;
	p.remainder = remainder;
	p.lazy = lazy;
	p.force = force;
	p.param = &adv_iso->lll;
	mfy_lll_prepare.param = &p;

	/* Kick LLL prepare */
	ret = mayfly_enqueue(TICKER_USER_ID_ULL_HIGH, TICKER_USER_ID_LLL, 0,
			     &mfy_lll_prepare);
	LL_ASSERT(!ret);

	DEBUG_RADIO_PREPARE_A(1);
}

static void ticker_op_cb(uint32_t status, void *param)
{
	*((uint32_t volatile *)param) = status;
}

static void tx_lll_flush(void *param)
{
	struct ll_adv_iso_set *adv_iso = param;
	struct node_rx_pdu *rx;
	memq_link_t *link;

	/* TODO: Flush TX */

	/* Get the terminate structure reserved in the ISO context.
	 * The terminate reason and connection handle should already be
	 * populated before this mayfly function was scheduled.
	 */
	rx = (void *)&adv_iso->node_rx_terminate;
	link = rx->hdr.link;
	LL_ASSERT(link);
	rx->hdr.link = NULL;

	/* Enqueue the terminate towards ULL context */
	ull_rx_put(link, rx);
	ull_rx_sched();
}

static void ticker_op_stop_cb(uint32_t status, void *param)
{
	uint32_t retval;
	static memq_link_t link;
	static struct mayfly mfy = {0, 0, &link, NULL, tx_lll_flush};

	LL_ASSERT(status == TICKER_STATUS_SUCCESS);

	mfy.param = param;

	/* Flush pending tx PDUs in LLL (using a mayfly) */
	retval = mayfly_enqueue(TICKER_USER_ID_ULL_LOW, TICKER_USER_ID_LLL, 0,
				&mfy);
	LL_ASSERT(!retval);
}
