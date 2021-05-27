#include "broker.h"

extern main_ctx_t *MAIN_CTX;

void find_svc_exist(element_t *elem, arg_svc_exist_t *arg)
{
	appls_ctx_t *appls_ctx = (appls_ctx_t *)elem->data;
	if (elem->key == NULL || appls_ctx == NULL) { /* root node */
		return;
	}
	if (!strcmp(appls_ctx->svcname, arg->svcname)) {
		arg->find += 1;
	}
}

void check_svc_alive(element_t *elem, void *appls_list)
{
	thrd_ctx_t *thrd_ctx = &MAIN_CTX->thrd_ctx[TR_MGNT];
	appls_ctx_t *appls_ctx = (appls_ctx_t *)elem->data;
	time_t now = time(NULL);

	if (elem->key == NULL || appls_ctx == NULL) { /* root node */
		return;
	}
	if ((now - appls_ctx->timestamp) > 10) {
		arg_svc_exist_t arg = { .svcname = appls_ctx->svcname, .find = 0 };
		if (strlen(appls_ctx->svcname) > 0) {
			list_element(thrd_ctx->appls_llist, find_svc_exist, &arg);
		}
		if (arg.find == 1) {
			fprintf(stderr, "notify delete svc=[%s]!\n", appls_ctx->svcname); 
			/* I'm the last one, notify to send thread */
			appls_ctx_t *rem_appls = malloc(sizeof(appls_ctx_t));
			memcpy(rem_appls, appls_ctx, sizeof(appls_ctx_t));
			event_base_once(MAIN_CTX->thrd_ctx[TR_SEND].evbase_thrd, -1, EV_TIMEOUT, evsock_push_remove, rem_appls, NULL);
		}
		fprintf(stderr, "remove elem uuid=[%s]!\n", appls_ctx->uuid);
		rem_element(elem);
	}
}

void handle_regi(int n_bytes, appls_ctx_t *appls_ctx)
{
	if (n_bytes != sizeof(appls_ctx_t)) {
		fprintf(stderr, "%s() recv wrong size=(%d)!\n", __func__, n_bytes);
		return;
	}
	appls_ctx->timestamp = time(NULL); // save last recv time

	thrd_ctx_t *thrd_ctx = &MAIN_CTX->thrd_ctx[TR_MGNT];
	element_t *elem = get_element(thrd_ctx->appls_llist, appls_ctx->uuid);
	if (elem != NULL) {
		/* update & return */
		memcpy((appls_ctx_t *)elem->data, appls_ctx, sizeof(appls_ctx_t));
		fprintf(stderr, "{dbg} uuid=[%s] snd[%ld:%.3fMB] rcv[%ld:%.3fMB]\n", 
				appls_ctx->uuid, 
				appls_ctx->snd_cnt, 
				(float)appls_ctx->snd_bytes / 1024 / 1024, 
				appls_ctx->rcv_cnt, 
				(float)appls_ctx->rcv_bytes / 1024 / 1024);
		return;
	} else {
		fprintf(stderr, "add new elem uuid=[%s]!\n", appls_ctx->uuid);
		/* create new to my list */
		elem = new_element(appls_ctx->uuid, appls_ctx, sizeof(appls_ctx_t));
		add_element(thrd_ctx->appls_llist, elem);
	}

	/* check svc already exist */
	arg_svc_exist_t arg = { .svcname = appls_ctx->svcname, .find = 0 };
	if (strlen(appls_ctx->svcname) > 0) {
		list_element(thrd_ctx->appls_llist, find_svc_exist, &arg);
	}
	if (arg.find == 1) { // it's me
		fprintf(stderr, "notify create svc=[%s]!\n", appls_ctx->svcname); 
		/* I'm new, notify to send thread */
		appls_ctx_t *new_appls = malloc(sizeof(appls_ctx_t));
		memcpy(new_appls, appls_ctx, sizeof(appls_ctx_t));
		event_base_once(MAIN_CTX->thrd_ctx[TR_SEND].evbase_thrd, -1, EV_TIMEOUT, evsock_push_create, new_appls, NULL);
	}
}

void manage_appls(evutil_socket_t fd, short what, void *arg)
{
	thrd_ctx_t *thrd_ctx = &MAIN_CTX->thrd_ctx[TR_MGNT];
	list_element(thrd_ctx->appls_llist, check_svc_alive, NULL);
}

void evsock_sub_read(evutil_socket_t fd, short what, void *arg)
{
    zsock_ctx_t *zsock_ctx = (zsock_ctx_t *)arg;

    uint32_t events = 0; size_t len = sizeof(events);
    zmq_getsockopt(zsock_ctx->zsock, ZMQ_EVENTS, &events, &len);

    if (!(events & ZMQ_POLLIN)) {
        return; /* EOF, none of interest */
    }

EVSOCK_SUB_READ_TRY:
	/* prepare */
	zsock_drain_remain_part(zsock_ctx->zsock);
	zmessage_t msg_buffer, *zmsg = &msg_buffer;

	if (zmq_recv(zsock_ctx->zsock, &zmsg->head, sizeof(envelop_t), ZMQ_DONTWAIT) < 0) {
		return; /* no more recv */
	}

	if (zmq_getsockopt(zsock_ctx->zsock, ZMQ_RCVMORE, &events, &len) < 0 || events <= 0) {
		zmsg->head.n_bytes = 0;
		fprintf(stderr, "%s() recv null body, will discard!\n", __func__);
		goto EVSOCK_SUB_READ_TRY;
	}

	int rcv_bytes = zmq_recv(zsock_ctx->zsock, zmsg->body, MAX_MSGSIZE_BYTE, ZMQ_DONTWAIT);
	if (rcv_bytes != zmsg->head.n_bytes) {
		fprintf(stderr, "%s() send=(%d)/recv=(%d) bytes mismatch, will discard!", __func__, zmsg->head.n_bytes, rcv_bytes);
		goto EVSOCK_SUB_READ_TRY;
	}

	switch (zmsg->head.type) {
		case ZMSG_REGI:
			handle_regi(zmsg->head.n_bytes, (appls_ctx_t *)zmsg->body);
			break;
		case ZMSG_CONF:
			break;
		case ZMSG_LOG:
			break;
		default:
			fprintf(stderr, "%s() recv invalid type=[%d] will discard!\n", __func__, zmsg->head.type);
			break;
	}

	goto EVSOCK_SUB_READ_TRY;
}
