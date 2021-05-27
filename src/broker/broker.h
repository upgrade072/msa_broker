#ifndef _BROKER_H
#define _BROKER_H 1

#include <zhelpers.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <event.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/thread.h>

#include <elem.h>
#include "zmessage.h"

#define ERR_MSG_REPLY(...)	err_msg_reply(__func__, msg_ctx, __VA_ARGS__)
#define STAT_INC(a,b)		zmq_atomic_counter_inc(MAIN_CTX->a[b])
#define STAT_GET(a,b)		zmq_atomic_counter_value(MAIN_CTX->a[b])
#define STAT_UNSET(a,b)		zmq_atomic_counter_set(MAIN_CTX->a[b], 0)

typedef enum thrd_enum_t {
	TR_SEND = 0,
	TR_RECV,
	TR_MGNT,
	TR_NUM
} thrd_enum_t;

typedef struct thrd_ctx_t {
	thrd_enum_t thrd_role;
	char thrd_name[16];
	pthread_t thrd_pid;

	struct event_base *evbase_thrd;
	void *zsock_llist;	// zsock_ctx linked list
	void *event_llist;	// event_ctx linked list
	void *appls_llist;	// appls_ctx linked list
} thrd_ctx_t;

#define MAX_MSGCTX_NUM 10240 
#define MAX_MSGSIZE_BYTE 1024
typedef struct msg_ctx_t {
	int msg_ctx_id;
	int occupied;

	/* zmessage.h */
	envelop_t head;
	char *buffer;
} msg_ctx_t;

typedef enum stat_enum_t {
	ST_STAT_ATT = 0,
	ST_STAT_SUCC,
	ST_STAT_FAIL,
	ST_STAT_MAX
} stat_enum_t;

typedef struct main_ctx_t {
	const char *iv_home;
	char ipc_path[1024];

	void *zmq_ctx;
	void *zsock_pair;
	struct event_base *evbase_main;
	thrd_ctx_t thrd_ctx[TR_NUM];

	msg_ctx_t *msg_ctx;	//pull (occupied:1) -> [push|router] == success ? (occupied:0) : router (occupied:0)

	void *stat[ST_STAT_MAX];
	void *reason[ZMSG_RESP_MAX];
} main_ctx_t;

typedef struct event_ctx_t {
	char my_key[1024];
	struct event *ev;
} event_ctx_t;

typedef struct zsock_ctx_t {
	char my_key[1024];
	int type;
	char ipc_path[1024];
	void *zsock;
} zsock_ctx_t;

typedef struct arg_svc_exist_t {
	char *svcname;
	int find;
} arg_svc_exist_t;

/* ------------------------- main.c --------------------------- */
int     initialize_main(main_ctx_t *MAIN_CTX);
void    add_zsock_to_list(thrd_ctx_t *thrd_ctx, const char *key, int type);
void    del_zsock_from_list(thrd_ctx_t *thrd_ctx, const char *key, int type);
void    add_zsock_rd_event(thrd_ctx_t *thrd_ctx, const char *key, const char *ev_name, void (*cb_func)(evutil_socket_t, short, void *));
void    add_event_to_list(thrd_ctx_t *thrd_ctx, const char *ev_name, void (*cb_func)(evutil_socket_t, short, void *), void *arg, int tm_ms);
int     initialize_thread(thrd_ctx_t *thrd_ctx);
void    *io_thread(void *arg);
int     main();

/* ------------------------- tr_mgnt.c --------------------------- */
void    find_svc_exist(element_t *elem, arg_svc_exist_t *arg);
void    check_svc_alive(element_t *elem, void *appls_list);
void    handle_regi(int n_bytes, appls_ctx_t *appls_ctx);
void    manage_appls(evutil_socket_t fd, short what, void *arg);
void    evsock_sub_read(evutil_socket_t fd, short what, void *arg);

/* ------------------------- tr_recv.c --------------------------- */
msg_ctx_t       *get_msg_ctx(void);
void    free_msg_ctx(msg_ctx_t *msg_ctx);
void    zsock_drain_remain_part(void *zsock);
void    err_msg_reply(const char *fn, msg_ctx_t *msg_ctx, int err_code, const char *format, ...);
void    evsock_pull_read(evutil_socket_t fd, short what, void *arg);

/* ------------------------- tr_send.c --------------------------- */
int     msg_relay(msg_ctx_t *msg_ctx, zsock_ctx_t *zsock_ctx);
int     service_relay(msg_ctx_t *msg_ctx, zsock_ctx_t *zsock_ctx);
int     router_relay(msg_ctx_t *msg_ctx, zsock_ctx_t *zsock_ctx, const char *uuid);
void    evsock_router_err_reply(evutil_socket_t fd, short what, void *arg);
void    evsock_push_relay(evutil_socket_t fd, short what, void *arg);
void    evsock_push_create(evutil_socket_t fd, short what, void *arg);
void    evsock_push_remove(evutil_socket_t fd, short what, void *arg);

#endif	/* broker.h */
