/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/sys/check.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/audio/audio.h>
#include <zephyr/bluetooth/audio/cap.h>
#include <zephyr/bluetooth/audio/tbs.h>
#include "cap_internal.h"
#include "csis_internal.h"
#include "endpoint.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_CAP_INITIATOR)
#define LOG_MODULE_NAME bt_cap_initiator
#include "common/log.h"

static const struct bt_cap_initiator_cb *cap_cb;

int bt_cap_initiator_register_cb(const struct bt_cap_initiator_cb *cb)
{
	CHECKIF(cb == NULL) {
		BT_DBG("cb is NULL");
		return -EINVAL;
	}

	CHECKIF(cap_cb != NULL) {
		BT_DBG("callbacks already registered");
		return -EALREADY;
	}

	cap_cb = cb;

	return 0;
}

static bool cap_initiator_valid_metadata(const struct bt_codec_data meta[],
					 size_t meta_count)
{
	bool stream_context_found;

	BT_DBG("meta %p count %zu", meta, meta_count);

	/* Streaming Audio Context shall be present in CAP */
	stream_context_found = false;
	for (size_t i = 0U; i < meta_count; i++) {
		const struct bt_data *metadata = &meta[i].data;

		BT_DBG("metadata %p type %u len %u data %s",
		       metadata, metadata->type, metadata->data_len,
		       bt_hex(metadata->data, metadata->data_len));

		if (metadata->type == BT_AUDIO_METADATA_TYPE_STREAM_CONTEXT) {
			if (metadata->data_len != 2) { /* Stream context size */
				return false;
			}

			stream_context_found = true;
			break;
		}
	}

	if (!stream_context_found) {
		BT_DBG("No streaming context supplied");
	}

	return stream_context_found;
}

#if defined(CONFIG_BT_AUDIO_BROADCAST_SOURCE)

static bool cap_initiator_broadcast_audio_start_valid_param(
	const struct bt_cap_initiator_broadcast_create_param *param)
{

	for (size_t i = 0U; i < param->subgroup_count; i++) {
		const struct bt_cap_initiator_broadcast_subgroup_param *subgroup_param;
		const struct bt_codec *codec;
		bool valid_metadata;

		subgroup_param = &param->subgroup_params[i];
		codec = subgroup_param->codec;

		/* Streaming Audio Context shall be present in CAP */

		CHECKIF(codec == NULL) {
			BT_DBG("subgroup[%zu]->codec is NULL");
			return false;
		}

		valid_metadata = cap_initiator_valid_metadata(codec->meta,
							      codec->meta_count);

		CHECKIF(!valid_metadata) {
			BT_DBG("Invalid metadata supplied for subgroup[%zu]", i);
			return false;
		}
	}

	return true;
}


int bt_cap_initiator_broadcast_audio_start(struct bt_cap_initiator_broadcast_create_param *param,
					   struct bt_le_ext_adv *adv,
					   struct bt_cap_broadcast_source **broadcast_source)
{
	/* TODO: For now the create param and broadcast sources are
	 * identical, so we can just cast them. This need to be updated and
	 * made resistant to changes in either the CAP or BAP APIs at some point
	 */
	struct bt_audio_broadcast_source_create_param *bap_create_param =
		(struct bt_audio_broadcast_source_create_param *)param;
	struct bt_audio_broadcast_source **bap_broadcast_source =
		(struct bt_audio_broadcast_source **)broadcast_source;
	int err;

	if (!cap_initiator_broadcast_audio_start_valid_param(param)) {
		return -EINVAL;
	}

	CHECKIF(broadcast_source == NULL) {
		BT_DBG("source is NULL");
		return -EINVAL;
	}

	err = bt_audio_broadcast_source_create(bap_create_param,
					       bap_broadcast_source);
	if (err != 0) {
		BT_DBG("Failed to create broadcast source: %d", err);
		return err;
	}

	err = bt_audio_broadcast_source_start(*bap_broadcast_source, adv);
	if (err != 0) {
		int del_err;

		BT_DBG("Failed to start broadcast source: %d\n", err);

		del_err = bt_audio_broadcast_source_delete(*bap_broadcast_source);
		if (del_err) {
			BT_ERR("Failed to delete BAP broadcast source: %d", del_err);
		}
	}

	return err;
}

int bt_cap_initiator_broadcast_audio_update(struct bt_cap_broadcast_source *broadcast_source,
					    const struct bt_codec_data meta[],
					    size_t meta_count)
{
	CHECKIF(meta == NULL) {
		BT_DBG("meta is NULL");
		return -EINVAL;
	}

	if (!cap_initiator_valid_metadata(meta, meta_count)) {
		BT_DBG("Invalid metadata");
		return -EINVAL;
	}

	return bt_audio_broadcast_source_update_metadata(
		(struct bt_audio_broadcast_source *)broadcast_source,
		meta, meta_count);
}

int bt_cap_initiator_broadcast_audio_stop(struct bt_cap_broadcast_source *broadcast_source)
{
	return bt_audio_broadcast_source_stop((struct bt_audio_broadcast_source *)broadcast_source);
}

int bt_cap_initiator_broadcast_audio_delete(struct bt_cap_broadcast_source *broadcast_source)
{
	return bt_audio_broadcast_source_delete(
		(struct bt_audio_broadcast_source *)broadcast_source);
}

int bt_cap_initiator_broadcast_get_id(const struct bt_cap_broadcast_source *source,
				      uint32_t *const broadcast_id)
{
	return bt_audio_broadcast_source_get_id(
		(struct bt_audio_broadcast_source *)source,
		broadcast_id);
}

int bt_cap_initiator_broadcast_get_base(struct bt_cap_broadcast_source *source,
					struct net_buf_simple *base_buf)
{
	return bt_audio_broadcast_source_get_base(
		(struct bt_audio_broadcast_source *)source,
		base_buf);
}

#endif /* CONFIG_BT_AUDIO_BROADCAST_SOURCE */

#if defined(CONFIG_BT_AUDIO_UNICAST_CLIENT)
enum {
	CAP_UNICAST_PROCEDURE_STATE_ACTIVE,
	CAP_UNICAST_PROCEDURE_STATE_ABORTED,

	CAP_UNICAST_PROCEDURE_STATE_FLAG_NUM,
} cap_unicast_procedure_state;

struct cap_unicast_procedure {
	ATOMIC_DEFINE(flags, CAP_UNICAST_PROCEDURE_STATE_FLAG_NUM);
	size_t stream_cnt;
	size_t stream_initiated;
	size_t stream_done;
	int err;
	struct bt_conn *failed_conn;
	struct bt_cap_stream *streams[CONFIG_BT_AUDIO_UNICAST_CLIENT_GROUP_STREAM_COUNT];
};

struct cap_unicast_client {
	struct bt_conn *conn;
	struct bt_gatt_discover_params param;
	uint16_t csis_start_handle;
	const struct bt_csis_client_csis_inst *csis_inst;
	bool cas_found;
};

static struct cap_unicast_client bt_cap_unicast_clients[CONFIG_BT_MAX_CONN];
static const struct bt_uuid *cas_uuid = BT_UUID_CAS;
static struct cap_unicast_procedure active_procedure;

static void cap_initiator_disconnected(struct bt_conn *conn, uint8_t reason)
{
	struct cap_unicast_client *client;

	client = &bt_cap_unicast_clients[bt_conn_index(conn)];

	if (client->conn != NULL) {
		bt_conn_unref(client->conn);
	}
	(void)memset(client, 0, sizeof(*client));

	/* Check if the disconnecting connection is part of the current active
	 * procedure, and abort it if it is.
	 */
	if (atomic_test_bit(active_procedure.flags,
			    CAP_UNICAST_PROCEDURE_STATE_ACTIVE) &&
	    !atomic_test_bit(active_procedure.flags,
			     CAP_UNICAST_PROCEDURE_STATE_ABORTED)) {
		for (size_t i = 0U; i < active_procedure.stream_initiated; i++) {
			if (active_procedure.streams[i]->bap_stream.conn == conn) {
				active_procedure.err = -ENOTCONN;
				active_procedure.failed_conn = conn;
				atomic_set_bit(active_procedure.flags,
					       CAP_UNICAST_PROCEDURE_STATE_ABORTED);
				break;
			}
		}
	}
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.disconnected = cap_initiator_disconnected,
};

static struct cap_unicast_client *lookup_unicast_client_by_csis(
	const struct bt_csis_client_csis_inst *csis_inst)
{
	if (csis_inst == NULL) {
		return NULL;
	}

	for (size_t i = 0U; i < ARRAY_SIZE(bt_cap_unicast_clients); i++) {
		struct cap_unicast_client *client = &bt_cap_unicast_clients[i];

		if (client->csis_inst == csis_inst) {
			return client;
		}
	}

	return NULL;
}

static void csis_client_discover_cb(struct bt_conn *conn,
				    const struct bt_csis_client_set_member *member,
				    int err, size_t set_count)
{
	struct cap_unicast_client *client;

	if (err != 0) {
		BT_DBG("CSIS client discover failed: %d", err);

		if (cap_cb && cap_cb->discovery_complete) {
			cap_cb->discovery_complete(conn, err, NULL);
		}

		return;
	}

	client = &bt_cap_unicast_clients[bt_conn_index(conn)];
	client->csis_inst = bt_csis_client_csis_inst_by_handle(
					conn, client->csis_start_handle);

	if (member == NULL || set_count == 0 || client->csis_inst == NULL) {
		BT_ERR("Unable to find CSIS for CAS");

		if (cap_cb && cap_cb->discovery_complete) {
			cap_cb->discovery_complete(conn, -ENODATA, NULL);
		}
	} else {
		BT_DBG("Found CAS with CSIS");
		if (cap_cb && cap_cb->discovery_complete) {
			cap_cb->discovery_complete(conn, 0, client->csis_inst);
		}
	}
}

static uint8_t cap_unicast_discover_included_cb(struct bt_conn *conn,
						const struct bt_gatt_attr *attr,
						struct bt_gatt_discover_params *params)
{
	params->func = NULL;

	if (attr == NULL) {
		BT_DBG("CAS CSIS include not found");

		if (cap_cb && cap_cb->discovery_complete) {
			cap_cb->discovery_complete(conn, 0, NULL);
		}
	} else {
		const struct bt_gatt_include *included_service = attr->user_data;
		struct cap_unicast_client *client = CONTAINER_OF(params,
								 struct cap_unicast_client,
								 param);

		/* If the remote CAS includes CSIS, we first check if we
		 * have already discovered it, and if so we can just retrieve it
		 * and forward it to the application. If not, then we start
		 * CSIS discovery
		 */
		client->csis_start_handle = included_service->start_handle;
		client->csis_inst = bt_csis_client_csis_inst_by_handle(
					conn, client->csis_start_handle);

		if (client->csis_inst == NULL) {
			static struct bt_csis_client_cb csis_client_cb = {
				.discover = csis_client_discover_cb
			};
			static bool csis_cbs_registered;
			int err;

			BT_DBG("CAS CSIS not known, discovering");

			if (!csis_cbs_registered) {
				bt_csis_client_register_cb(&csis_client_cb);
				csis_cbs_registered = true;
			}

			err = bt_csis_client_discover(conn);
			if (err != 0) {
				BT_DBG("Discover failed (err %d)", err);
				if (cap_cb && cap_cb->discovery_complete) {
					cap_cb->discovery_complete(conn, err,
								   NULL);
				}
			}
		} else if (cap_cb && cap_cb->discovery_complete) {
			BT_DBG("Found CAS with CSIS");
			cap_cb->discovery_complete(conn, 0, client->csis_inst);
		}
	}

	return BT_GATT_ITER_STOP;
}

static uint8_t cap_unicast_discover_cas_cb(struct bt_conn *conn,
				       const struct bt_gatt_attr *attr,
				       struct bt_gatt_discover_params *params)
{
	params->func = NULL;

	if (attr == NULL) {
		if (cap_cb && cap_cb->discovery_complete) {
			cap_cb->discovery_complete(conn, -ENODATA, NULL);
		}
	} else {
		const struct bt_gatt_service_val *prim_service = attr->user_data;
		struct cap_unicast_client *client = CONTAINER_OF(params,
								 struct cap_unicast_client,
								 param);
		int err;

		client->cas_found = true;
		client->conn = bt_conn_ref(conn);

		if (attr->handle == prim_service->end_handle) {
			BT_DBG("Found CAS without CSIS");
			cap_cb->discovery_complete(conn, 0, NULL);

			return BT_GATT_ITER_STOP;
		}

		BT_DBG("Found CAS, discovering included CSIS");

		params->uuid = NULL;
		params->start_handle = attr->handle + 1;
		params->end_handle = prim_service->end_handle;
		params->type = BT_GATT_DISCOVER_INCLUDE;
		params->func = cap_unicast_discover_included_cb;

		err = bt_gatt_discover(conn, params);
		if (err != 0) {
			BT_DBG("Discover failed (err %d)", err);

			params->func = NULL;
			if (cap_cb && cap_cb->discovery_complete) {
				cap_cb->discovery_complete(conn, err, NULL);
			}
		}
	}

	return BT_GATT_ITER_STOP;
}

int bt_cap_initiator_unicast_discover(struct bt_conn *conn)
{
	struct bt_gatt_discover_params *param;
	int err;

	CHECKIF(conn == NULL) {
		BT_DBG("NULL conn");
		return -EINVAL;
	}

	param = &bt_cap_unicast_clients[bt_conn_index(conn)].param;

	/* use param->func to tell if a client is "busy" */
	if (param->func != NULL) {
		return -EBUSY;
	}

	param->func = cap_unicast_discover_cas_cb;
	param->uuid = cas_uuid;
	param->type = BT_GATT_DISCOVER_PRIMARY;
	param->start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	param->end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;

	err = bt_gatt_discover(conn, param);
	if (err != 0) {
		param->func = NULL;
	}

	return err;
}

static bool cap_stream_in_active_procedure(const struct bt_cap_stream *cap_stream)
{
	if (!atomic_test_bit(active_procedure.flags,
			     CAP_UNICAST_PROCEDURE_STATE_ACTIVE)) {
		return false;
	}

	for (size_t i = 0U; i < active_procedure.stream_cnt; i++) {
		if (active_procedure.streams[i] == cap_stream) {
			return true;
		}
	}

	return false;
}

static bool valid_unicast_audio_start_param(const struct bt_cap_unicast_audio_start_param *param)
{
	CHECKIF(param == NULL) {
		BT_DBG("param is NULL");
		return false;
	}

	CHECKIF(param->count == 0) {
		BT_DBG("Invalid param->count: %u", param->count);
		return false;
	}

	CHECKIF(param->stream_params == NULL) {
		BT_DBG("param->mestream_paramsmbers is NULL");
		return false;
	}

	CHECKIF(param->count > CONFIG_BT_AUDIO_UNICAST_CLIENT_GROUP_STREAM_COUNT) {
		BT_DBG("param->count (%zu) is larger than "
		       "CONFIG_BT_AUDIO_UNICAST_CLIENT_GROUP_STREAM_COUNT (%d)",
		       param->count,
		       CONFIG_BT_AUDIO_UNICAST_CLIENT_GROUP_STREAM_COUNT);
		return false;
	}

	for (size_t i = 0U; i < param->count; i++) {
		const struct bt_cap_unicast_audio_start_stream_param *stream_param =
									&param->stream_params[i];
		const union bt_cap_set_member *member = &stream_param->member;
		const struct bt_cap_stream *cap_stream = stream_param->stream;
		const struct bt_codec *codec = stream_param->codec;
		const struct bt_audio_stream *bap_stream;

		CHECKIF(stream_param->codec == NULL) {
			BT_DBG("param->stream_params[%zu].codec is NULL", i);
			return false;
		}

		CHECKIF(!cap_initiator_valid_metadata(codec->meta,
						      codec->meta_count)) {
			BT_DBG("param->stream_params[%zu].codec is invalid", i);
			return false;
		}

		CHECKIF(stream_param->ep == NULL) {
			BT_DBG("param->stream_params[%zu].ep is NULL", i);
			return false;
		}

		CHECKIF(stream_param->qos == NULL) {
			BT_DBG("param->stream_params[%zu].qos is NULL", i);
			return false;
		}

		CHECKIF(member == NULL) {
			BT_DBG("param->stream_params[%zu].member is NULL", i);
			return false;
		}

		if (param->type == BT_CAP_SET_TYPE_AD_HOC) {
			struct cap_unicast_client *client;

			CHECKIF(member->member == NULL) {
				BT_DBG("param->members[%zu] is NULL", i);
				return false;
			}

			client = &bt_cap_unicast_clients[bt_conn_index(member->member)];

			if (!client->cas_found) {
				BT_DBG("CAS was not found for param->members[%zu]", i);
				return false;
			}
		}

		if (param->type == BT_CAP_SET_TYPE_CSIP) {
			struct cap_unicast_client *client;

			CHECKIF(member->csip == NULL) {
				BT_DBG("param->csip.set[%zu] is NULL", i);
				return false;
			}

			client = lookup_unicast_client_by_csis(member->csip);
			if (client == NULL) {
				BT_DBG("CSIS was not found for param->members[%zu]", i);
				return false;
			}
		}

		CHECKIF(cap_stream == NULL) {
			BT_DBG("param->streams[%zu] is NULL", i);
			return false;
		}

		bap_stream = &cap_stream->bap_stream;

		CHECKIF(bap_stream->ep != NULL) {
			BT_DBG("param->streams[%zu] is already started", i);
			return false;
		}

		CHECKIF(bap_stream->group != NULL) {
			BT_DBG("param->streams[%zu] is already in a unicast group", i);
			return false;
		}

		for (size_t j = 0U; j < i; j++) {
			if (param->stream_params[j].stream == cap_stream) {
				BT_DBG("param->stream_params[%zu] (%p) is "
				       "duplicated by "
				       "param->stream_params[%zu] (%p)",
				       j, param->stream_params[j].stream,
				       i, cap_stream);
				return false;
			}
		}
	}

	return true;
}

static int cap_initiator_unicast_audio_create_group(
	const struct bt_cap_unicast_audio_start_param *param,
	struct bt_audio_unicast_group **unicast_group)
{
	struct bt_audio_unicast_group_param group_param[
		CONFIG_BT_AUDIO_UNICAST_CLIENT_GROUP_STREAM_COUNT];

	for (size_t i = 0U; i < param->count; i++) {
		group_param[i].stream = &param->stream_params[i].stream->bap_stream;
		group_param[i].qos = param->stream_params[i].qos;
		group_param[i].dir = param->stream_params[i].ep->dir;
	}

	return bt_audio_unicast_group_create(group_param, param->count,
					     unicast_group);
}

static int cap_initiator_unicast_audio_configure(
	const struct bt_cap_unicast_audio_start_param *param)
{
	/** TODO: If this is a CSIP set, then the order of the procedures may
	 * not match the order in the parameters, and the CSIP ordered access
	 * procedure should be used.
	 */

	/* Store the information about the active procedure so that we can
	 * continue the procedure after each step
	 */
	atomic_set_bit(active_procedure.flags,
		       CAP_UNICAST_PROCEDURE_STATE_ACTIVE);
	active_procedure.stream_cnt = param->count;

	for (size_t i = 0U; i < param->count; i++) {
		struct bt_cap_unicast_audio_start_stream_param *stream_param =
									&param->stream_params[i];
		union bt_cap_set_member *member = &stream_param->member;
		struct bt_cap_stream *cap_stream = stream_param->stream;
		struct bt_audio_stream *bap_stream = &cap_stream->bap_stream;
		struct bt_audio_ep *ep = stream_param->ep;
		struct bt_codec *codec = stream_param->codec;
		struct bt_conn *conn;
		int err;

		if (param->type == BT_CAP_SET_TYPE_AD_HOC) {
			conn = member->member;
		} else {
			struct cap_unicast_client *client;

			/* We have verified that `client` wont be NULL in
			 * `valid_unicast_audio_start_param`.
			 */
			client = lookup_unicast_client_by_csis(member->csip);
			conn = client->conn;
		}

		/* Ensure that ops are registered before any procedures are started */
		bt_cap_stream_ops_register_bap(cap_stream);

		active_procedure.streams[i] = cap_stream;

		err = bt_audio_stream_config(conn, bap_stream, ep, codec);
		if (err != 0) {
			BT_DBG("bt_audio_stream_config failed for param->stream_params[%zu]: %d",
			       i, err);

			if (i > 0U) {
				atomic_set_bit(active_procedure.flags,
					       CAP_UNICAST_PROCEDURE_STATE_ABORTED);
			} else {
				(void)memset(&active_procedure, 0,
					     sizeof(active_procedure));
			}

			return err;
		}

		active_procedure.stream_initiated++;
	}

	return 0;
}

static void cap_initiator_unicast_audio_start_complete(void)
{
	struct bt_audio_unicast_group *unicast_group;
	struct bt_conn *failed_conn;
	int err;

	/* All streams in the procedure share the same unicast group, so we just
	 * use the reference from the first stream
	 */
	unicast_group = active_procedure.streams[0]->bap_stream.unicast_group;
	failed_conn = active_procedure.failed_conn;
	err = active_procedure.err;

	(void)memset(&active_procedure, 0, sizeof(active_procedure));
	if (cap_cb != NULL && cap_cb->unicast_start_complete != NULL) {
		cap_cb->unicast_start_complete(unicast_group, err, failed_conn);
	}
}

int bt_cap_initiator_unicast_audio_start(const struct bt_cap_unicast_audio_start_param *param,
					 struct bt_audio_unicast_group **unicast_group)
{
	int err;

	if (atomic_test_bit(active_procedure.flags,
			    CAP_UNICAST_PROCEDURE_STATE_ACTIVE)) {
		BT_DBG("A CAP procedure is already in progress");

		return -EBUSY;
	}

	CHECKIF(unicast_group == NULL) {
		BT_DBG("unicast_group is NULL");
		return -EINVAL;
	}

	*unicast_group = NULL;

	if (!valid_unicast_audio_start_param(param)) {
		return -EINVAL;
	}

	err = cap_initiator_unicast_audio_create_group(param, unicast_group);
	if (err != 0) {
		BT_DBG("Failed to create unicast group: %d", err);
		return err;
	}

	err = cap_initiator_unicast_audio_configure(param);
	if (err != 0) {
		int del_err;

		BT_DBG("Failed to create unicast group: %d", err);

		del_err = bt_audio_unicast_group_delete(*unicast_group);
		if (del_err != 0) {
			BT_ERR("Could not delete unicast group: %d", del_err);
		} else {
			*unicast_group = NULL;
		}

		return err;
	}

	return 0;
}

void bt_cap_initiator_codec_configured(struct bt_cap_stream *cap_stream)
{
	struct bt_conn *conns[MIN(CONFIG_BT_MAX_CONN,
				  CONFIG_BT_AUDIO_UNICAST_CLIENT_GROUP_STREAM_COUNT)];
	struct bt_audio_unicast_group *unicast_group;

	if (!cap_stream_in_active_procedure(cap_stream)) {
		/* State change happened outside of a procedure; ignore */
		return;
	}

	active_procedure.stream_done++;

	BT_DBG("Stream %p configured (%zu/%zu streams done)",
	       cap_stream, active_procedure.stream_done,
	       active_procedure.stream_cnt);

	if (active_procedure.stream_done < active_procedure.stream_cnt) {
		/* Not yet finished, wait for all */
		return;
	} else if (atomic_test_bit(active_procedure.flags,
				   CAP_UNICAST_PROCEDURE_STATE_ABORTED)) {
		if (active_procedure.stream_done == active_procedure.stream_initiated) {
			cap_initiator_unicast_audio_start_complete();
		}

		return;
	}

	/* The QoS Configure procedure works on a set of connections and a
	 * unicast group, so we generate a list of unique connection pointers
	 * for the procedure
	 */
	(void)memset(conns, 0, sizeof(conns));
	for (size_t i = 0U; i < active_procedure.stream_cnt; i++) {
		struct bt_conn *stream_conn = active_procedure.streams[i]->bap_stream.conn;
		struct bt_conn **free_conn = NULL;
		bool already_added = false;

		for (size_t j = 0U; j < ARRAY_SIZE(conns); j++) {
			if (stream_conn == conns[j]) {
				already_added = true;
				break;
			} else if (conns[j] == NULL && free_conn == NULL) {
				free_conn = &conns[j];
			}
		}

		if (already_added) {
			continue;
		}

		__ASSERT(free_conn, "No free conns");
		*free_conn = stream_conn;
	}

	/* All streams in the procedure share the same unicast group, so we just
	 * use the reference from the first stream
	 */
	unicast_group = active_procedure.streams[0]->bap_stream.unicast_group;
	active_procedure.stream_done = 0U;
	active_procedure.stream_initiated = 0U;
	for (size_t i = 0U; i < ARRAY_SIZE(conns); i++) {
		int err;

		/* When conns[i] is NULL, we have QoS Configured all unique connections */
		if (conns[i] == NULL) {
			break;
		}

		err = bt_audio_stream_qos(conns[i], unicast_group);
		if (err != 0) {
			BT_DBG("Failed to set stream QoS for conn %p and group %p: %d",
			       (void *)conns[i], unicast_group, err);

			/* End or mark procedure as aborted.
			 * If we have sent any requests over air, we will abort
			 * once all sent requests has completed
			 */
			active_procedure.err = err;
			active_procedure.failed_conn = conns[i];
			if (i == 0U) {
				cap_initiator_unicast_audio_start_complete();
			} else {
				atomic_set_bit(active_procedure.flags,
					       CAP_UNICAST_PROCEDURE_STATE_ABORTED);
			}

			return;
		}

		active_procedure.stream_initiated++;
	}
}

void bt_cap_initiator_qos_configured(struct bt_cap_stream *cap_stream)
{
	if (!cap_stream_in_active_procedure(cap_stream)) {
		/* State change happened outside of a procedure; ignore */
		return;
	}

	active_procedure.stream_done++;

	BT_DBG("Stream %p QoS configured (%zu/%zu streams done)",
	       cap_stream, active_procedure.stream_done,
	       active_procedure.stream_cnt);

	if (active_procedure.stream_done < active_procedure.stream_cnt) {
		/* Not yet finished, wait for all */
		return;
	} else if (atomic_test_bit(active_procedure.flags,
				   CAP_UNICAST_PROCEDURE_STATE_ABORTED)) {
		if (active_procedure.stream_done == active_procedure.stream_initiated) {
			cap_initiator_unicast_audio_start_complete();
		}

		return;
	}

	active_procedure.stream_done = 0U;
	active_procedure.stream_initiated = 0U;

	for (size_t i = 0U; i < active_procedure.stream_cnt; i++) {
		struct bt_cap_stream *cap_stream = active_procedure.streams[i];
		struct bt_audio_stream *bap_stream = &cap_stream->bap_stream;
		int err;

		/* TODO: Add metadata */
		err = bt_audio_stream_enable(bap_stream,
					     bap_stream->codec->meta,
					     bap_stream->codec->meta_count);
		if (err != 0) {
			BT_DBG("Failed to enable stream %p: %d",
			       cap_stream, err);

			/* End or mark procedure as aborted.
			 * If we have sent any requests over air, we will abort
			 * once all sent requests has completed
			 */
			active_procedure.err = err;
			active_procedure.failed_conn = bap_stream->conn;
			if (i == 0U) {
				cap_initiator_unicast_audio_start_complete();
			} else {
				atomic_set_bit(active_procedure.flags,
					       CAP_UNICAST_PROCEDURE_STATE_ABORTED);
			}

			return;
		}
	}
}

void bt_cap_initiator_enabled(struct bt_cap_stream *cap_stream)
{
	struct bt_audio_stream *bap_stream;
	int err;

	if (!cap_stream_in_active_procedure(cap_stream)) {
		/* State change happened outside of a procedure; ignore */
		return;
	}

	active_procedure.stream_done++;

	BT_DBG("Stream %p enabled (%zu/%zu streams done)",
	       cap_stream, active_procedure.stream_done,
	       active_procedure.stream_cnt);

	if (active_procedure.stream_done < active_procedure.stream_cnt) {
		/* Not yet finished, wait for all */
		return;
	} else if (atomic_test_bit(active_procedure.flags,
				   CAP_UNICAST_PROCEDURE_STATE_ABORTED)) {
		if (active_procedure.stream_done == active_procedure.stream_initiated) {
			cap_initiator_unicast_audio_start_complete();
		}

		return;
	}

	active_procedure.stream_done = 0U;
	active_procedure.stream_initiated = 0U;

	bap_stream = &active_procedure.streams[0]->bap_stream;

	/* Since bt_audio_stream_start connects the ISO, we can, at this point,
	 * only do this one by one due to a restriction in the ISO layer
	 * (maximum 1 outstanding ISO connection request at any one time).
	 */
	err = bt_audio_stream_start(bap_stream);
	if (err != 0) {
		BT_DBG("Failed to start stream %p: %d",
			active_procedure.streams[0], err);

		/* End and mark procedure as aborted.
		 * If we have sent any requests over air, we will abort
		 * once all sent requests has completed
		 */
		active_procedure.err = err;
		active_procedure.failed_conn = bap_stream->conn;
		cap_initiator_unicast_audio_start_complete();

		return;
	}
}

void bt_cap_initiator_started(struct bt_cap_stream *cap_stream)
{
	if (!cap_stream_in_active_procedure(cap_stream)) {
		/* State change happened outside of a procedure; ignore */
		return;
	}

	active_procedure.stream_done++;

	BT_DBG("Stream %p started (%zu/%zu streams done)",
	       cap_stream, active_procedure.stream_done,
	       active_procedure.stream_cnt);

	/* Since bt_audio_stream_start connects the ISO, we can, at this point,
	 * only do this one by one due to a restriction in the ISO layer
	 * (maximum 1 outstanding ISO connection request at any one time).
	 */
	if (active_procedure.stream_done < active_procedure.stream_cnt) {
		struct bt_cap_stream *next = active_procedure.streams[active_procedure.stream_done];
		struct bt_audio_stream *bap_stream = &next->bap_stream;
		int err;

		/* Not yet finished, start next */
		err = bt_audio_stream_start(bap_stream);
		if (err != 0) {
			BT_DBG("Failed to start stream %p: %d", next, err);

			/* End and mark procedure as aborted.
			 * If we have sent any requests over air, we will abort
			 * once all sent requests has completed
			 */
			active_procedure.err = err;
			active_procedure.failed_conn = bap_stream->conn;
			cap_initiator_unicast_audio_start_complete();
		}
	} else {
		cap_initiator_unicast_audio_start_complete();
	}
}

static void cap_initiator_unicast_audio_update_complete(void)
{
	struct bt_conn *failed_conn;
	int err;

	failed_conn = active_procedure.failed_conn;
	err = active_procedure.err;

	(void)memset(&active_procedure, 0, sizeof(active_procedure));
	if (cap_cb != NULL && cap_cb->unicast_update_complete != NULL) {
		cap_cb->unicast_update_complete(err, failed_conn);
	}
}

int bt_cap_initiator_unicast_audio_update(const struct bt_cap_unicast_audio_update_param params[],
					  size_t count)
{
	if (atomic_test_bit(active_procedure.flags,
			    CAP_UNICAST_PROCEDURE_STATE_ACTIVE)) {
		BT_DBG("A CAP procedure is already in progress");

		return -EBUSY;
	}

	for (size_t i = 0U; i < count; i++) {
		CHECKIF(params[i].stream == NULL) {
			BT_DBG("params[%zu].stream is NULL", i);

			return -EINVAL;
		}

		CHECKIF(params[i].stream->bap_stream.conn == NULL) {
			BT_DBG("params[%zu].stream->bap_stream.conn is NULL", i);

			return -EINVAL;
		}

		CHECKIF(!cap_initiator_valid_metadata(params[i].meta,
						      params[i].meta_count)) {
			BT_DBG("params[%zu].meta is invalid", i);

			return -EINVAL;
		}

		for (size_t j = 0U; j < i; j++) {
			if (params[j].stream == params[i].stream) {
				BT_DBG("param.streams[%zu] is duplicated by param.streams[%zu]",
				       j, i);
				return false;
			}
		}
	}

	atomic_set_bit(active_procedure.flags,
		       CAP_UNICAST_PROCEDURE_STATE_ACTIVE);
	active_procedure.stream_cnt = count;

	/** TODO: If this is a CSIP set, then the order of the procedures may
	 * not match the order in the parameters, and the CSIP ordered access
	 * procedure should be used.
	 */
	for (size_t i = 0U; i < count; i++) {
		int err;

		active_procedure.streams[i] = params[i].stream;

		err = bt_audio_stream_metadata(&params[i].stream->bap_stream,
					       params[i].meta,
					       params[i].meta_count);
		if (err != 0) {
			BT_DBG("Failed to update metadata for stream %p: %d",
			       params[i].stream, err);

			active_procedure.err = err;
			active_procedure.failed_conn = params[i].stream->bap_stream.conn;

			if (i > 0U) {
				atomic_set_bit(active_procedure.flags,
					       CAP_UNICAST_PROCEDURE_STATE_ABORTED);
			} else {
				(void)memset(&active_procedure, 0,
					     sizeof(active_procedure));
			}

			return err;
		}

		active_procedure.stream_initiated++;
	}

	return 0;
}

void bt_cap_initiator_metadata_updated(struct bt_cap_stream *cap_stream)
{
	if (!cap_stream_in_active_procedure(cap_stream)) {
		/* State change happened outside of a procedure; ignore */
		return;
	}

	active_procedure.stream_done++;

	BT_DBG("Stream %p QoS metadata updated (%zu/%zu streams done)",
	       cap_stream, active_procedure.stream_done,
	       active_procedure.stream_cnt);

	if (active_procedure.stream_done < active_procedure.stream_cnt) {
		/* Not yet finished, wait for all */
		return;
	} else if (atomic_test_bit(active_procedure.flags,
				   CAP_UNICAST_PROCEDURE_STATE_ABORTED)) {
		if (active_procedure.stream_done == active_procedure.stream_initiated) {
			cap_initiator_unicast_audio_update_complete();
		}

		return;
	}

	cap_initiator_unicast_audio_update_complete();
}

int bt_cap_initiator_unicast_audio_stop(struct bt_audio_unicast_group *unicast_group)
{
	return -ENOSYS;
}

#endif /* CONFIG_BT_AUDIO_UNICAST_CLIENT */

#if defined(CONFIG_BT_AUDIO_BROADCAST_SOURCE) && defined(CONFIG_BT_AUDIO_UNICAST_CLIENT)

int bt_cap_initiator_unicast_to_broadcast(
	const struct bt_cap_unicast_to_broadcast_param *param,
	struct bt_cap_broadcast_source **source)
{
	return -ENOSYS;
}

int bt_cap_initiator_broadcast_to_unicast(const struct bt_cap_broadcast_to_unicast_param *param,
					  struct bt_audio_unicast_group **unicast_group)
{
	return -ENOSYS;
}

#endif /* CONFIG_BT_AUDIO_BROADCAST_SOURCE && CONFIG_BT_AUDIO_UNICAST_CLIENT */
