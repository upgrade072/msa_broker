#include "broker.h"

main_ctx_t MainCtx;
main_ctx_t *MAIN_CTX = &MainCtx;

int initialize_main(main_ctx_t *MAIN_CTX)
{
	/* create base */
	MAIN_CTX->zmq_ctx = zmq_ctx_new();
	MAIN_CTX->evbase_main = event_base_new();

	/* check (if non, create) ipc path directory */
	MAIN_CTX->iv_home = getenv("IV_HOME");
	sprintf(MAIN_CTX->ipc_path, "%s/data/ipc_path", MAIN_CTX->iv_home == NULL ? getenv("HOME") : MAIN_CTX->iv_home);
	fprintf(stderr, "ipc_path=[%s]\n", MAIN_CTX->ipc_path);

	DIR *ipc_path = opendir(MAIN_CTX->ipc_path);
	if (ipc_path) {
		fprintf(stderr, "%s() check ipc_path=[%s] is already exist\n", __func__, MAIN_CTX->ipc_path);
		closedir(ipc_path);
	} else {
		char sys_cmd[1024] = {0,};
		sprintf(sys_cmd, "mkdir -p %s\n", MAIN_CTX->ipc_path);
		fprintf(stderr, "%s() will create unexist ipc_path=[%s]\n", __func__, MAIN_CTX->ipc_path);
		system(sys_cmd);
	}

	// create huge buffer, todo will config
	fprintf(stderr, "will prepare msgctx num=[%d] each size=[%ld]\n", MAX_MSGCTX_NUM, sizeof(msg_ctx_t) + MAX_MSGSIZE_BYTE);
	MAIN_CTX->msg_ctx = calloc(MAX_MSGCTX_NUM, sizeof(msg_ctx_t));
	for (int i = 0; i < MAX_MSGCTX_NUM; i++) {
		msg_ctx_t *msg_ctx = &MAIN_CTX->msg_ctx[i];
		msg_ctx->msg_ctx_id = i;
		msg_ctx->buffer = calloc(1, MAX_MSGSIZE_BYTE);
		if (i % 100 == 0) fprintf(stderr, ".");
	}
	fprintf(stderr, "\n");

	// create atomic stat counter
	fprintf(stderr, "will prepare stat counter\n");
	for (int i = 0; i < ST_STAT_MAX; i++) {
		MAIN_CTX->stat[i] = zmq_atomic_counter_new();
	}
	for (int k = 0; k < ZMSG_RESP_MAX; k++) {
		MAIN_CTX->reason[k] = zmq_atomic_counter_new();
	}

	// assign zmq I/O setting, todo move to config
	if (zmq_ctx_set(MAIN_CTX->zmq_ctx, ZMQ_IO_THREADS, ZMQ_IO_THREAD_MAX) < 0) {
		fprintf(stderr, "%s() fail to zmq_ctx_set(ZMQ_IO_THREADS, %d)!\n", __func__, ZMQ_IO_THREAD_MAX);
		return -1;
	}

	// create main zmq socket, will use in later, todo will config
	MAIN_CTX->zsock_pair = zmq_socket(MAIN_CTX->zmq_ctx, ZMQ_PAIR);

	// create & start threads
	// Caution! must follow strict order. send -> recv -> mgnt creation
	char thread_name[][16] = { "worker/tr_send", "worker/tr_recv", "worker/tr_mgnt" };
	for (int i = 0; i < TR_NUM; i++) {
		thrd_ctx_t *thrd_ctx = &MAIN_CTX->thrd_ctx[i];
		thrd_ctx->thrd_role = i;
		sprintf(thrd_ctx->thrd_name, "%s", thread_name[i]);
		if (pthread_create(&thrd_ctx->thrd_pid, NULL, io_thread, thrd_ctx) < 0) {
			fprintf(stderr, "%s() fail to create io_thread=[%s]!\n", __func__, thread_name[i]);
			return -1;
		}
		pthread_setname_np(thrd_ctx->thrd_pid, thrd_ctx->thrd_name);
		sleep(1);
	}

	return 0;
}

void add_zsock_to_list(thrd_ctx_t *thrd_ctx, const char *key, int type)
{
	if (get_element(thrd_ctx->zsock_llist, key) != NULL) {
		fprintf(stderr, "%s() add=[%s] fail, already exist!\n", __func__, key);
		return;
	}

	zsock_ctx_t zsock_ctx;
	memset(&zsock_ctx, 0x00, sizeof(zsock_ctx_t));

	sprintf(zsock_ctx.my_key, key);
	zsock_ctx.type = type;
	sprintf(zsock_ctx.ipc_path, "ipc://%s/%s.ipc", MAIN_CTX->ipc_path, key);

	element_t *elem = new_element(zsock_ctx.my_key, &zsock_ctx, sizeof(zsock_ctx_t));
	add_element(thrd_ctx->zsock_llist, elem);
	zsock_ctx_t *zsock_created = (zsock_ctx_t *)elem->data;
	zsock_created->zsock = zmq_socket(MAIN_CTX->zmq_ctx, zsock_created->type);

	{ /* set sock opt */
		if (type == ZMQ_ROUTER) { /* return immediately when routing faied */
			int mandatory = 1;
			zmq_setsockopt(zsock_created->zsock, ZMQ_ROUTER_MANDATORY, &mandatory, sizeof(int));
		}
		if (type == ZMQ_SUB) { /* subscribe all of events */
			zmq_setsockopt(zsock_created->zsock, ZMQ_SUBSCRIBE, "", 0);
		}
		if (1) { /* send reset when finished */
			int linger = 0;
			zmq_setsockopt (zsock_created->zsock, ZMQ_LINGER, &linger, sizeof(linger));
		}
	}

	zmq_bind(zsock_created->zsock, zsock_created->ipc_path);

	// todo set sockopt, rst, max msg size, max queue num
	fprintf(stderr, "create zsock=[%s] at thread=[%s] with ipc_path=[%s]\n", zsock_created->my_key, thrd_ctx->thrd_name, zsock_created->ipc_path);
}

void del_zsock_from_list(thrd_ctx_t *thrd_ctx, const char *key, int type)
{
	element_t *elem = get_element(thrd_ctx->zsock_llist, key);
	if (elem == NULL) {
		fprintf(stderr, "%s() del=[%s] fail, not exist!\n", __func__, key);
		return;
	}

	zsock_ctx_t *zsock_ctx = (zsock_ctx_t *)elem->data;
	if (zsock_ctx->type != type) {
		fprintf(stderr, "%s() del=[%s] fail, type mismatch (%d:%d)!\n", __func__, key, zsock_ctx->type, type);
		return;
	}

	zmq_close(zsock_ctx->zsock);
	rem_element(elem);
}

void add_zsock_rd_event(thrd_ctx_t *thrd_ctx, const char *key, const char *ev_name, void (*cb_func)(evutil_socket_t, short, void *))
{
	element_t *elem = get_element(thrd_ctx->zsock_llist, key);
	if (elem == NULL) {
		fprintf(stderr, "%s() fail to get zsock[%s] from zsock_list!\n", __func__, key);
		return;
	}
	zsock_ctx_t *zsock_ctx = (zsock_ctx_t *)elem->data;
	int zfd = 0; size_t opt_len = sizeof(zsock_ctx->zsock);
	if (zmq_getsockopt(zsock_ctx->zsock, ZMQ_FD, &zfd, &opt_len) < 0) {
		fprintf(stderr,"%s() fail to zmq_getsockopt(ZMQ_FD) fail to zsock(%s)!\n", __func__, key);
		return;
	}

	if (get_element(thrd_ctx->event_llist, ev_name) != NULL) {
		fprintf(stderr, "%s() add=[%s] fail, already exist!\n", __func__, ev_name);
		return;
	}

	event_ctx_t evt_ctx;
	memset(&evt_ctx, 0x00, sizeof(event_ctx_t));

	sprintf(evt_ctx.my_key, ev_name);
	elem = new_element(evt_ctx.my_key, &evt_ctx, sizeof(event_ctx_t));
	add_element(thrd_ctx->event_llist, elem);
	event_ctx_t *evt_created = (event_ctx_t *)elem->data;
	evt_created->ev = event_new(thrd_ctx->evbase_thrd, zfd, EV_READ | EV_PERSIST, cb_func, zsock_ctx);
	event_add(evt_created->ev, NULL);

	fprintf(stderr, "create event=[%s] at sock=[%s] with fd=(%d)\n", ev_name, key, zfd);
}

void add_event_to_list(thrd_ctx_t *thrd_ctx, const char *ev_name, void (*cb_func)(evutil_socket_t, short, void *), void *arg, int tm_ms)
{
	if (get_element(thrd_ctx->event_llist, ev_name) != NULL) {
		fprintf(stderr, "%s() add=[%s] fail, already exist!\n", __func__, ev_name);
		return;
	}

	event_ctx_t evt_ctx;
	memset(&evt_ctx, 0x00, sizeof(event_ctx_t));

	sprintf(evt_ctx.my_key, ev_name);
	element_t *elem = new_element(evt_ctx.my_key, &evt_ctx, sizeof(event_ctx_t));
	add_element(thrd_ctx->event_llist, elem);
	event_ctx_t *evt_created = (event_ctx_t *)elem->data;

	evt_created->ev = event_new(thrd_ctx->evbase_thrd, -1, EV_PERSIST, cb_func, arg);
	struct timeval tm_intval = { .tv_sec = tm_ms / 1000, .tv_usec = (tm_ms % 1000) * 1000 };
	event_add(evt_created->ev, &tm_intval);

	fprintf(stderr, "create event=[%s] at thread=[%s] tm_intvl=(%d)ms\n", 
			ev_name, thrd_ctx->thrd_name, tm_ms);
}

int initialize_thread(thrd_ctx_t *thrd_ctx)
{
	thrd_ctx->zsock_llist = new_element(NULL, NULL, 0);
	thrd_ctx->event_llist = new_element(NULL, NULL, 0);
	thrd_ctx->appls_llist = new_element(NULL, NULL, 0);

	/* create socks possible */
	switch (thrd_ctx->thrd_role) {
		case TR_SEND:
			add_zsock_to_list(thrd_ctx, "ROUTER", ZMQ_ROUTER);
			break;
		case TR_RECV:
			add_zsock_to_list(thrd_ctx, "PULL", ZMQ_PULL);
			add_zsock_rd_event(thrd_ctx, "PULL", "pull_reader", evsock_pull_read);
			break;
		case TR_MGNT:
			add_zsock_to_list(thrd_ctx, "PUB", ZMQ_PUB);
			add_zsock_to_list(thrd_ctx, "SUB", ZMQ_SUB);
			add_zsock_rd_event(thrd_ctx, "SUB", "sub_reader", evsock_sub_read);
			add_event_to_list(thrd_ctx, "manage_appls", manage_appls, thrd_ctx, 1000);
			break;
		default:
			fprintf(stderr, "%s() recv unknown thrd_role=(%d)!\n", __func__, thrd_ctx->thrd_role);
			exit(0);
	}

	/* assign event */

	return 0;
}

void *io_thread(void *arg)
{
	thrd_ctx_t *thrd_ctx = (thrd_ctx_t *)arg;
	thrd_ctx->evbase_thrd = event_base_new();

	fprintf(stderr, "\n>>> thread=[%s] created <<<\n", thrd_ctx->thrd_name);

	if (initialize_thread(thrd_ctx) < 0) {
		fprintf(stderr, ">>> thread=[%s] fail to initialize()!\n", thrd_ctx->thrd_name);
		exit(0);
	}
	event_base_loop(thrd_ctx->evbase_thrd, EVLOOP_NO_EXIT_ON_EMPTY);

	return NULL;
}

void tick_stat()
{
	time_t now = time(NULL); 
   	char tm_buff[128] = {0,};
	ctime_r(&now, tm_buff); 

	/* print stat */
	char stat_str[][128] = { "att", "succ", "fail" };
	char reason_str[][128] = { "ok", "type_err", "null_recv", "size_err", "route_err", "fail_route" };
	char *ptr = NULL; size_t ptr_size = 0;
	FILE *fp_buff= open_memstream(&ptr, &ptr_size);

	for (int i = 0; i < ST_STAT_MAX; i++) {
		fprintf(fp_buff, "[%s]=(%d) ", stat_str[i], STAT_GET(stat, i)); 
		STAT_UNSET(stat,i);
	}
	fprintf(fp_buff, "| ");
	for (int k = 0; k < ZMSG_RESP_MAX; k++) {
		fprintf(fp_buff, "[%s]=(%d) ", reason_str[k], STAT_GET(reason, k)); 
		STAT_UNSET(reason,k);
	}
	fclose(fp_buff);
	fprintf(stderr, "%.19s %s\n", tm_buff, ptr);
	free(ptr);
}

void main_tick(evutil_socket_t fd, short what, void *arg)
{
	tick_stat();
}

int main()
{
	evthread_use_pthreads();

	if (initialize_main(MAIN_CTX) < 0) {
		fprintf(stderr, "%s() fail to initialize_main()!\n", __func__);
		exit(0);
	}

	/* tick event */
	struct timeval one_sec = {1, 0};
	struct event *ev_tick = event_new(MAIN_CTX->evbase_main, -1, EV_PERSIST, main_tick, NULL);
	event_add(ev_tick, &one_sec);

	event_base_loop(MAIN_CTX->evbase_main, EVLOOP_NO_EXIT_ON_EMPTY);
}
