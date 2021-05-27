#include <zutil.h>

zutil_ctx_t zctx, *ZCTX = &zctx;

static int zutil_init_create_context(int io_cnt)
{
	if ((ZCTX->iv_home = getenv("IV_HOME")) == NULL) {
		fprintf(stderr, "%s() fail to find env(IV_HOME)!\n", __func__);
		return -1;
	}
	if (io_cnt < 1 || io_cnt > ZMQ_IO_THREAD_MAX) {
		fprintf(stderr, "%s() recv invalid io_cnt[%d] (valid: 1 ~ %d)!\n", __func__, io_cnt, ZMQ_IO_THREAD_MAX);
		return -1;
	}
	if ((ZCTX->zmq_ctx = zmq_ctx_new()) == NULL) {
		fprintf(stderr, "%s() fail to create zmq_ctx!\n", __func__);
		return -1;
	}
	if (zmq_ctx_set(ZCTX->zmq_ctx, ZMQ_IO_THREADS, io_cnt) < 0) {
		fprintf(stderr, "%s() fail to set [ZMQ_IO_THREADS]!\n", __func__);
		return -1;
	}
	return 0;
}

static int zutil_init_create_regi_info(const char *svcname, const char *appname)
{
	appls_ctx_t *regi = &ZCTX->zregi.body;
	memset(regi, 0x00, sizeof(appls_ctx_t));

	/* create regi message */
	FILE *fp_sysuuid = fopen("/proc/sys/kernel/random/uuid", "r");
	if (fp_sysuuid == NULL) {
		fprintf(stderr, "%s() fail to read uuid from sys!\n", __func__);
		return -1;
	} else {
		fgets(regi->uuid, ZMSG_COMM_STRLEN, fp_sysuuid);
		regi->uuid[strcspn(regi->uuid, "\n")] = 0;
		fclose(fp_sysuuid);
	}
	snprintf(regi->svcname, ZMSG_COMM_STRLEN, svcname);
	snprintf(regi->appname, ZMSG_COMM_STRLEN, appname);
	regi->timestamp = 0;

	return 0;
}

static int zutil_init_create_zconnects(const char *svcname)
{
	zsocks_t *zsocks = &ZCTX->zsocks;
	memset(zsocks, 0x00, sizeof(zsocks_t));

	const char *uuid = ZCTX->zregi.body.uuid;
	int linger = 0;

	char root_path[1024] = {0,};
	char full_path[1024] = {0,};
	sprintf(root_path, "ipc://%s/data/ipc_path", ZCTX->iv_home);

	/* pub */
	sprintf(full_path , "%s/SUB.ipc", root_path);
	fprintf(stderr, "connect pub=[%s]\n", full_path);
	zsocks->zsock_pub = zmq_socket(ZCTX->zmq_ctx, ZMQ_PUB);
	zmq_setsockopt(zsocks->zsock_pub, ZMQ_LINGER, &linger, sizeof(linger));
	zmq_connect(zsocks->zsock_pub, full_path);

	/* sub */
	sprintf(full_path , "%s/PUB.ipc", root_path);
	fprintf(stderr, "connect sub=[%s]\n", full_path);
	zsocks->zsock_sub = zmq_socket(ZCTX->zmq_ctx, ZMQ_SUB);
	zmq_setsockopt(zsocks->zsock_sub, ZMQ_LINGER, &linger, sizeof(linger));
	zmq_connect(zsocks->zsock_sub, full_path);

	/* push */
	sprintf(full_path , "%s/PULL.ipc", root_path);
	fprintf(stderr, "connect push=[%s]\n", full_path);
	zsocks->zsock_push = zmq_socket(ZCTX->zmq_ctx, ZMQ_PUSH);
	zmq_setsockopt(zsocks->zsock_push, ZMQ_LINGER, &linger, sizeof(linger));
	zmq_connect(zsocks->zsock_push, full_path);

	/* dealer */
	sprintf(full_path , "%s/ROUTER.ipc", root_path);
	fprintf(stderr, "connect dealer=[%s]\n", full_path);
	zsocks->zsock_dealer = zmq_socket(ZCTX->zmq_ctx, ZMQ_DEALER);
	zmq_setsockopt(zsocks->zsock_dealer, ZMQ_LINGER, &linger, sizeof(linger));
	zmq_setsockopt(zsocks->zsock_dealer, ZMQ_ROUTING_ID, uuid, strlen(uuid));
	zmq_connect(zsocks->zsock_dealer, full_path);

	/* pull */
	sprintf(full_path , "%s/%s.ipc", root_path, svcname);
	fprintf(stderr, "connect pull=[%s]\n", full_path);
	zsocks->zsock_pull = zmq_socket(ZCTX->zmq_ctx, ZMQ_PULL);
	zmq_setsockopt(zsocks->zsock_pull, ZMQ_LINGER, &linger, sizeof(linger));
	zmq_connect(zsocks->zsock_pull, full_path);

	return 0;
}

void zutil_regi()
{
	zregimsg_t  *regi_msg = &ZCTX->zregi;
	envelop_t	*regi_head = &regi_msg->head;
	appls_ctx_t *regi_body = &regi_msg->body;
	zsocks_t	*zsocks = &ZCTX->zsocks;

	time_t now = time(NULL);
	if (now - regi_body->timestamp < 1) {
		return; /* not yet */
	} else {
		regi_body->timestamp = now;
	}

	memset(regi_head, 0x00, sizeof(envelop_t));
	regi_head->type = ZMSG_REGI;

	char tm_buff[128] = {0,};
	ctime_r(&regi_body->timestamp, tm_buff);
	sprintf(regi_head->stamp, "%.19s", tm_buff);

	regi_head->n_bytes = sizeof(appls_ctx_t);

	if (zmq_send(zsocks->zsock_pub, regi_head, sizeof(envelop_t), ZMQ_DONTWAIT|ZMQ_SNDMORE) > 0) {
		zmq_send(zsocks->zsock_pub, regi_body, sizeof(appls_ctx_t), ZMQ_DONTWAIT);
	}
	regi_body->snd_cnt = 0;
	regi_body->snd_bytes = 0;
	regi_body->rcv_cnt = 0;
	regi_body->rcv_bytes = 0;
}


int zutil_init(const char *svcname, const char *appname, int io_cnt /* default 1 */)
{
	if (zutil_init_create_context  (io_cnt)				< 0 || 
		zutil_init_create_regi_info(svcname, appname)	< 0 ||
		zutil_init_create_zconnects(svcname)			< 0 ) {
		return -1;
	} else {
		return 0;
	}
}

int zutil_send(int route, char *dest, char *buff, size_t size, char *stamp)
{
	zregimsg_t  *regi_msg = &ZCTX->zregi;
	appls_ctx_t *regi_body = &regi_msg->body;
	zsocks_t	*zsocks = &ZCTX->zsocks;

	if (route != ZMSG_ROUTE_SVC && route != ZMSG_ROUTE_INST) {
		return -1;
	}
	if (size <= 0 || size >= MAX_ZMQMSG_SIZE) {
		return -1;
	}
	envelop_t head = { .type = ZMSG_TRAFFIC, .route = route };
	snprintf(head.stamp, ZMSG_COMM_STRLEN, "%s", stamp != NULL ? stamp : "");
	snprintf(head.from, ZMSG_COMM_STRLEN, "%s", ZCTX->zregi.body.uuid);
	snprintf(head.dest, ZMSG_COMM_STRLEN, "%s", dest);
	head.n_bytes = size;
	head.resp_code = 0;
	head.reason_string[0] = '\0';

	int snd_bytes = -1;
	if (zmq_send(zsocks->zsock_push, &head, sizeof(envelop_t), ZMQ_DONTWAIT|ZMQ_SNDMORE) > 0) {
		snd_bytes = zmq_send(zsocks->zsock_push, buff, size, ZMQ_DONTWAIT);
	}
	if (snd_bytes >= 0) {
		regi_body->snd_cnt ++;
		regi_body->snd_bytes += snd_bytes;
	}
	return snd_bytes;
}

int zutil_recv(envelop_t *head, char *buff, size_t size)
{
	zregimsg_t  *regi_msg = &ZCTX->zregi;
	appls_ctx_t *regi_body = &regi_msg->body;
	zsocks_t	*zsocks = &ZCTX->zsocks;
	uint32_t events = 0; size_t len = sizeof(events);

	zmq_pollitem_t poll_items[] = {
		{ zsocks->zsock_sub,    -1, ZMQ_POLLIN, 0}, /* config change */
		{ zsocks->zsock_dealer, -1, ZMQ_POLLIN, 0}, /* (error) reply */
		{ zsocks->zsock_pull,   -1, ZMQ_POLLIN, 0}, /* service traffic */
	};
	if (zmq_poll(poll_items, 3, 0) <= 0) {
		return -1;
	}
	for (int i = 0; i < 3; i++) {
		if (poll_items[i].revents & ZMQ_POLLIN) {
			int rcv_bytes = -1;
			if (zmq_recv(poll_items[i].socket, head, sizeof(envelop_t), ZMQ_DONTWAIT) > 0) {
				if (zmq_getsockopt(poll_items[i].socket, ZMQ_RCVMORE, &events, &len) == 0 && events > 0) {
					rcv_bytes = zmq_recv(poll_items[i].socket, buff, size, ZMQ_DONTWAIT);
				}
			}
			if (rcv_bytes >= 0) {
				regi_body->rcv_cnt ++;
				regi_body->rcv_bytes += rcv_bytes;
				buff[rcv_bytes] = '\0';
			}
			return rcv_bytes;
		}
	}
	return -1;
}

void zutil_close_all()
{
	zsocks_t	*zsocks = &ZCTX->zsocks;
    zmq_close(zsocks->zsock_pub);
    zmq_close(zsocks->zsock_sub);
    zmq_close(zsocks->zsock_push);
    zmq_close(zsocks->zsock_dealer);
    zmq_close(zsocks->zsock_pull);
	zmq_ctx_destroy(ZCTX->zmq_ctx);
}
