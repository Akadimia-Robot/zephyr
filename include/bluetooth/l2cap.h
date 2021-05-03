/** @file
 *  @brief Bluetooth L2CAP handling
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_BLUETOOTH_L2CAP_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_L2CAP_H_

/**
 * @brief L2CAP
 * @defgroup bt_l2cap L2CAP
 * @ingroup bluetooth
 * @{
 */

#include <sys/atomic.h>
#include <bluetooth/buf.h>
#include <bluetooth/conn.h>
#include <bluetooth/hci.h>

#ifdef __cplusplus
extern "C" {
#endif

/** L2CAP PDU header size, used for buffer size calculations */
#define BT_L2CAP_HDR_SIZE               4

/** Maximum Transmission Unit (MTU) for an outgoing L2CAP PDU. */
#define BT_L2CAP_TX_MTU (CONFIG_BT_L2CAP_TX_MTU)

/** Maximum Transmission Unit (MTU) for an incoming L2CAP PDU. */
#define BT_L2CAP_RX_MTU (CONFIG_BT_BUF_ACL_RX_SIZE - BT_L2CAP_HDR_SIZE)

/** @def BT_L2CAP_BUF_SIZE
 *
 *  @brief Helper to calculate needed buffer size for L2CAP PDUs.
 *         Useful for creating buffer pools.
 *
 *  @param mtu Needed L2CAP PDU MTU.
 *
 *  @return Needed buffer size to match the requested L2CAP PDU MTU.
 */
#define BT_L2CAP_BUF_SIZE(mtu) BT_BUF_ACL_SIZE(BT_L2CAP_HDR_SIZE + (mtu))

/** L2CAP SDU header size, used for buffer size calculations */
#define BT_L2CAP_SDU_HDR_SIZE           2

/** @brief Maximum Transmission Unit for an unsegmented outgoing L2CAP SDU.
 *
 *  The Maximum Transmission Unit for an outgoing L2CAP SDU when sent without
 *  segmentation, i.e a single L2CAP SDU will fit inside a single L2CAP PDU.
 *
 *  The MTU for outgoing L2CAP SDUs with segmentation is defined by the
 *  size of the application buffer pool.
 */
#define BT_L2CAP_SDU_TX_MTU (BT_L2CAP_TX_MTU - BT_L2CAP_SDU_HDR_SIZE)

/** @brief Maximum Transmission Unit for an unsegmented incoming L2CAP SDU.
 *
 *  The Maximum Transmission Unit for an incoming L2CAP SDU when sent without
 *  segmentation, i.e a single L2CAP SDU will fit inside a single L2CAP PDU.
 *
 *  The MTU for incoming L2CAP SDUs with segmentation is defined by the
 *  size of the application buffer pool. The application will have to define
 *  an alloc_buf callback for the channel in order to support receiving
 *  segmented L2CAP SDUs.
 */
#define BT_L2CAP_SDU_RX_MTU (BT_L2CAP_RX_MTU - BT_L2CAP_SDU_HDR_SIZE)

/** @def BT_L2CAP_SDU_BUF_SIZE
 *
 *  @brief Helper to calculate needed buffer size for L2CAP SDUs.
 *         Useful for creating buffer pools.
 *
 *  @param mtu Required BT_L2CAP_*_SDU.
 *
 *  @return Needed buffer size to match the requested L2CAP SDU MTU.
 */
#define BT_L2CAP_SDU_BUF_SIZE(mtu) BT_L2CAP_BUF_SIZE(BT_L2CAP_SDU_HDR_SIZE + (mtu))

struct bt_l2cap_chan;

/** @typedef bt_l2cap_chan_destroy_t
 *  @brief Channel destroy callback
 *
 *  @param chan Channel object.
 */
typedef void (*bt_l2cap_chan_destroy_t)(struct bt_l2cap_chan *chan);

/** @brief Life-span states of L2CAP CoC channel.
 *
 *  Used only by internal APIs dealing with setting channel to proper state
 *  depending on operational context.
 */
typedef enum bt_l2cap_chan_state {
	/** Channel disconnected */
	BT_L2CAP_DISCONNECTED,
	/** Channel in connecting state */
	BT_L2CAP_CONNECT,
	/** Channel in config state, BR/EDR specific */
	BT_L2CAP_CONFIG,
	/** Channel ready for upper layer traffic on it */
	BT_L2CAP_CONNECTED,
	/** Channel in disconnecting state */
	BT_L2CAP_DISCONNECT,

} __packed bt_l2cap_chan_state_t;

/** @brief Status of L2CAP channel. */
typedef enum bt_l2cap_chan_status {
	/** Channel output status */
	BT_L2CAP_STATUS_OUT,

	/** @brief Channel shutdown status
	 *
	 * Once this status is notified it means the channel will no longer be
	 * able to transmit or receive data.
	 */
	BT_L2CAP_STATUS_SHUTDOWN,

	/** @brief Channel encryption pending status */
	BT_L2CAP_STATUS_ENCRYPT_PENDING,

	/* Total number of status - must be at the end of the enum */
	BT_L2CAP_NUM_STATUS,
} __packed bt_l2cap_chan_status_t;

/** @brief L2CAP Channel structure. */
struct bt_l2cap_chan {
	/** Channel connection reference */
	struct bt_conn			*conn;
	/** Channel operations reference */
	const struct bt_l2cap_chan_ops	*ops;
	sys_snode_t			node;
	bt_l2cap_chan_destroy_t		destroy;
	/* Response Timeout eXpired (RTX) timer */
	struct k_delayed_work		rtx_work;
	ATOMIC_DEFINE(status, BT_L2CAP_NUM_STATUS);

#if defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
	bt_l2cap_chan_state_t		state;
	/** Remote PSM to be connected */
	uint16_t				psm;
	/** Helps match request context during CoC */
	uint8_t				ident;
	bt_security_t			required_sec_level;
#endif /* CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */
};

/** @brief LE L2CAP Endpoint structure. */
struct bt_l2cap_le_endpoint {
	/** Endpoint Channel Identifier (CID) */
	uint16_t				cid;
	/** Endpoint Maximum Transmission Unit */
	uint16_t				mtu;
	/** Endpoint Maximum PDU payload Size */
	uint16_t				mps;
	/** Endpoint initial credits */
	uint16_t				init_credits;
	/** Endpoint credits */
	atomic_t			credits;
};

/** @brief LE L2CAP Channel structure. */
struct bt_l2cap_le_chan {
	/** Common L2CAP channel reference object */
	struct bt_l2cap_chan		chan;
	/** @brief Channel Receiving Endpoint.
	 *
	 *  If the application has set an alloc_buf channel callback for the
	 *  channel to support receiving segmented L2CAP SDUs the application
	 *  should inititalize the MTU of the Receiving Endpoint. Otherwise the
	 *  MTU of the receiving endpoint will be initialized to
	 *  @ref BT_L2CAP_SDU_RX_MTU by the stack.
	 */
	struct bt_l2cap_le_endpoint	rx;
	/** Channel Transmission Endpoint */
	struct bt_l2cap_le_endpoint	tx;
	/** Channel Transmission queue */
	struct k_fifo                   tx_queue;
	/** Channel Pending Transmission buffer  */
	struct net_buf                  *tx_buf;
	/** Channel Transmission work  */
	struct k_work			tx_work;
	/** Segment SDU packet from upper layer */
	struct net_buf			*_sdu;
	uint16_t				_sdu_len;

	struct k_work			rx_work;
	struct k_fifo			rx_queue;
};

/** @def BT_L2CAP_LE_CHAN(_ch)
 *  @brief Helper macro getting container object of type bt_l2cap_le_chan
 *  address having the same container chan member address as object in question.
 *
 *  @param _ch Address of object of bt_l2cap_chan type
 *
 *  @return Address of in memory bt_l2cap_le_chan object type containing
 *          the address of in question object.
 */
#define BT_L2CAP_LE_CHAN(_ch) CONTAINER_OF(_ch, struct bt_l2cap_le_chan, chan)

/** @brief BREDR L2CAP Endpoint structure. */
struct bt_l2cap_br_endpoint {
	/** Endpoint Channel Identifier (CID) */
	uint16_t				cid;
	/** Endpoint Maximum Transmission Unit */
	uint16_t				mtu;
};

/** @brief BREDR L2CAP Channel structure. */
struct bt_l2cap_br_chan {
	/** Common L2CAP channel reference object */
	struct bt_l2cap_chan		chan;
	/** Channel Receiving Endpoint */
	struct bt_l2cap_br_endpoint	rx;
	/** Channel Transmission Endpoint */
	struct bt_l2cap_br_endpoint	tx;
	/* For internal use only */
	atomic_t			flags[1];
};

/** @brief L2CAP Channel operations structure. */
struct bt_l2cap_chan_ops {
	/** @brief Channel connected callback
	 *
	 *  If this callback is provided it will be called whenever the
	 *  connection completes.
	 *
	 *  @param chan The channel that has been connected
	 */
	void (*connected)(struct bt_l2cap_chan *chan);

	/** @brief Channel disconnected callback
	 *
	 *  If this callback is provided it will be called whenever the
	 *  channel is disconnected, including when a connection gets
	 *  rejected.
	 *
	 *  @param chan The channel that has been Disconnected
	 */
	void (*disconnected)(struct bt_l2cap_chan *chan);

	/** @brief Channel encrypt_change callback
	 *
	 *  If this callback is provided it will be called whenever the
	 *  security level changed (indirectly link encryption done) or
	 *  authentication procedure fails. In both cases security initiator
	 *  and responder got the final status (HCI status) passed by
	 *  related to encryption and authentication events from local host's
	 *  controller.
	 *
	 *  @param chan The channel which has made encryption status changed.
	 *  @param status HCI status of performed security procedure caused
	 *  by channel security requirements. The value is populated
	 *  by HCI layer and set to 0 when success and to non-zero (reference to
	 *  HCI Error Codes) when security/authentication failed.
	 */
	void (*encrypt_change)(struct bt_l2cap_chan *chan, uint8_t hci_status);

	/** @brief Channel alloc_buf callback
	 *
	 *  If this callback is provided the channel will use it to allocate
	 *  buffers to store incoming data. Channels that requires segmentation
	 *  must set this callback.
	 *  If the application has not set a callback the L2CAP SDU MTU will be
	 *  truncated to @ref BT_L2CAP_SDU_RX_MTU.
	 *
	 *  @param chan The channel requesting a buffer.
	 *
	 *  @return Allocated buffer.
	 */
	struct net_buf *(*alloc_buf)(struct bt_l2cap_chan *chan);

	/** @brief Channel recv callback
	 *
	 *  @param chan The channel receiving data.
	 *  @param buf Buffer containing incoming data.
	 *
	 *  @return 0 in case of success or negative value in case of error.
	 *  @return -EINPROGRESS in case where user has to confirm once the data
	 *                       has been processed by calling
	 *                       @ref bt_l2cap_chan_recv_complete passing back
	 *                       the buffer received with its original user_data
	 *                       which contains the number of segments/credits
	 *                       used by the packet.
	 */
	int (*recv)(struct bt_l2cap_chan *chan, struct net_buf *buf);

	/** @brief Channel sent callback
	 *
	 *  If this callback is provided it will be called whenever a SDU has
	 *  been completely sent.
	 *
	 *  @param chan The channel which has sent data.
	 */
	void (*sent)(struct bt_l2cap_chan *chan);

	/** @brief Channel status callback
	 *
	 *  If this callback is provided it will be called whenever the
	 *  channel status changes.
	 *
	 *  @param chan The channel which status changed
	 *  @param status The channel status
	 */
	void (*status)(struct bt_l2cap_chan *chan, atomic_t *status);

	/* @brief Channel released callback
	 *
	 * If this callback is set it is called when the stack has release all
	 * references to the channel object.
	 */
	void (*released)(struct bt_l2cap_chan *chan);
};

/** @def BT_L2CAP_CHAN_SEND_RESERVE
 *  @brief Headroom needed for outgoing L2CAP PDUs.
 */
#define BT_L2CAP_CHAN_SEND_RESERVE (BT_L2CAP_BUF_SIZE(0))

/** @def BT_L2CAP_SDU_CHAN_SEND_RESERVE
 * @brief Headroom needed for outgoing L2CAP SDUs.
 */
#define BT_L2CAP_SDU_CHAN_SEND_RESERVE (BT_L2CAP_SDU_BUF_SIZE(0))

/** @brief L2CAP Server structure. */
struct bt_l2cap_server {
	/** @brief Server PSM.
	 *
	 *  Possible values:
	 *  0               A dynamic value will be auto-allocated when
	 *                  bt_l2cap_server_register() is called.
	 *
	 *  0x0001-0x007f   Standard, Bluetooth SIG-assigned fixed values.
	 *
	 *  0x0080-0x00ff   Dynamically allocated. May be pre-set by the
	 *                  application before server registration (not
	 *                  recommended however), or auto-allocated by the
	 *                  stack if the app gave 0 as the value.
	 */
	uint16_t			psm;

	/** Required minimum security level */
	bt_security_t		sec_level;

	/** @brief Server accept callback
	 *
	 *  This callback is called whenever a new incoming connection requires
	 *  authorization.
	 *
	 *  @param conn The connection that is requesting authorization
	 *  @param chan Pointer to received the allocated channel
	 *
	 *  @return 0 in case of success or negative value in case of error.
	 *  @return -ENOMEM if no available space for new channel.
	 *  @return -EACCES if application did not authorize the connection.
	 *  @return -EPERM if encryption key size is too short.
	 */
	int (*accept)(struct bt_conn *conn, struct bt_l2cap_chan **chan);

	sys_snode_t node;
};

/** @brief Register L2CAP server.
 *
 *  Register L2CAP server for a PSM, each new connection is authorized using
 *  the accept() callback which in case of success shall allocate the channel
 *  structure to be used by the new connection.
 *
 *  For fixed, SIG-assigned PSMs (in the range 0x0001-0x007f) the PSM should
 *  be assigned to server->psm before calling this API. For dynamic PSMs
 *  (in the range 0x0080-0x00ff) server->psm may be pre-set to a given value
 *  (this is however not recommended) or be left as 0, in which case upon
 *  return a newly allocated value will have been assigned to it. For
 *  dynamically allocated values the expectation is that it's exposed through
 *  a GATT service, and that's how L2CAP clients discover how to connect to
 *  the server.
 *
 *  @param server Server structure.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int bt_l2cap_server_register(struct bt_l2cap_server *server);

/** @brief Register L2CAP server on BR/EDR oriented connection.
 *
 *  Register L2CAP server for a PSM, each new connection is authorized using
 *  the accept() callback which in case of success shall allocate the channel
 *  structure to be used by the new connection.
 *
 *  @param server Server structure.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int bt_l2cap_br_server_register(struct bt_l2cap_server *server);

/** @brief Connect Enhanced Credit Based L2CAP channels
 *
 *  Connect up to 5 L2CAP channels by PSM, once the connection is completed
 *  each channel connected() callback will be called. If the connection is
 *  rejected disconnected() callback is called instead.
 *
 *  @param conn Connection object.
 *  @param chans Array of channel objects.
 *  @param psm Channel PSM to connect to.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int bt_l2cap_ecred_chan_connect(struct bt_conn *conn,
				struct bt_l2cap_chan **chans, uint16_t psm);

/** @brief Connect L2CAP channel
 *
 *  Connect L2CAP channel by PSM, once the connection is completed channel
 *  connected() callback will be called. If the connection is rejected
 *  disconnected() callback is called instead.
 *  Channel object passed (over an address of it) as second parameter shouldn't
 *  be instantiated in application as standalone. Instead of, application should
 *  create transport dedicated L2CAP objects, i.e. type of bt_l2cap_le_chan for
 *  LE and/or type of bt_l2cap_br_chan for BR/EDR. Then pass to this API
 *  the location (address) of bt_l2cap_chan type object which is a member
 *  of both transport dedicated objects.
 *
 *  @param conn Connection object.
 *  @param chan Channel object.
 *  @param psm Channel PSM to connect to.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int bt_l2cap_chan_connect(struct bt_conn *conn, struct bt_l2cap_chan *chan,
			  uint16_t psm);

/** @brief Disconnect L2CAP channel
 *
 *  Disconnect L2CAP channel, if the connection is pending it will be
 *  canceled and as a result the channel disconnected() callback is called.
 *  Regarding to input parameter, to get details see reference description
 *  to bt_l2cap_chan_connect() API above.
 *
 *  @param chan Channel object.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int bt_l2cap_chan_disconnect(struct bt_l2cap_chan *chan);

/** @brief Send data to L2CAP channel
 *
 *  Send data from buffer to the channel. If credits are not available, buf will
 *  be queued and sent as and when credits are received from peer.
 *  Regarding to first input parameter, to get details see reference description
 *  to bt_l2cap_chan_connect() API above.
 *
 *  When sending L2CAP data over an BR/EDR connection the application is sending
 *  L2CAP PDUs. The application is required to have reserved
 *  @ref BT_L2CAP_CHAN_SEND_RESERVE bytes in the buffer before sending.
 *  The application should use the @def BT_L2CAP_BUF_SIZE helper to correctly
 *  size the buffers for the for the outgoing buffer pool.
 *
 *  When sending L2CAP data over an LE connection the applicatios is sending
 *  L2CAP SDUs. The application can optionally reserve
 *  @ref BT_L2CAP_SDU_CHAN_SEND_RESERVE bytes in the buffer before sending.
 *  By reserving bytes in the buffer the stack can use this buffer as a segment
 *  directly, otherwise it will have to allocate a new segment for the first
 *  segment.
 *  If the application is reserving the bytes it should use the
 *  @def BT_L2CAP_BUF_SIZE helper to correctly size the buffers for the for the
 *  outgoing buffer pool.
 *  When segmenting an L2CAP SDU into L2CAP PDUs the stack will first attempt
 *  to allocate buffers from the original buffer pool of the L2CAP SDU before
 *  using the stacks own buffer pool.
 *
 *  @note Buffer ownership is transferred to the stack in case of success, in
 *  case of an error the caller retains the ownership of the buffer.
 *
 *  @return Bytes sent in case of success or negative value in case of error.
 */
int bt_l2cap_chan_send(struct bt_l2cap_chan *chan, struct net_buf *buf);

/** @brief Complete receiving L2CAP channel data
 *
 * Complete the reception of incoming data. This shall only be called if the
 * channel recv callback has returned -EINPROGRESS to process some incoming
 * data. The buffer shall contain the original user_data as that is used for
 * storing the credits/segments used by the packet.
 *
 * @param chan Channel object.
 * @param buf Buffer containing the data.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int bt_l2cap_chan_recv_complete(struct bt_l2cap_chan *chan,
				struct net_buf *buf);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_L2CAP_H_ */
