#ifndef _ZUTIL_H
#define _ZUTIL_H 1

#include <zhelpers.h>
#include <zmessage.h>

typedef struct zsocks_t {
	void *zsock_pub;
	void *zsock_sub;
	void *zsock_push;
	void *zsock_dealer;
	void *zsock_pull;
} zsocks_t;

typedef struct zutil_ctx_t {
	void *zmq_ctx;
	char *iv_home;
	zsocks_t zsocks;
	zregimsg_t zregi;
	zmessage_t zmsg;
} zutil_ctx_t;

extern zutil_ctx_t *ZCTX;

/* ------------------------- zutil.c --------------------------- */
void    zutil_regi();
int     zutil_init(const char *svcname, const char *appname, int io_cnt /* default 1 */);
int     zutil_send(int route, char *dest, char *buff, size_t size, char *stamp);
int     zutil_recv(envelop_t *head, char *buff, size_t size);
void    zutil_close_all();

#endif /* zutil.h */


