#include "broker.h"

extern main_ctx_t *MAIN_CTX;

static int CURR_POS;

/* get context, if fail new alloc for temp use */
msg_ctx_t *get_msg_ctx(void)
{
	msg_ctx_t *msg_ctx = NULL;

	/* find empty slot */
	for (int i = 0; i < MAX_MSGCTX_NUM; i++) {
		int pos = (CURR_POS + i) % MAX_MSGCTX_NUM;
		if (MAIN_CTX->msg_ctx[pos].occupied == 0) {
			msg_ctx = &MAIN_CTX->msg_ctx[pos];
			break;
		}
	}
	/* malloc if not found, this ctx must be free-ed after use */
	if (msg_ctx == NULL) {
		msg_ctx = malloc(sizeof(msg_ctx_t));
		msg_ctx->msg_ctx_id = -1; // say i'm temp ctx
		msg_ctx->buffer = calloc(1, MAX_MSGSIZE_BYTE); // todo, divide size
	}
	msg_ctx->occupied = 1;
	memset(&msg_ctx->head, 0x00, sizeof(envelop_t));

	// forward pos + 1
	CURR_POS = (msg_ctx->msg_ctx_id) + 1 % MAX_MSGCTX_NUM;

	return msg_ctx;
}

void free_msg_ctx(msg_ctx_t *msg_ctx)
{
	if (msg_ctx->msg_ctx_id >= 0) {
		/* buffer allocated */
		msg_ctx->occupied = 0;
	} else {
		/* temp allocated */
		free(msg_ctx->buffer);
		free(msg_ctx);
	}
}

void zsock_drain_remain_part(void *zsock)
{
	/* drain remain part of message */
	uint32_t events = 0; size_t len = sizeof(events);
	while (zmq_getsockopt(zsock, ZMQ_RCVMORE, &events, &len) == 0 && events > 0) {
		zmq_recv(zsock, NULL, 0, ZMQ_DONTWAIT);
	}
}

void err_msg_reply(const char *fn, msg_ctx_t *msg_ctx, int err_code, const char *format, ...)
{
	STAT_INC(stat, ST_STAT_FAIL);	/* stat fail +1 */
	STAT_INC(reason, err_code);		/* reason err_code +1 */

	msg_ctx->head.resp_code = err_code;

	va_list args;
	va_start(args, format);
	vsnprintf(msg_ctx->head.reason_string, ZMSG_COMM_STRLEN, format, args);
	va_end(args);

	//fprintf(stderr, "%s() err_code=(%d) reason={%s}\n", fn, err_code, msg_ctx->head.reason_string);

	event_base_once(MAIN_CTX->thrd_ctx[TR_SEND].evbase_thrd, -1, EV_TIMEOUT, evsock_router_err_reply, msg_ctx, NULL);
}

void evsock_pull_read(evutil_socket_t fd, short what, void *arg)
{
	zsock_ctx_t *zsock_ctx = (zsock_ctx_t *)arg;
	msg_ctx_t *msg_ctx = NULL;

	uint32_t events = 0; size_t len = sizeof(events);
	zmq_getsockopt(zsock_ctx->zsock, ZMQ_EVENTS, &events, &len);
	
	if (!(events & ZMQ_POLLIN)) {
		return; /* EOF, none of interest */
	}

EVSOCK_PULL_READ_TRY:
	/* prepare */
	zsock_drain_remain_part(zsock_ctx->zsock);
	msg_ctx = get_msg_ctx();

	if (zmq_recv(zsock_ctx->zsock, &msg_ctx->head, sizeof(envelop_t), ZMQ_DONTWAIT) < 0) {
		free_msg_ctx(msg_ctx);
		return; /* EOF, release context & stop processing */
	} else {
		STAT_INC(stat, ST_STAT_ATT); /* stat att +1 */
	}

	if (msg_ctx->head.type != ZMSG_TRAFFIC) {
		msg_ctx->head.n_bytes = 0;
		ERR_MSG_REPLY(ZMSG_RESP_FAIL_INVALID_TYPE, "recv invalid type=(%d)/valid_type=(%d)", msg_ctx->head.type, ZMSG_TRAFFIC);
		goto EVSOCK_PULL_READ_TRY;
	}

	if (zmq_getsockopt(zsock_ctx->zsock, ZMQ_RCVMORE, &events, &len) < 0 || events <= 0) {
		msg_ctx->head.n_bytes = 0;
		ERR_MSG_REPLY(ZMSG_RESP_FAIL_NULL_RECV, "%s", "recv null body");
		goto EVSOCK_PULL_READ_TRY;
	}

	int rcv_bytes = zmq_recv(zsock_ctx->zsock, msg_ctx->buffer, MAX_MSGSIZE_BYTE, ZMQ_DONTWAIT);
	if (rcv_bytes != msg_ctx->head.n_bytes) {
		msg_ctx->head.n_bytes = rcv_bytes;
		ERR_MSG_REPLY(ZMSG_RESP_FAIL_DIFF_RECV, "send=(%d)/recv=(%d) bytes mismatch", msg_ctx->head.n_bytes, rcv_bytes);
		goto EVSOCK_PULL_READ_TRY;
	}

	/* ok this message will replayed */
	event_base_once(MAIN_CTX->thrd_ctx[TR_SEND].evbase_thrd, -1, EV_TIMEOUT, evsock_push_relay, msg_ctx, NULL);

	/* check more to relay */
	goto EVSOCK_PULL_READ_TRY;
}
