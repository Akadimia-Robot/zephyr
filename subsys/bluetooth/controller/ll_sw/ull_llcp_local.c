/*
 * Copyright (c) 2020 Demant
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>

#include <bluetooth/hci.h>
#include <sys/byteorder.h>
#include <sys/slist.h>
#include <sys/util.h>

#include "hal/ccm.h"

#include "util/mem.h"
#include "util/memq.h"

#include "pdu.h"
#include "ll.h"
#include "ll_settings.h"

#include "lll.h"
#include "lll_conn.h"

#include "ull_tx_queue.h"

#include "ull_conn_types.h"
#include "ull_llcp.h"
#include "ull_llcp_internal.h"

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#define LOG_MODULE_NAME bt_ctlr_ull_llcp_local
#include "common/log.h"
#include <soc.h>
#include "hal/debug.h"

static void lr_check_done(struct ll_conn *conn, struct proc_ctx *ctx);
static struct proc_ctx *lr_dequeue(struct ll_conn *conn);

/* LLCP Local Request FSM State */
enum lr_state {
	LR_STATE_IDLE,
	LR_STATE_ACTIVE,
	LR_STATE_DISCONNECT,
	LR_STATE_TERMINATE,
};

/* LLCP Local Request FSM Event */
enum {
	/* Procedure run */
	LR_EVT_RUN,

	/* Procedure completed */
	LR_EVT_COMPLETE,

	/* Link connected */
	LR_EVT_CONNECT,

	/* Link disconnected */
	LR_EVT_DISCONNECT,
};

typedef void (*llcp_run_fsm_transition_t)(struct ll_conn *conn, struct proc_ctx *ctx, void *param);
struct llcp_lp_fsm {
	llcp_run_fsm_transition_t on_run;
};

static const struct llcp_lp_fsm common_fsm = {
	.on_run = lp_comm_run,
};

static const struct llcp_lp_fsm encryption_start_fsm = {
	.on_run = lp_enc_run,
};

static const struct llcp_lp_fsm encryption_pause_fsm = {
	.on_run = lp_enc_run,
};

static const struct llcp_lp_fsm phy_update_fsm = {
	.on_run = lp_pu_run,
};

static const struct llcp_lp_fsm conn_update_fsm = {
	.on_run = lp_cu_run,
};

static const struct llcp_lp_fsm conn_param_req_fsm = {
	.on_run = lp_cu_run,
};

static const struct llcp_lp_fsm *llcp_fsm[] = {
	[PROC_LE_PING] = &common_fsm,
	[PROC_FEATURE_EXCHANGE] = &common_fsm,
	[PROC_MIN_USED_CHANS] = &common_fsm,
	[PROC_VERSION_EXCHANGE] = &common_fsm,
	[PROC_ENCRYPTION_START] = &encryption_start_fsm,
	[PROC_ENCRYPTION_PAUSE] = &encryption_pause_fsm,
	[PROC_PHY_UPDATE] = &phy_update_fsm,
	[PROC_CONN_UPDATE] = &conn_update_fsm,
	[PROC_CONN_PARAM_REQ] = &conn_param_req_fsm,
	[PROC_TERMINATE] = &common_fsm,
	[PROC_CHAN_MAP_UPDATE] = &common_fsm,
	[PROC_DATA_LENGTH_UPDATE] = &common_fsm,
};

static void lr_check_done(struct ll_conn *conn, struct proc_ctx *ctx)
{
	if (ctx->done) {
		struct proc_ctx *ctx_header;

		ctx_header = lr_peek(conn);
		LL_ASSERT(ctx_header == ctx);

		lr_dequeue(conn);
		ull_cp_priv_proc_ctx_release(ctx);
	}
}
/*
 * LLCP Local Request FSM
 */

static void lr_set_state(struct ll_conn *conn, enum lr_state state)
{
	conn->llcp.local.state = state;
}

void ull_cp_priv_lr_enqueue(struct ll_conn *conn, struct proc_ctx *ctx)
{
	sys_slist_append(&conn->llcp.local.pend_proc_list, &ctx->node);
}

static struct proc_ctx *lr_dequeue(struct ll_conn *conn)
{
	struct proc_ctx *ctx;

	ctx = (struct proc_ctx *) sys_slist_get(&conn->llcp.local.pend_proc_list);
	return ctx;
}

struct proc_ctx *lr_peek(struct ll_conn *conn)
{
	struct proc_ctx *ctx;

	ctx = (struct proc_ctx *) sys_slist_peek_head(&conn->llcp.local.pend_proc_list);
	return ctx;
}

void ull_cp_priv_lr_rx(struct ll_conn *conn, struct proc_ctx *ctx, struct node_rx_pdu *rx)
{
	switch (ctx->proc) {
	case PROC_LE_PING:
		lp_comm_rx(conn, ctx, rx);
		break;
	case PROC_FEATURE_EXCHANGE:
		lp_comm_rx(conn, ctx, rx);
		break;
	case PROC_MIN_USED_CHANS:
		lp_comm_rx(conn, ctx, rx);
		break;
	case PROC_VERSION_EXCHANGE:
		lp_comm_rx(conn, ctx, rx);
		break;
	case PROC_ENCRYPTION_START:
	case PROC_ENCRYPTION_PAUSE:
		lp_enc_rx(conn, ctx, rx);
		break;
	case PROC_PHY_UPDATE:
		lp_pu_rx(conn, ctx, rx);
		break;
	case PROC_CONN_PARAM_REQ:
		lp_cu_rx(conn, ctx, rx);
		break;
	case PROC_TERMINATE:
		lp_comm_rx(conn, ctx, rx);
		break;
	case PROC_DATA_LENGTH_UPDATE:
		lp_comm_rx(conn, ctx, rx);
		break;
	default:
		/* Unknown procedure */
		LL_ASSERT(0);
	}

	lr_check_done(conn, ctx);
}

void ull_cp_priv_lr_tx_ack(struct ll_conn *conn, struct proc_ctx *ctx, struct node_tx *tx)
{
	switch (ctx->proc) {
	case PROC_MIN_USED_CHANS:
		lp_comm_tx_ack(conn, ctx, tx);
		break;
	case PROC_TERMINATE:
		lp_comm_tx_ack(conn, ctx, tx);
		break;
	case PROC_DATA_LENGTH_UPDATE:
		lp_comm_tx_ack(conn, ctx, tx);
		break;
	default:
		break;
		/* Ignore tx_ack */
	}
	lr_check_done(conn, ctx);
}

static void lr_act_run(struct ll_conn *conn)
{
	struct proc_ctx *ctx;

	ctx = lr_peek(conn);
	LL_ASSERT(ctx->proc < PROC_UNKNOWN);

	llcp_run_fsm_transition_t on_run = llcp_fsm[ctx->proc]->on_run;

	if (on_run) {
		on_run(conn, ctx, NULL);
	}

	lr_check_done(conn, ctx);
}

static void lr_act_complete(struct ll_conn *conn)
{
	struct proc_ctx *ctx;

	ctx = lr_peek(conn);

	ctx->done = 1U;
}

static void lr_act_connect(struct ll_conn *conn)
{
	/* TODO */
}

static void lr_act_disconnect(struct ll_conn *conn)
{
	struct proc_ctx *ctx;

	ctx = lr_dequeue(conn);

	/*
	 * we may have been disconnected in the
	 * middle of a control procedure, in
	 * which case we need to release context
	 */
	while (ctx != NULL) {
		proc_ctx_release(ctx);
		ctx = lr_dequeue(conn);
	}
}

static void lr_st_disconnect(struct ll_conn *conn, uint8_t evt, void *param)
{
	switch (evt) {
	case LR_EVT_CONNECT:
		lr_act_connect(conn);
		lr_set_state(conn, LR_STATE_IDLE);
		break;
	default:
		/* Ignore other evts */
		break;
	}
}

static void lr_st_idle(struct ll_conn *conn, uint8_t evt, void *param)
{
	struct proc_ctx *ctx;

	switch (evt) {
	case LR_EVT_RUN:
		if ((ctx = lr_peek(conn))) {
			lr_act_run(conn);
			if (ctx->proc != PROC_TERMINATE) {
				lr_set_state(conn, LR_STATE_ACTIVE);
			} else {
				lr_set_state(conn, LR_STATE_TERMINATE);
			}
		}
		break;
	case LR_EVT_DISCONNECT:
		lr_act_disconnect(conn);
		lr_set_state(conn, LR_STATE_DISCONNECT);
		break;
	default:
		/* Ignore other evts */
		break;
	}
}

static void lr_st_active(struct ll_conn *conn, uint8_t evt, void *param)
{
	switch (evt) {
	case LR_EVT_RUN:
		if (lr_peek(conn)) {
			lr_act_run(conn);
		}
		break;
	case LR_EVT_COMPLETE:
		lr_act_complete(conn);
		lr_set_state(conn, LR_STATE_IDLE);
		break;
	case LR_EVT_DISCONNECT:
		lr_act_disconnect(conn);
		lr_set_state(conn, LR_STATE_DISCONNECT);
		break;
	default:
		/* Ignore other evts */
		break;
	}
}

static void lr_st_terminate(struct ll_conn *conn, uint8_t evt, void *param)
{
	switch (evt) {
	case LR_EVT_RUN:
		if (lr_peek(conn)) {
			lr_act_run(conn);
		}
		break;
	case LR_EVT_COMPLETE:
		lr_act_complete(conn);
		lr_set_state(conn, LR_STATE_IDLE);
		break;
	case LR_EVT_DISCONNECT:
		lr_act_disconnect(conn);
		lr_set_state(conn, LR_STATE_DISCONNECT);
		break;
	default:
		/* Ignore other evts */
		break;
	}
}

static void lr_execute_fsm(struct ll_conn *conn, uint8_t evt, void *param)
{
	switch (conn->llcp.local.state) {
	case LR_STATE_DISCONNECT:
		lr_st_disconnect(conn, evt, param);
		break;
	case LR_STATE_IDLE:
		lr_st_idle(conn, evt, param);
		break;
	case LR_STATE_ACTIVE:
		lr_st_active(conn, evt, param);
		break;
	case LR_STATE_TERMINATE:
		lr_st_terminate(conn, evt, param);
		break;
	default:
		/* Unknown state */
		LL_ASSERT(0);
	}
}

void ull_cp_priv_lr_init(struct ll_conn *conn)
{
	lr_set_state(conn, LR_STATE_DISCONNECT);
}

void ull_cp_priv_lr_run(struct ll_conn *conn)
{
	lr_execute_fsm(conn, LR_EVT_RUN, NULL);
}

void ull_cp_priv_lr_complete(struct ll_conn *conn)
{
	lr_execute_fsm(conn, LR_EVT_COMPLETE, NULL);
}

void ull_cp_priv_lr_connect(struct ll_conn *conn)
{
	lr_execute_fsm(conn, LR_EVT_CONNECT, NULL);
}

void ull_cp_priv_lr_disconnect(struct ll_conn *conn)
{
	lr_execute_fsm(conn, LR_EVT_DISCONNECT, NULL);
}

void ull_cp_priv_lr_abort(struct ll_conn *conn)
{
	struct proc_ctx *ctx;

	/* Flush all pending procedures */
	ctx = lr_dequeue(conn);
	while (ctx) {
		proc_ctx_release(ctx);
		ctx = lr_dequeue(conn);
	}

	/* TODO(thoh): Whats missing here ??? */
	rr_set_incompat(conn, 0U);
	lr_set_state(conn, LR_STATE_IDLE);
}

#ifdef ZTEST_UNITTEST

bool lr_is_disconnected(struct ll_conn *conn)
{
	return conn->llcp.local.state == LR_STATE_DISCONNECT;
}

bool lr_is_idle(struct ll_conn *conn)
{
	return conn->llcp.local.state == LR_STATE_IDLE;
}

void test_int_local_pending_requests(void)
{
	struct ll_conn conn;
	struct proc_ctx *peek_ctx;
	struct proc_ctx *dequeue_ctx;
	struct proc_ctx ctx;

	ull_cp_init();
	ull_tx_q_init(&conn.tx_q);
	ll_conn_init(&conn);

	peek_ctx = lr_peek(&conn);
	zassert_is_null(peek_ctx, NULL);

	dequeue_ctx = lr_dequeue(&conn);
	zassert_is_null(dequeue_ctx, NULL);

	lr_enqueue(&conn, &ctx);
	peek_ctx = (struct proc_ctx *) sys_slist_peek_head(&conn.llcp.local.pend_proc_list);
	zassert_equal_ptr(peek_ctx, &ctx, NULL);

	peek_ctx = lr_peek(&conn);
	zassert_equal_ptr(peek_ctx, &ctx, NULL);

	dequeue_ctx = lr_dequeue(&conn);
	zassert_equal_ptr(dequeue_ctx, &ctx, NULL);

	peek_ctx = lr_peek(&conn);
	zassert_is_null(peek_ctx, NULL);

	dequeue_ctx = lr_dequeue(&conn);
	zassert_is_null(dequeue_ctx, NULL);
}

#endif
