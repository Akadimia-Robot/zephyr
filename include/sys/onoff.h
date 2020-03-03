/*
 * Copyright (c) 2019 Peter Bigot Consulting, LLC
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_SYS_ONOFF_H_
#define ZEPHYR_INCLUDE_SYS_ONOFF_H_

#include <kernel.h>
#include <zephyr/types.h>
#include <sys/notify.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup resource_mgmt_onoff_apis On-Off Service APIs
 * @ingroup kernel_apis
 * @{
 */

/**
 * @brief Flag used in struct onoff_manager_transitions.
 *
 * When provided this indicates the start transition function
 * may cause the calling thread to wait.  This blocks attempts
 * to initiate a transition from a non-thread context.
 */
#define ONOFF_START_SLEEPS BIT(0)

/**
 * @brief Flag used in struct onoff_manager_transitions.
 *
 * As with @ref ONOFF_START_SLEEPS but describing the stop
 * transition function.
 */
#define ONOFF_STOP_SLEEPS BIT(1)

/**
 * @brief Flag used in struct onoff_manager_transitions.
 *
 * As with @ref ONOFF_START_SLEEPS but describing the reset
 * transition function.
 */
#define ONOFF_RESET_SLEEPS BIT(2)

/**
 * @brief Flag indicating an error state.
 *
 * Error states are cleared using onoff_reset().
 */
#define ONOFF_HAS_ERROR BIT(3)

/** @internal */
#define ONOFF_FLAG_ONOFF BIT(4)
/** @internal */
#define ONOFF_FLAG_TRANSITION BIT(5)

/**
 * @brief Mask used to isolate bits defining the service state.
 *
 * Mask a value with this then test for ONOFF_HAS_ERROR to determine
 * whether the machine has an unfixed error, or compare against
 * ONOFF_STATE_ON, ONOFF_STATE_OFF, ONOFF_STATE_TO_ON, or
 * ONOFF_STATE_TO_OFF.
 */
#define ONOFF_STATE_MASK (ONOFF_HAS_ERROR \
			  | ONOFF_FLAG_TRANSITION | ONOFF_FLAG_ONOFF)

/**
 * @brief Value exposed by ONOFF_STATE_MASK when service is off.
 */
#define ONOFF_STATE_OFF 0

/**
 * @brief Value exposed by ONOFF_STATE_MASK when service is on.
 */
#define ONOFF_STATE_ON ONOFF_FLAG_ONOFF

/**
 * @brief Value exposed by ONOFF_STATE_MASK when service is
 * transitioning to on.
 */
#define ONOFF_STATE_TO_ON (ONOFF_FLAG_TRANSITION | ONOFF_STATE_ON)

/**
 * @brief Value exposed by ONOFF_STATE_MASK when service is
 * transitioning to off.
 */
#define ONOFF_STATE_TO_OFF (ONOFF_FLAG_TRANSITION | ONOFF_STATE_OFF)

#define ONOFF_SERVICE_START_SLEEPS __DEPRECATED_MACRO ONOFF_START_SLEEPS
#define ONOFF_SERVICE_STOP_SLEEPS __DEPRECATED_MACRO ONOFF_STOP_SLEEPS
#define ONOFF_SERVICE_RESET_SLEEPS __DEPRECATED_MACRO ONOFF_RESET_SLEEPS
#define ONOFF_SERVICE_HAS_ERROR __DEPRECATED_MACRO ONOFF_HAS_ERROR
#define ONOFF_SERVICE_INTERNAL_BASE __DEPRECATED_MACRO ONOFF_INTERNAL_BASE

/* Forward declarations */
struct onoff_manager;
struct onoff_monitor;
struct onoff_notifier;

/**
 * @brief Signature used to notify an on-off manager that a transition
 * has completed.
 *
 * Functions of this type are passed to service-specific transition
 * functions to be used to report the completion of the operation.
 * The functions may be invoked from any context.
 *
 * @param mgr the manager for which transition was requested.
 *
 * @param res the result of the transition.  This shall be
 * non-negative on success, or a negative error code.  If an error is
 * indicated the service shall enter an error state.
 */
typedef void (*onoff_notify_fn)(struct onoff_manager *mgr,
				int res);

/**
 * @brief Signature used by service implementations to effect a
 * transition.
 *
 * Service definitions use two function pointers of this type to be
 * notified that a transition is required, and a third optional one to
 * reset service state.
 *
 * The start function will be called only from the off state.
 *
 * The stop function will be called only from the on state.
 *
 * The reset function may be called only when onoff_has_error()
 * returns true.
 *
 * @param mgr the manager for which transition was requested.
 *
 * @param notify the function to be invoked when the transition has
 * completed.  The callee shall capture this parameter to notify on
 * completion of asynchronous transitions.  If the transition is not
 * asynchronous, notify shall be invoked before the transition
 * function returns.
 */
typedef void (*onoff_transition_fn)(struct onoff_manager *mgr,
				    onoff_notify_fn notify);

/** @brief On-off service transition functions. */
struct onoff_transitions {
	/* Function to invoke to transition the service to on. */
	onoff_transition_fn start;

	/* Function to invoke to transition the service to off. */
	onoff_transition_fn stop;

	/* Function to force the service state to reset, where supported. */
	onoff_transition_fn reset;

	/* Flags identifying transition function capabilities. */
	u8_t flags;
};

/**
 * @brief State associated with an on-off manager.
 *
 * No fields in this structure are intended for use by service
 * providers or clients.  The state is to be initialized once, using
 * onoff_manager_init(), when the service provider is initialized.  In
 * case of error it may be reset through the onoff_reset() API.
 */
struct onoff_manager {
	/* List of clients waiting for transition or reset completion
	 * notifications.
	 */
	sys_slist_t clients;

	/* List of monitors to be notified of state changes including
	 * errors and transition completion.
	 */
	sys_slist_t monitors;

	/* Transition functions. */
	const struct onoff_transitions *transitions;

	/* Mutex protection for flags, clients, monitors, and refs. */
	struct k_spinlock lock;

	/* Flags identifying the service state. */
	u16_t flags;

	/* Number of active clients for the service. */
	u16_t refs;
};

/** @brief Initializer for a onoff_transitions object.
 *
 * @param _start a function used to transition from off to on state.
 *
 * @param _stop a function used to transition from on to off state.
 *
 * @param _reset a function used to clear errors and force the service to an off
 * state. Can be null.
 *
 * @param _flags any or all of the flags from enum onoff_manager_flags.
 */
#define ONOFF_TRANSITIONS_INITIALIZER(_start, _stop, _reset, _flags) { \
		.start = _start,				       \
		.stop = _stop,					       \
		.reset = _reset,				       \
		.flags = _flags,				       \
}

#define ONOFF_SERVICE_TRANSITIONS_INITIALIZER(_start, _stop, _reset, _flags) \
	__DEPRECATED_MACRO						     \
	ONOFF_TRANSISTIONS_INITIALIZER(_start, _stop, _reset, _flags)


/** @internal */
#define ONOFF_MANAGER_INITIALIZER(_transitions) { \
		.transitions = _transitions,	  \
		.flags = (_transitions)->flags,	  \
}

#define ONOFF_SERVICE_INITIALIZER(_transitions)	\
	__DEPRECATED_MACRO			\
	ONOFF_MANAGER_INITIALIZER(_transitions)

/**
 * @brief Initialize an on-off service to off state.
 *
 * This function must be invoked exactly once per service instance, by
 * the infrastructure that provides the service, and before any other
 * on-off service API is invoked on the service.
 *
 * This function should never be invoked by clients of an on-off service.
 *
 * @param mgr the manager definition object to be initialized.
 *
 * @param transitions A structure with transition functions. Structure must be
 * persistent as it is used by the service.
 *
 * @retval 0 on success
 * @retval -EINVAL if start, stop, or flags are invalid
 */
int onoff_manager_init(struct onoff_manager *mgr,
		       const struct onoff_transitions *transitions);

/* Forward declaration */
struct onoff_client;

/**
 * @brief Signature used to notify an on-off service client of the
 * completion of an operation.
 *
 * These functions may be invoked from any context including
 * pre-kernel, ISR, or cooperative or pre-emptible threads.
 * Compatible functions must be isr-callable and non-suspendable.
 *
 * @param mgr the manager for which the operation was initiated.
 *
 * @param cli the client structure passed to the function that
 * initiated the operation.
 *
 * @param state the state of the machine at the time of completion,
 * restricted by ONOFF_STATE_MASK.  ONOFF_HAS_ERROR must be checked
 * independently of whether res is negative as a machine error may
 * indicate that all future operations except onoff_reset() will fail.
 *
 * @param res the result of the operation.  Expected values are
 * service-specific, but the value shall be non-negative if the
 * operation succeeded, and negative if the operation failed.  If res
 * is negative ONOFF_HAS_ERROR will be set in state, but if res is
 * non-negative ONOFF_HAS_ERROR may still be set in state.
 *
 * @param user_data user data provided when the client structure was
 * initialized with onoff_client_init_callback().
 */
typedef void (*onoff_client_callback)(struct onoff_manager *mgr,
				      struct onoff_client *cli,
				      u32_t state,
				      int res,
				      void *user_data);

/**
 * @brief State associated with a client of an on-off service.
 *
 * Objects of this type are allocated by a client, which must use an
 * initialization function (e.g. onoff_client_init_signal()) to
 * configure them.
 *
 * Control of the object content transfers to the service provider
 * when a pointer to the object is passed to any on-off service
 * function.  While the service provider controls the object the
 * client must not change any object fields.  Control reverts to the
 * client concurrent with release of the owned sys_notify structure,
 * or when indicated by an onoff_cancel() return value.
 *
 * After control has reverted to the client the state object must be
 * reinitialized for the next operation.
 *
 * The content of this structure is not public API: all configuration
 * and inspection should be done with functions like
 * onoff_client_init_callback() and onoff_client_fetch_result().
 */
struct onoff_client {
	/* Links the client into the set of waiting service users. */
	sys_snode_t node;

	/* Notification configuration. */
	struct sys_notify notify;

	/* User data for callback-based notification. */
	void *user_data;
};

/** @internal */
#define ONOFF_CLIENT_TYPE_POS SYS_NOTIFY_EXTENSION_POS
/** @internal */
#define ONOFF_CLIENT_TYPE_BITS 2U
/**
 * @brief Identify region of sys_notify flags available for
 * containing services.
 *
 * Bits of the flags field of the sys_notify structure contained
 * within the queued_operation structure at and above this position
 * may be used by extensions to the onoff_client structure.
 *
 * These bits are intended for use by containing service
 * implementations to record client-specific information.  Use of
 * these does not imply that the flags field becomes public API.
 */
#define ONOFF_CLIENT_EXTENSION_POS (SYS_NOTIFY_EXTENSION_POS \
				    + ONOFF_CLIENT_TYPE_BITS)

/**
 * @brief Check for and read the result of an asynchronous operation.
 *
 * @param op pointer to the object used to specify asynchronous
 * function behavior and store completion information.
 *
 * @param result pointer to storage for the result of the operation.
 * The result is stored only if the operation has completed.
 *
 * @retval 0 if the operation has completed.
 * @retval -EAGAIN if the operation has not completed.
 */
static inline int onoff_client_fetch_result(const struct onoff_client *op,
					    int *result)
{
	__ASSERT_NO_MSG(op != NULL);

	return sys_notify_fetch_result(&op->notify, result);
}

/**
 * @brief Initialize an on-off client to be used for a spin-wait
 * operation notification.
 *
 * Clients that use this initialization receive no asynchronous
 * notification, and instead must periodically check for completion
 * using onoff_client_fetch_result().
 *
 * On completion of the operation the client object must be
 * reinitialized before it can be re-used.
 *
 * @param cli pointer to the client state object.
 */
static inline void onoff_client_init_spinwait(struct onoff_client *cli)
{
	__ASSERT_NO_MSG(cli != NULL);

	*cli = (struct onoff_client){};
	sys_notify_init_spinwait(&cli->notify);
}

/**
 * @brief Initialize an on-off client to be used for a signal
 * operation notification.
 *
 * Clients that use this initialization will be notified of the
 * completion of operations submitted through onoff_request() and
 * onoff_release() through the provided signal.
 *
 * On completion of the operation the client object must be
 * reinitialized before it can be re-used.
 *
 * @note
 *   @rst
 *   This capability is available only when :option:`CONFIG_POLL` is
 *   selected.
 *   @endrst
 *
 * @param cli pointer to the client state object.
 *
 * @param sigp pointer to the signal to use for notification.  The
 * value must not be null.  The signal must be reset before the client
 * object is passed to the on-off service API.
 */
static inline void onoff_client_init_signal(struct onoff_client *cli,
					    struct k_poll_signal *sigp)
{
	__ASSERT_NO_MSG(cli != NULL);

	*cli = (struct onoff_client){};
	sys_notify_init_signal(&cli->notify, sigp);
}

/**
 * @brief Initialize an on-off client to be used for a callback
 * operation notification.
 *
 * Clients that use this initialization will be notified of the
 * completion of operations submitted through on-off service API
 * through the provided callback.  Note that callbacks may be invoked
 * from various contexts depending on the specific service; see
 * @ref onoff_client_callback.
 *
 * On completion of the operation the client object must be
 * reinitialized before it can be re-used.
 *
 * @param cli pointer to the client state object.
 *
 * @param handler a function pointer to use for notification.
 *
 * @param user_data an opaque pointer passed to the handler to provide
 * additional context.
 */
static inline void onoff_client_init_callback(struct onoff_client *cli,
					      onoff_client_callback handler,
					      void *user_data)
{
	__ASSERT_NO_MSG(cli != NULL);
	__ASSERT_NO_MSG(handler != NULL);

	*cli = (struct onoff_client){
		.user_data = user_data,
	};
	sys_notify_init_callback(&cli->notify, handler);
}

/**
 * @brief Request a reservation to use an on-off service.
 *
 * The return value indicates the success or failure of an attempt to
 * initiate an operation to request the resource be made available.
 * If initiation of the operation succeeds the result of the request
 * operation is provided through the configured client notification
 * method, possibly before this call returns.
 *
 * Note that the call to this function may succeed in a case where the
 * actual request fails.  Always check the operation completion
 * result.
 *
 * As a specific example: A call to this function may succeed at a
 * point while the service is still transitioning to off due to a
 * previous call to onoff_release().  When the transition completes
 * the service would normally start a transition to on.  However, if
 * the transition to off completed in a non-thread context, and the
 * transition to on can sleep, the transition cannot be started and
 * the request will fail with `-EWOULDBLOCK`.
 *
 * @param mgr the manager that will be used.
 *
 * @param cli a non-null pointer to client state providing
 * instructions on synchronous expectations and how to notify the
 * client when the request completes.  Behavior is undefined if client
 * passes a pointer object associated with an incomplete service
 * operation.
 *
 * @retval Non-negative on successful (initiation of) request
 * @retval -EIO if service has recorded an an error
 * @retval -EINVAL if the parameters are invalid
 * @retval -EAGAIN if the reference count would overflow
 * @retval -EWOULDBLOCK if the function was invoked from non-thread
 * context and successful initiation could result in an attempt to
 * make the calling thread sleep.
 */
int onoff_request(struct onoff_manager *mgr,
		  struct onoff_client *cli);

/**
 * @brief Release a reserved use of an on-off service.
 *
 * The return value indicates the success or failure of an attempt to
 * initiate an operation to release the resource.  If initiation of
 * the operation succeeds the result of the release operation itself
 * is provided through the configured client notification method,
 * possibly before this call returns.
 *
 * Note that the call to this function may succeed in a case where the
 * actual release fails.  Always check the operation completion
 * result.
 *
 * @param mgr the manager that will be used.
 *
 * @param cli a non-null pointer to client state providing
 * instructions on how to notify the client when release completes.
 * Behavior is undefined if cli references an object associated with
 * an incomplete service operation.
 *
 * @retval Non-negative on successful (initiation of) release
 * @retval -EINVAL if the parameters are invalid
 * @retval -EIO if service has recorded an an error
 * @retval -EWOULDBLOCK if a non-blocking request was made and
 *         could not be satisfied without potentially blocking.
 * @retval -EALREADY if the service is already off or transitioning
 *         to off
 * @retval -EBUSY if the service is transitioning to on
 */
int onoff_release(struct onoff_manager *mgr,
		  struct onoff_client *cli);

/**
 * @brief Test whether an on-off service has recorded an error.
 *
 * This function can be used to determine whether the service has
 * recorded an error.  Errors may be cleared by invoking
 * onoff_reset().
 *
 * This is an unlocked convenience function suitable for use only when
 * it is known that no other process might invoke an operation that
 * transitions the service between an error and non-error state.
 *
 * @return true if and only if the service has an uncleared error.
 */
static inline bool onoff_has_error(const struct onoff_manager *mgr)
{
	return (mgr->flags & ONOFF_HAS_ERROR) != 0;
}

/**
 * @brief Clear errors on an on-off service and reset it to its off
 * state.
 *
 * A service can only be reset when it is in an error state as
 * indicated by onoff_has_error().
 *
 * The return value indicates the success or failure of an attempt to
 * initiate an operation to reset the resource.  If initiation of the
 * operation succeeds the result of the reset operation itself is
 * provided through the configured client notification method,
 * possibly before this call returns.  Multiple clients may request a
 * reset; all are notified when it is complete.
 *
 * Note that the call to this function may succeed in a case where the
 * actual reset fails.  Always check the operation completion result.
 *
 * @note Due to the conditions on state transition all incomplete
 * asynchronous operations will have been informed of the error when
 * it occurred.  There need be no concern about dangling requests left
 * after a reset completes.
 *
 * @param mgr the manager to be reset.
 *
 * @param cli pointer to client state, including instructions on how
 * to notify the client when reset completes.  Behavior is undefined
 * if cli references an object associated with an incomplete service
 * operation.
 *
 * @retval 0 on success
 * @retval -ENOTSUP if reset is not supported by the service.
 * @retval -EINVAL if the parameters are invalid.
 * @retval -EALREADY if the service does not have a recorded error.
 */
int onoff_reset(struct onoff_manager *mgr,
		struct onoff_client *cli);

/**
 * @brief Attempt to cancel an in-progress client operation.
 *
 * It may be that a client has initiated an operation but needs to
 * shut down before the operation has completed.  For example, when a
 * request was made and the need is no longer present.
 *
 * In-progress transitions on behalf of a specific client can be
 * cancelled, with the following behavior:
 *
 * * Clients with an incomplete request or reset can immediately
 *   cancel it regardless of whether a start, stop, or reset is in
 *   progress.
 * * Clients with an incomplete release can cancel it, and the client
 *   structure will be converted to a request operation that will
 *   force a transition to on as soon as the transition to off
 *   completes.
 *
 *   @warning It is possible for the synthesized request to complete
 *   notification before execution returns to the caller informing it
 *   that the release was converted to a request.  Consequently
 *   clients that choose to cancel release operations must take care
 *   to inspect either the state parameter to a callback notifier, or
 *   to use the onoff_monitor infrastructure to be informed
 *   synchronously with each state change, to correctly associate the
 *   completion result with a request or release operation.
 *
 * Be aware that any transition that was initiated on behalf of the
 * client will continue to progress to completion: it is only
 * notification of transition completion that may be eliminated.  If
 * there are no active requests when a transition to on completes the
 * manager will initiate a transition to off.  If there are pending
 * requests when a transition to off completes the manager will
 * initiate a transition to on.
 *
 * The onoff_notifier infrastructure provides a wrapper API for
 * anonymous clients to register requests and releases without
 * directly maintaining state related to in-progress transitions.
 * This can be used to work around the complexities that result from
 * trying to cancel an in-progress transition.
 *
 * @param mgr the manager for which an operation is to be cancelled.
 *
 * @param cli a pointer to the same client state that was provided
 * when the operation to be cancelled was issued.
 *
 * @retval 0 if the cancellation was completed before the client could
 * be notified.  The cancellation succeeds and control of the cli
 * structure returns to the client without any completion notification.
 * @retval 1 in the case where a pending release was cancelled before
 * the service transitioned to off.  In this case the manager retains
 * control of the client structure, which is converted to a request,
 * and completion will be notified when the manager completes a
 * transition back to on.
 * @retval -EINVAL if the parameters are invalid.
 * @retval -EALREADY if cli was not a record of an uncompleted
 * notification at the time the cancellation was processed.  This
 * likely indicates that the operation and client notification had
 * already completed.
 */
int onoff_cancel(struct onoff_manager *mgr,
		 struct onoff_client *cli);

/**
 * @brief Signature used to notify a monitor of an onoff service of
 * errors or completion of a state transition.
 *
 * This is similar to onoff_client_callback but provides information
 * about all transitions, not just ones associated with a specific
 * client.  Monitor callbacks are invoked before any completion
 * notifications associated with the state change are made.
 *
 * These functions may be invoked from any context including
 * pre-kernel, ISR, or cooperative or pre-emptible threads.
 * Compatible functions must be isr-callable and non-suspendable.
 *
 * The callback is permitted to unregister itself from the manager,
 * but must not register or unregister any other monitors.
 *
 * @param mgr the manager for which a transition has completed.
 *
 * @param mon the monitor instance through which this notification
 * arrived.
 *
 * @param state the state of the machine at the time of completion,
 * restricted by ONOFF_STATE_MASK.  This includes the ONOFF_HAS_ERROR
 * flag as well as all four non-error state: ONOFF_STATE_OFF,
 * ONOFF_STATE_TO_ON, ONOFF_STATE_ON, ONOFF_STATE_TO_OFF.
 *
 * @param res the result of the operation.  Expected values are
 * service- and state-specific, but the value shall be non-negative if
 * the operation succeeded, and negative if the operation failed.
 */
typedef void (*onoff_monitor_callback)(struct onoff_manager *mgr,
				       struct onoff_monitor *mon,
				       u32_t state,
				       int res);

/**
 * @brief Registration state for notifications of onoff service
 * transitions.
 *
 * Any given onoff_monitor structure can be associated with at most
 * one onoff_manager instance.
 */
struct onoff_monitor {
	/* Links the client into the set of waiting service users. */
	sys_snode_t node;

	/* Callback to be invoked on state change. */
	onoff_monitor_callback callback;
};

/**
 * @brief Add a monitor of state changes for a manager.
 *
 * @param mgr the manager for which a state changes are to be monitored.
 *
 * @param mon a linkable node providing the callback to be invoked on
 * state changes.
 *
 * @return non-negative on successful addition, or a negative error
 * code.
 */
int onoff_monitor_register(struct onoff_manager *mgr,
			   struct onoff_monitor *mon);

/**
 * @brief Remove a monitor of state changes from a manager.
 *
 * @param mgr the manager for which a state changes are to be monitored.
 *
 * @param mon a linkable node providing the callback to be invoked on
 * state changes.
 *
 * @return non-negative on successful addition, or a negative error
 * code.
 */
int onoff_monitor_unregister(struct onoff_manager *mgr,
			     struct onoff_monitor *mon);

/**
 * @brief Callback for an event-based API for onoff request/release
 * completions.
 *
 * Transition to off (for the client) is indicated by a zero value.
 * Transition to on (for the client) is indicated by a positive value.
 * A negative value indicates an error in the onoff service state.
 *
 * If the client issues requests or releases before transitions are
 * incomplete the notifier will be invoked only when the final target
 * state is reached with no changes pending.  In this case a series of
 * notifications may repeat the previous notification value, rather
 * than alternating between on and off.
 *
 * @param np pointer to the state object that manages on/off
 * transitions for a client that prefers event-based notifications
 * rather than separate request/release operations.
 *
 * @param status the status of the service at the time of the
 * callback: 1 if it's on, 0 if it's not on (including transitioning
 * to or from on).  A negative status value is passed if the
 * underlying service has indicated an error; in this case
 * onoff_notifier_reset() may need to be invoked to restore
 * functionality.
 */
typedef void (*onoff_notifier_callback)(struct onoff_notifier *np,
					int status);

/**
 * @brief Convert an onoff manager async API to an event-based API.
 *
 * In some use cases it's inconvenient for a client of an onoff
 * service to manage request and release states itself.  An example is
 * a case where the client no longer needs the service and wants to
 * shut down without waiting for the stop to complete.
 *
 * An onoff notifier wraps an onoff service, providing state-free
 * request and release methods and holding a callback that is invoked
 * whenever the state change for the individual client has completed.
 *
 * Note that this does not track the state of the service as a whole:
 * only the state of the client-specific requests and releases.
 * Specifically, while notification of a transition to on confirms
 * that the underlying service is on, notification of a transition to
 * off does not indicate the state of the underlying service, only
 * that this client no longer has a demand for the service.
 */
struct onoff_notifier {
	/* Pointer to the underlying onoff service. */
	struct onoff_manager *onoff;

	/* Callback use to notify of transition to stable state. */
	onoff_notifier_callback callback;

	/* Protects changes to state. */
	struct k_spinlock lock;

	/* Client structure used to communicate with onoff service. */
	struct onoff_client cli;

	/* Internal state of the notifier. */
	u32_t volatile state;

	/* The completion value for the last onoff operation. */
	int onoff_result;
};

/** @brief Initializer for a onoff_notifier object.
 *
 * @param _onoff a pointer to the onoff_manager to use for requests
 * and releases
 *
 * @param _callback the onoff_notifier_callback function pointer to be
 * invoked to inform the client of state changes.
 */
#define ONOFF_NOTIFIER_INITIALIZER(_onoff, _callback) {	\
		.onoff = _onoff,			\
		.callback = _callback,			\
}							\

/**
 * @brief Inform an onoff service that this notifier client needs the
 * service.
 *
 * If the underlying service is already on a positive value is
 * returned, and the notifier callback is not invoked.  If zero is
 * returned the notifier's callback will be invoked as soon as the
 * service completes a transition to on, which may be before the call
 * to this function returns.
 *
 * @param np pointer to the notifier state.
 *
 * @retval 0 if the request was successful but the service is not yet on.
 * @retval positive if the request was successful and the service is stable on.
 * @retval -EIO if the notifier is in an error state.
 * @retval -EALREADY if the client already in a state heading toward on.
 * @retval -EWOULDBLOCK if the client is in the process of resetting
 * from an error state.
 * @retval other negative values indicate an error from the onoff manager.
 */
int onoff_notifier_request(struct onoff_notifier *np);

/**
 * @brief Inform the onoff service that this notifier client no longer
 * needs the service.
 *
 * If the underlying service is already off a positive value is
 * returned.  If zero is returned the notifier's callback will be
 * invoked as soon as this client's request for the service to turn
 * off completes, which may be before the call to this function
 * returns.
 *
 * @param np pointer to the notifier state.
 *
 * @retval 0 if the request was successful.
 * @retval -EIO if the notifier is in an error state
 * @retval -EALREADY if the client is already in a state heading toward off.
 * @retval -EWOULDBLOCK if the client is in the process of resetting
 * from an error state.
 * @retval other negative values indicate an error from the onoff manager.
 */
int onoff_notifier_release(struct onoff_notifier *np);

/**
 * @brief Clear an error state from an onoff notifier.
 *
 * This function should be invoked if the notifier has been passed a
 * negative error code.  On successful invocation the notifier
 * callback will be invoked to indicate reaching the stable off state,
 * or with an error indicating why the reset could not be performed.
 *
 * @retval 0 if the request was successful.
 * @retval -EALREADY if the notifier does not have an error state
 * recorded.  The notifier callback will not be invoked.
 */
int onoff_notifier_reset(struct onoff_notifier *np);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SYS_ONOFF_H_ */
