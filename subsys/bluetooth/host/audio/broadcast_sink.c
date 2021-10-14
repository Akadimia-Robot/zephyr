/*  Bluetooth Audio Broadcast Sink */

/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <sys/byteorder.h>
#include <sys/check.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/conn.h>
#include <bluetooth/gatt.h>
#include <bluetooth/audio.h>

#include "../conn_internal.h"
#include "../iso_internal.h"

#include "endpoint.h"
#include "capabilities.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_AUDIO_DEBUG_BROADCAST_SINK)
#define LOG_MODULE_NAME bt_audio_broadcast_sink
#include "common/log.h"

#define PA_SYNC_SKIP              5
#define SYNC_RETRY_COUNT          6 /* similar to retries for connections */
#define BASE_MIN_SIZE             17
#define BASE_BIS_DATA_MIN_SIZE    2 /* index and length */
#define BROADCAST_SYNC_MIN_INDEX  (BIT(1))

static struct bt_audio_broadcast_sink broadcast_sinks[BROADCAST_SNK_CNT];
static struct bt_le_scan_cb broadcast_scan_cb;

static void broadcast_sink_cleanup(struct bt_audio_broadcast_sink *sink);

static struct bt_audio_broadcast_sink *broadcast_sink_syncing_get(void)
{
	for (int i = 0; i < ARRAY_SIZE(broadcast_sinks); i++) {
		if (broadcast_sinks[i].syncing) {
			return &broadcast_sinks[i];
		}
	}

	return NULL;
}

static struct bt_audio_broadcast_sink *broadcast_sink_free_get(void)
{
	/* Find free entry */
	for (int i = 0; i < ARRAY_SIZE(broadcast_sinks); i++) {
		if (broadcast_sinks[i].pa_sync == NULL) {
			broadcast_sinks[i].index = i;
			return &broadcast_sinks[i];
		}
	}

	return NULL;
}

static struct bt_audio_broadcast_sink *broadcast_sink_get_by_pa(struct bt_le_per_adv_sync *sync)
{
	for (int i = 0; i < ARRAY_SIZE(broadcast_sinks); i++) {
		if (broadcast_sinks[i].pa_sync == sync) {
			return &broadcast_sinks[i];
		}
	}

	return NULL;
}

static void pa_synced(struct bt_le_per_adv_sync *sync,
		      struct bt_le_per_adv_sync_synced_info *info)
{
	struct bt_audio_broadcast_sink *sink;
	struct bt_audio_capability *cap;
	sys_slist_t *lst;

	sink = broadcast_sink_syncing_get();
	if (sink == NULL || sync != sink->pa_sync) {
		/* Not ours */
		return;
	}

	BT_DBG("Synced to broadcast source with ID 0x%06X", sink->broadcast_id);

	sink->syncing = false;

	bt_audio_broadcast_sink_scan_stop();

	lst = bt_audio_capability_get(BT_AUDIO_SINK);
	if (lst == NULL) {
		/* Terminate early if we do not have any audio sink
		 * capabilities
		 */
		return;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(lst, cap, node) {
		struct bt_audio_capability_ops *ops;

		ops = cap->ops;

		if (ops != NULL && ops->pa_synced != NULL) {
			ops->pa_synced(sink, sink->pa_sync, sink->broadcast_id);
		}
	}

	/* TODO: Wait for an parse PA data and use the capability ops to
	 * get audio channels from the upper layer
	 * TBD: What if sync to a bad broadcast source that does not send
	 * properly formatted (or any) BASE?
	 */
}

static void pa_term(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_term_info *info)
{
	struct bt_audio_broadcast_sink *sink;
	struct bt_audio_capability *cap;
	sys_slist_t *lst;

	sink = broadcast_sink_get_by_pa(sync);
	if (sink == NULL) {
		/* Not ours */
		return;
	}

	BT_DBG("PA sync with broadcast source with ID 0x%06X lost",
	       sink->broadcast_id);
	broadcast_sink_cleanup(sink);

	lst = bt_audio_capability_get(BT_AUDIO_SINK);
	if (lst == NULL) {
		/* Terminate early if we do not have any audio sink
		 * capabilities
		 */
		return;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(lst, cap, node) {
		struct bt_audio_capability_ops *ops;

		ops = cap->ops;

		if (ops != NULL && ops->pa_sync_lost != NULL) {
			ops->pa_sync_lost(sink);
		}
	}
}

static bool net_buf_decode_codec_ltv(struct net_buf_simple *buf,
				     struct bt_codec_data *codec_data)
{
	size_t value_len;
	void *value;

	if (buf->len < sizeof(codec_data->data.data_len)) {
		BT_DBG("Not enough data for LTV length field: %u", buf->len);
		return false;
	}
	codec_data->data.data_len = net_buf_simple_pull_u8(buf);

	if (buf->len < sizeof(codec_data->data.type)) {
		BT_DBG("Not enough data for LTV type field: %u", buf->len);
		return false;
	}
	codec_data->data.type = net_buf_simple_pull_u8(buf);
	codec_data->data.data = codec_data->value;

	value_len = codec_data->data.data_len - sizeof(codec_data->data.type);
	if (buf->len < value_len) {
		BT_DBG("Not enough data for LTV value field: %u/%zu",
		       buf->len, value_len);
		return false;
	}
	value = net_buf_simple_pull_mem(buf, value_len);
	memcpy(codec_data->value, value, value_len);

	return true;
}

static bool net_buf_decode_bis_data(struct net_buf_simple *buf,
				    struct bt_audio_base_bis_data *bis,
				    bool codec_data_already_found)
{
	uint8_t len;

	if (buf->len < BASE_BIS_DATA_MIN_SIZE) {
		BT_DBG("Not enough bytes (%u) to decode BIS data", buf->len);
		return false;
	}

	bis->index = net_buf_simple_pull_u8(buf);
	if (bis->index == 0) {
		BT_DBG("BIS index was 0");
		return false;
	}

	/* codec config data length */
	len = net_buf_simple_pull_u8(buf);
	if (len > buf->len) {
		BT_DBG("Invalid BIS specific codec config data length: "
		       "%u (buf is %u)", len, buf->len);
		return false;
	}

	if (len > 0) {
		struct net_buf_simple ltv_buf;
		void *ltv_data;

		if (codec_data_already_found) {
			/* Codec config can either be specific to each
			 *  BIS or for all, but not both
			 */
			BT_DBG("BASE contains both codec config data and BIS "
			       "codec config data. Aborting.");
			return false;
		}

		/* TODO: Support codec configuration data per bis */
		BT_WARN("BIS specific codec config data of length %u "
			"was found but is not supported yet", len);

		/* Use an extra net_buf_simple to be able to decode until it
		 * is empty (len = 0)
		 */
		ltv_data = net_buf_simple_pull_mem(buf, len);
		net_buf_simple_init_with_data(&ltv_buf, ltv_data, len);

		while (ltv_buf.len != 0) {
			struct bt_codec_data *bis_codec_data;

			bis_codec_data = &bis->data[bis->data_count];

			if (!net_buf_decode_codec_ltv(&ltv_buf,
						      bis_codec_data)) {
				BT_DBG("Failed to decode BIS config data for entry %u",
				       bis->data_count);
				return false;
			}
			bis->data_count++;
		}
	}

	return true;
}

static bool net_buf_decode_subgroup(struct net_buf_simple *buf,
				    struct bt_audio_base_subgroup *subgroup)
{
	struct net_buf_simple ltv_buf;
	struct bt_codec	*codec;
	void *ltv_data;
	uint8_t len;

	codec = &subgroup->codec;

	subgroup->bis_count = net_buf_simple_pull_u8(buf);
	if (subgroup->bis_count > ARRAY_SIZE(subgroup->bis_data)) {
		BT_DBG("BASE has more BIS %u than we support %u",
		       subgroup->bis_count,
		       (uint8_t)ARRAY_SIZE(subgroup->bis_data));
		return false;
	}
	codec->id = net_buf_simple_pull_u8(buf);
	codec->cid = net_buf_simple_pull_le16(buf);
	codec->vid = net_buf_simple_pull_le16(buf);

	/* codec configuration data length */
	len = net_buf_simple_pull_u8(buf);
	if (len > buf->len) {
		BT_DBG("Invalid codec config data length: %u (buf is %u)",
		len, buf->len);
		return false;
	}

	/* Use an extra net_buf_simple to be able to decode until it
	 * is empty (len = 0)
	 */
	ltv_data = net_buf_simple_pull_mem(buf, len);
	net_buf_simple_init_with_data(&ltv_buf, ltv_data, len);

	/* The loop below is very similar to codec_config_store with notable
	 * exceptions that it can do early termination, and also does not log
	 * every LTV entry, which would simply be too much for handling
	 * broadcasted BASEs
	 */
	while (ltv_buf.len != 0) {
		struct bt_codec_data *codec_data = &codec->data[codec->data_count++];

		if (!net_buf_decode_codec_ltv(&ltv_buf, codec_data)) {
			BT_DBG("Failed to decode codec config data for entry %u",
			       codec->data_count - 1);
			return false;
		}
	}

	if (sizeof(len) > buf->len) {
		return false;
	}

	/* codec metadata length */
	len = net_buf_simple_pull_u8(buf);
	if (len > buf->len) {
		BT_DBG("Invalid codec config data length: %u (buf is %u)",
		len, buf->len);
		return false;
	}


	/* Use an extra net_buf_simple to be able to decode until it
	 * is empty (len = 0)
	 */
	ltv_data = net_buf_simple_pull_mem(buf, len);
	net_buf_simple_init_with_data(&ltv_buf, ltv_data, len);

	/* The loop below is very similar to codec_config_store with notable
	 * exceptions that it can do early termination, and also does not log
	 * every LTV entry, which would simply be too much for handling
	 * broadcasted BASEs
	 */
	while (ltv_buf.len != 0) {
		struct bt_codec_data *metadata = &codec->meta[codec->meta_count++];

		if (!net_buf_decode_codec_ltv(&ltv_buf, metadata)) {
			BT_DBG("Failed to decode codec metadata for entry %u",
			       codec->meta_count - 1);
			return false;
		}
	}

	for (int i = 0; i < subgroup->bis_count; i++) {
		if (!net_buf_decode_bis_data(buf, &subgroup->bis_data[i],
					     codec->data_count > 0)) {
			BT_DBG("Failed to decode BIS data for bis %d", i);
			return false;
		}
	}

	return true;
}

static bool pa_decode_base(struct bt_data *data, void *user_data)
{
	struct bt_audio_broadcast_sink *sink = (struct bt_audio_broadcast_sink *)user_data;
	struct bt_codec_qos codec_qos = { 0 };
	struct bt_audio_base base = { 0 };
	struct bt_uuid_16 broadcast_uuid;
	struct bt_audio_capability *cap;
	struct net_buf_simple net_buf;
	sys_slist_t *lst;
	void *uuid;

	lst = bt_audio_capability_get(BT_AUDIO_SINK);
	if (lst == NULL) {
		/* Terminate early if we do not have any audio sink
		 * capabilities
		 */
		return false;
	}

	if (data->type != BT_DATA_SVC_DATA16) {
		return true;
	}

	if (data->data_len < BASE_MIN_SIZE) {
		return true;
	}

	net_buf_simple_init_with_data(&net_buf, (void *)data->data,
				      data->data_len);

	uuid = net_buf_simple_pull_mem(&net_buf, BT_UUID_SIZE_16);

	if (!bt_uuid_create(&broadcast_uuid.uuid, uuid, BT_UUID_SIZE_16)) {
		BT_ERR("bt_uuid_create failed");
		return false;
	}

	if (bt_uuid_cmp(&broadcast_uuid.uuid, BT_UUID_BASIC_AUDIO) != 0) {
		/* Continue parsing */
		return true;
	}

	codec_qos.pd = net_buf_simple_pull_le24(&net_buf);
	sink->subgroup_count = net_buf_simple_pull_u8(&net_buf);
	base.subgroup_count = sink->subgroup_count;
	for (int i = 0; i < base.subgroup_count; i++) {
		if (!net_buf_decode_subgroup(&net_buf, &base.subgroups[i])) {
			BT_DBG("Failed to decode subgroup %d", i);
			return false;
		}
	}

	/* TODO: We can already at this point validate whether the BASE
	 * is useful for us based on our capabilities. E.g. if the codec
	 * or number of BIS doesn't match our capabilities, there's no
	 * reason to continue with the BASE. For now let the upper layer
	 * decide.
	 */

	if (sink->biginfo_received) {
		uint8_t num_bis = 0;

		for (int i = 0; i < base.subgroup_count; i++) {
			num_bis += base.subgroups[i].bis_count;
		}

		if (num_bis > sink->biginfo_num_bis) {
			BT_WARN("BASE contains more BIS than reported by BIGInfo");
			return false;
		}
	}

	SYS_SLIST_FOR_EACH_CONTAINER(lst, cap, node) {
		struct bt_audio_capability_ops *ops;

		ops = cap->ops;

		if (ops != NULL && ops->base_recv != NULL) {
			ops->base_recv(sink, &base);
		}
	}

	return false;
}

static void pa_recv(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_recv_info *info,
		    struct net_buf_simple *buf)
{
	struct bt_audio_broadcast_sink *sink = broadcast_sink_get_by_pa(sync);

	if (sink == NULL) {
		/* Not a PA sync that we control */
		return;
	}

	bt_data_parse(buf, pa_decode_base, (void *)sink);
}

static void biginfo_recv(struct bt_le_per_adv_sync *sync,
			 const struct bt_iso_biginfo *biginfo)
{
	struct bt_audio_broadcast_sink *sink;
	struct bt_audio_capability *cap;
	sys_slist_t *lst;

	sink = broadcast_sink_get_by_pa(sync);
	if (sink == NULL) {
		/* Not ours */
		return;
	}

	sink->biginfo_received = true;
	sink->iso_interval = biginfo->iso_interval;
	sink->biginfo_num_bis = biginfo->num_bis;
	sink->big_encrypted = biginfo->encryption;

	lst = bt_audio_capability_get(BT_AUDIO_SINK);
	if (lst == NULL) {
		/* Terminate early if we do not have any audio sink
		 * capabilities
		 */
		return;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(lst, cap, node) {
		struct bt_audio_capability_ops *ops;

		ops = cap->ops;

		if (ops != NULL && ops->syncable != NULL) {
			ops->syncable(sink, biginfo->encryption);
		}
	}
}

static uint16_t interval_to_sync_timeout(uint16_t interval)
{
	uint16_t timeout;

	/* Ensure that the following calculation does not overflow silently */
	__ASSERT(SYNC_RETRY_COUNT < 10, "SYNC_RETRY_COUNT shall be less than 10");

	/* Add retries and convert to unit in 10's of ms */
	timeout = ((uint32_t)interval * SYNC_RETRY_COUNT) / 10;

	/* Enforce restraints */
	timeout = CLAMP(timeout, BT_GAP_PER_ADV_MIN_TIMEOUT,
			BT_GAP_PER_ADV_MAX_TIMEOUT);

	return timeout;
}

static void sync_broadcast_pa(sys_slist_t *lst,
			      const struct bt_le_scan_recv_info *info,
			      uint32_t broadcast_id,
			      struct bt_audio_capability *cap)
{
	struct bt_le_per_adv_sync_param param;
	struct bt_audio_broadcast_sink *sink;
	static bool pa_cb_registered;
	int err;

	if (!pa_cb_registered) {
		static struct bt_le_per_adv_sync_cb cb = {
			.synced = pa_synced,
			.recv = pa_recv,
			.term = pa_term,
			.biginfo = biginfo_recv
		};

		bt_le_per_adv_sync_cb_register(&cb);
	}

	sink = broadcast_sink_free_get();
	/* Should never happen as we check for free entry before
	 * scanning
	 */
	__ASSERT(sink != NULL, "sink is NULL");

	bt_addr_le_copy(&param.addr, info->addr);
	param.options = 0;
	param.sid = info->sid;
	param.skip = PA_SYNC_SKIP;
	param.timeout = interval_to_sync_timeout(info->interval);
	err = bt_le_per_adv_sync_create(&param, &sink->pa_sync);
	if (err != 0) {
		BT_ERR("Could not sync to PA: %d", err);
		err = bt_le_scan_stop();
		if (err != 0 && err != -EALREADY) {
			BT_ERR("Could not stop scan: %d", err);
		}

		SYS_SLIST_FOR_EACH_CONTAINER(lst, cap, node) {
			struct bt_audio_capability_ops *ops;

			ops = cap->ops;

			if (ops != NULL && ops->scan_term != NULL) {
				ops->scan_term(err);
			}
		}
	} else {
		sink->syncing = true;
		sink->pa_interval = info->interval;
		sink->broadcast_id = broadcast_id;
		sink->cap = cap;
	}
}

static bool scan_check_and_sync_broadcast(struct bt_data *data, void *user_data)
{
	const struct bt_le_scan_recv_info *info = user_data;
	struct bt_uuid_16 adv_uuid;
	uint32_t broadcast_id;
	sys_slist_t *lst;
	struct bt_audio_capability *cap;

	lst = bt_audio_capability_get(BT_AUDIO_SINK);
	if (lst == NULL) {
		/* Terminate early if we do not have any audio sink
		 * capabilities
		 */
		return false;
	}

	if (data->type != BT_DATA_SVC_DATA16) {
		return true;
	}

	if (data->data_len < BT_UUID_SIZE_16 + BT_BROADCAST_ID_SIZE) {
		return true;
	}

	if (!bt_uuid_create(&adv_uuid.uuid, data->data, BT_UUID_SIZE_16)) {
		return true;
	}

	if (bt_uuid_cmp(&adv_uuid.uuid, BT_UUID_BROADCAST_AUDIO)) {
		return true;
	}

	if (broadcast_sink_syncing_get() != NULL) {
		/* Already syncing, can maximum sync one */
		return true;
	}

	broadcast_id = sys_get_le24(data->data + BT_UUID_SIZE_16);

	BT_DBG("Found broadcast source with address %s and id 0x%6X",
	       bt_addr_le_str(info->addr), broadcast_id);

	SYS_SLIST_FOR_EACH_CONTAINER(lst, cap, node) {
		struct bt_audio_capability_ops *ops;

		ops = cap->ops;

		if (ops != NULL && ops->scan_recv != NULL) {
			bool sync_pa = ops->scan_recv(info, broadcast_id);

			if (sync_pa) {
				sync_broadcast_pa(lst, info, broadcast_id, cap);
				break;
			}
		}
	}

	/* Stop parsing */
	return false;
}

static void broadcast_scan_recv(const struct bt_le_scan_recv_info *info,
				struct net_buf_simple *ad)
{
	/* We are only interested in non-connectable periodic advertisers */
	if ((info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) ||
	     info->interval == 0) {
		return;
	}

	bt_data_parse(ad, scan_check_and_sync_broadcast, (void *)info);
}

static void broadcast_scan_timeout(void)
{
	struct bt_audio_capability *cap;
	sys_slist_t *lst;

	bt_le_scan_cb_unregister(&broadcast_scan_cb);

	lst = bt_audio_capability_get(BT_AUDIO_SINK);
	if (lst == NULL) {
		BT_WARN("No BT_AUDIO_SINK capabilities");
		return;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(lst, cap, node) {
		struct bt_audio_capability_ops *ops;

		ops = cap->ops;

		if (ops != NULL && ops->scan_term != NULL) {
			ops->scan_term(-ETIME);
		}
	}
}

int bt_audio_broadcast_sink_scan_start(const struct bt_le_scan_param *param)
{
	sys_slist_t *lst;
	int err;

	CHECKIF(param == NULL) {
		BT_DBG("param is NULL");
		return -EINVAL;
	}

	CHECKIF(param->timeout != 0) {
		/* This is to avoid having to re-implement the scan timeout
		 * callback as well, and can be modified later if requested
		 */
		BT_DBG("Scan param shall not have a timeout");
		return -EINVAL;
	}

	lst = bt_audio_capability_get(BT_AUDIO_SINK);
	if (lst == NULL) {
		BT_WARN("No BT_AUDIO_SINK capabilities");
		return -EINVAL;
	}

	if (broadcast_sink_free_get() == NULL) {
		BT_DBG("No more free broadcast sinks");
		return -ENOMEM;
	}

	/* TODO: check for scan callback */
	err = bt_le_scan_start(param, NULL);
	if (err == 0) {
		broadcast_scan_cb.recv = broadcast_scan_recv;
		broadcast_scan_cb.timeout = broadcast_scan_timeout;
		bt_le_scan_cb_register(&broadcast_scan_cb);
	}

	return err;
}

int bt_audio_broadcast_sink_scan_stop(void)
{
	struct bt_audio_broadcast_sink *sink;
	struct bt_audio_capability *cap;
	sys_slist_t *lst;
	int err;

	sink = broadcast_sink_syncing_get();
	if (sink != NULL) {
		err = bt_le_per_adv_sync_delete(sink->pa_sync);
		if (err != 0) {
			BT_DBG("Could not delete PA sync: %d", err);
			return err;
		}
		sink->pa_sync = NULL;
		sink->syncing = false;
	}

	err = bt_le_scan_stop();
	if (err == 0) {
		bt_le_scan_cb_unregister(&broadcast_scan_cb);
	}

	lst = bt_audio_capability_get(BT_AUDIO_SINK);
	if (lst == NULL) {
		BT_WARN("No BT_AUDIO_SINK capabilities");
		return 0;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(lst, cap, node) {
		struct bt_audio_capability_ops *ops;

		ops = cap->ops;

		if (ops != NULL && ops->scan_term != NULL) {
			ops->scan_term(0);
		}
	}

	return err;
}

static int bt_audio_broadcast_sink_setup_chan(uint8_t index,
					      struct bt_audio_chan *chan,
					      struct bt_codec *codec)
{
	static struct bt_iso_chan_io_qos sink_chan_io_qos;
	static struct bt_codec_qos codec_qos;
	struct bt_audio_ep *ep;
	int err;

	if (chan->group != NULL) {
		BT_DBG("Channel %p already in group %p", chan, chan->group);
		return -EALREADY;
	}

	ep = bt_audio_ep_broadcaster_new(index, BT_AUDIO_SINK);
	if (ep == NULL) {
		BT_DBG("Could not allocate new broadcast endpoint");
		return -ENOMEM;
	}

	bt_audio_chan_attach(NULL, chan, ep, NULL, codec);
	/* TODO: The values of sink_chan_io_qos and codec_qos are not used,
	 * but the `rx` and `qos` pointers need to be set. This should be fixed.
	 */
	chan->iso->qos->rx = &sink_chan_io_qos;
	chan->iso->qos->tx = NULL;
	codec_qos.dir = BT_CODEC_QOS_IN;
	chan->qos = &codec_qos;
	err = bt_audio_chan_codec_qos_to_iso_qos(chan->iso->qos, &codec_qos);
	if (err) {
		BT_ERR("Unable to convert codec QoS to ISO QoS");
		return err;
	}

	return 0;
}

static void broadcast_sink_cleanup_chans(struct bt_audio_broadcast_sink *sink)
{
	for (size_t i = 0; i < sink->chan_count; i++) {
		struct bt_audio_chan *chan;

		chan = &sink->chans[i];

		chan->ep->chan = NULL;
		chan->ep = NULL;
		chan->qos = NULL;
		chan->codec = NULL;
		chan->iso = NULL;
		chan->group = NULL;
	}
}

static void broadcast_sink_cleanup(struct bt_audio_broadcast_sink *sink)
{
	broadcast_sink_cleanup_chans(sink);
	(void)memset(sink, 0, sizeof(*sink));
}

int bt_audio_broadcast_sink_sync(struct bt_audio_broadcast_sink *sink,
				 uint32_t indexes_bitfield,
				 struct bt_audio_chan *chans,
				 const uint8_t broadcast_code[16])
{
	struct bt_iso_big_sync_param param;
	uint8_t chan_count;
	int err;

	CHECKIF(sink == NULL) {
		BT_DBG("sink is NULL");
		return -EINVAL;
	}

	CHECKIF(indexes_bitfield == 0) {
		BT_DBG("indexes_bitfield is 0");
		return -EINVAL;
	}

	CHECKIF(indexes_bitfield & BIT(0)) {
		BT_DBG("BIT(0) is not a valid BIS index");
		return -EINVAL;
	}

	CHECKIF(chans == NULL) {
		BT_DBG("chans is NULL");
		return -EINVAL;
	}

	if (sink->pa_sync == NULL) {
		BT_DBG("Sink is not PA synced");
		return -EINVAL;
	}

	if (!sink->biginfo_received) {
		/* TODO: We could store the request to sync and start the sync
		 * once the BIGInfo has been received, and then do the sync
		 * then. This would be similar how LE Create Connection works.
		 */
		BT_DBG("BIGInfo not received, cannot sync yet");
		return -EAGAIN;
	}

	CHECKIF(sink->big_encrypted && broadcast_code == NULL) {
		BT_DBG("Broadcast code required");
		return -EINVAL;
	}

	/* Validate that number of bits set is less than number of channels */
	chan_count = 0;
	for (int i = 1; i < BT_ISO_MAX_GROUP_ISO_COUNT; i++) {
		if ((indexes_bitfield & BIT(i)) != 0) {
			chan_count++;
		}
	}

	sink->chan_count = chan_count;
	sink->chans = chans;
	for (size_t i = 0; i < chan_count; i++) {
		struct bt_audio_chan *chan;

		chan = &chans[i];

		err = bt_audio_broadcast_sink_setup_chan(sink->index, chan,
							 sink->cap->codec);
		if (err != 0) {
			BT_DBG("Failed to setup chans[%zu]: %d", i, err);
			broadcast_sink_cleanup_chans(sink);
			return err;
		}

		sink->bis[i] = &chan->ep->iso;
	}

	param.bis_channels = sink->bis;
	param.num_bis = sink->chan_count;
	param.bis_bitfield = indexes_bitfield;
	param.mse = 0; /* Let controller decide */
	param.sync_timeout = interval_to_sync_timeout(sink->iso_interval);
	param.encryption = sink->big_encrypted; /* TODO */
	if (param.encryption) {
		memcpy(param.bcode, broadcast_code, sizeof(param.bcode));
	} else {
		memset(param.bcode, 0, sizeof(param.bcode));
	}

	err = bt_iso_big_sync(sink->pa_sync, &param, &sink->big);
	if (err != 0) {
		broadcast_sink_cleanup_chans(sink);
		return err;
	}

	for (size_t i = 0; i < chan_count; i++) {
		struct bt_audio_ep *ep = chans[i].ep;

		ep->broadcast_sink = sink;
		bt_audio_ep_set_state(ep, BT_AUDIO_EP_STATE_QOS_CONFIGURED);
	}

	return 0;
}

int bt_audio_broadcast_sink_stop(struct bt_audio_broadcast_sink *sink)
{
	struct bt_audio_chan *chan;
	int err;

	CHECKIF(sink == NULL) {
		BT_DBG("sink is NULL");
		return -EINVAL;
	}

	chan = &sink->chans[0];

	if (chan == NULL) {
		BT_DBG("chan is NULL");
		return -EINVAL;
	}

	if (chan->ep == NULL) {
		BT_DBG("chan->ep is NULL");
		return -EINVAL;
	}

	if (chan->ep->status.state != BT_AUDIO_EP_STATE_STREAMING &&
	    chan->ep->status.state != BT_AUDIO_EP_STATE_QOS_CONFIGURED) {
		BT_DBG("Broadcast sink chan %p invalid state: %u",
		       chan, chan->ep->status.state);
		return -EBADMSG;
	}

	err = bt_iso_big_terminate(sink->big);
	if (err) {
		BT_DBG("Failed to terminate BIG (err %d)", err);
		return err;
	}

	sink->big = NULL;
	sink->chan_count = 0;
	/* Channel states will be updated in the ep_iso_disconnected function */

	return 0;
}

int bt_audio_broadcast_sink_delete(struct bt_audio_broadcast_sink *sink)
{
	struct bt_audio_chan *chan;
	int err;

	CHECKIF(sink == NULL) {
		BT_DBG("sink is NULL");
		return -EINVAL;
	}

	chan = &sink->chans[0];

	if (chan != NULL && chan->ep != NULL) {
		BT_DBG("Sink is not stopped");
		return -EBADMSG;
	}

	if (sink->pa_sync == NULL) {
		BT_DBG("Broadcast sink is already deleted");
		return -EALREADY;
	}

	err = bt_le_per_adv_sync_delete(sink->pa_sync);
	if (err != 0) {
		BT_DBG("Failed to delete periodic advertising sync (err %d)",
		       err);
		return err;
	}

	/* Reset the broadcast sink */
	broadcast_sink_cleanup(sink);

	return 0;
}
