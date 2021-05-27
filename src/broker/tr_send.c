#include "broker.h"

extern main_ctx_t *MAIN_CTX;

int msg_relay(msg_ctx_t *msg_ctx, zsock_ctx_t *zsock_ctx)
{
	int head_res = zmq_send(zsock_ctx->zsock, &msg_ctx->head, sizeof(msg_ctx->head), ZMQ_DONTWAIT|ZMQ_SNDMORE);
	if (head_res < 0) {
		fprintf(stderr, "%s() fail to send head err(%d:%s)\n", __func__, errno, strerror(errno));
		return -1;
	}
	int body_res = zmq_send(zsock_ctx->zsock, msg_ctx->buffer, msg_ctx->head.n_bytes, ZMQ_DONTWAIT);
	if (body_res < 0) {
		fprintf(stderr, "%s() fail to send body err(%d:%s)\n", __func__, errno, strerror(errno));
		return -1;
	}

	return 0;
}

int service_relay(msg_ctx_t *msg_ctx, zsock_ctx_t *zsock_ctx)
{
	return msg_relay(msg_ctx, zsock_ctx);
}

int router_relay(msg_ctx_t *msg_ctx, zsock_ctx_t *zsock_ctx, const char *uuid)
{
	/* router uuid */
	int route_res = zmq_send(zsock_ctx->zsock, uuid, strlen(uuid), ZMQ_DONTWAIT|ZMQ_SNDMORE);
	if (route_res < 0) {
		fprintf(stderr, "%s() fail to route=[%s] err(%d:%s)\n", __func__, uuid, errno, strerror(errno));
		return -1;
	}
	return msg_relay(msg_ctx, zsock_ctx);
}

void evsock_router_err_reply(evutil_socket_t fd, short what, void *arg)
{
	msg_ctx_t *msg_ctx = (msg_ctx_t *)arg;
	thrd_ctx_t *thrd_ctx = &MAIN_CTX->thrd_ctx[TR_SEND];
	element_t *elem = get_element(thrd_ctx->zsock_llist, "ROUTER");
	if (elem == NULL) {
		fprintf(stderr, "%s() fatal / fail to get \"ROUTER\" zsock from list!\n", __func__);
		return;
	}
	zsock_ctx_t *zsock_ctx = (zsock_ctx_t *)elem->data;

	if (router_relay(msg_ctx, zsock_ctx, msg_ctx->head.from) < 0) {
		fprintf(stderr, "%s() fail to relay error reply to uuid=[%s]\n", __func__, msg_ctx->head.from);
	}

	return free_msg_ctx(msg_ctx);
}

void evsock_push_relay(evutil_socket_t fd, short what, void *arg)
{
	msg_ctx_t *msg_ctx = (msg_ctx_t *)arg;
	envelop_t *head = &msg_ctx->head;

	if (head->route != ZMSG_ROUTE_SVC && head->route != ZMSG_ROUTE_INST) {
		return ERR_MSG_REPLY(ZMSG_RESP_INVLD_ROUTE_TYPE, "unknown route_type=(%d)", head->route);
	}

	thrd_ctx_t *thrd_ctx = &MAIN_CTX->thrd_ctx[TR_SEND];
	element_t *elem = get_element(thrd_ctx->zsock_llist, head->route == ZMSG_ROUTE_SVC ? head->dest : "ROUTER");
	if (elem == NULL) {
		return ERR_MSG_REPLY(ZMSG_RESP_FAIL_TO_ROUTE, "fail to route type=(%d) dest=[%s]", head->route, head->dest);
	}
	zsock_ctx_t *zsock_ctx = (zsock_ctx_t *)elem->data;

	int relay_res = head->route == ZMSG_ROUTE_SVC ? 
		service_relay(msg_ctx, zsock_ctx) : router_relay(msg_ctx, zsock_ctx, head->dest);
	if (relay_res < 0) {
		return ERR_MSG_REPLY(ZMSG_RESP_FAIL_TO_ROUTE, "fail to route type=(%d) dest=[%s]", head->route, head->dest);
	} else {
		STAT_INC(stat, ST_STAT_SUCC);	/* stat succ + 1 */
		STAT_INC(reason, ZMSG_RESP_OK); /* reason succ + 1 */
	}

	return free_msg_ctx(msg_ctx);
}

void evsock_push_create(evutil_socket_t fd, short what, void *arg)
{
	appls_ctx_t *appls_ctx = (appls_ctx_t *)arg;
	thrd_ctx_t *thrd_ctx = &MAIN_CTX->thrd_ctx[TR_SEND];

	fprintf(stderr, "create new push sock name=[%s]\n", appls_ctx->svcname);
	add_zsock_to_list(thrd_ctx, appls_ctx->svcname, ZMQ_PUSH);

	free(appls_ctx);
}

void evsock_push_remove(evutil_socket_t fd, short what, void *arg)
{
	appls_ctx_t *appls_ctx = (appls_ctx_t *)arg;
	thrd_ctx_t *thrd_ctx = &MAIN_CTX->thrd_ctx[TR_SEND];

	fprintf(stderr, "remove dead push sock name=[%s]\n", appls_ctx->svcname);
	del_zsock_from_list(thrd_ctx, appls_ctx->svcname, ZMQ_PUSH);

	free(appls_ctx);
}
