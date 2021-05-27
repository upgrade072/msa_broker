#ifndef _ZMESSAGE_H
#define _ZMESSAGE_H 1

#define ZMQ_IO_THREAD_MAX 12

typedef enum zmsg_type_t {
	ZMSG_REGI = 0,
	ZMSG_TRAFFIC,
	ZMSG_CONF,
	ZMSG_LOG,
	ZMSG_MAX
} zmsg_type_t;

typedef enum zmsg_route_t {
	ZMSG_ROUTE_SVC = 0,
	ZMSG_ROUTE_INST,
	ZMSG_ROUTE_MAX
} zmsg_route_t;

typedef enum zmsg_resp_t {
	ZMSG_RESP_OK = 0,
	ZMSG_RESP_FAIL_INVALID_TYPE,
	ZMSG_RESP_FAIL_NULL_RECV,
	ZMSG_RESP_FAIL_DIFF_RECV,
	ZMSG_RESP_INVLD_ROUTE_TYPE,
	ZMSG_RESP_FAIL_TO_ROUTE,
	ZMSG_RESP_MAX
} zmsg_resp_t;

#define ZMSG_COMM_STRLEN 128
typedef struct envelop_t {
	zmsg_type_t type;
	zmsg_route_t route;

	char stamp[ZMSG_COMM_STRLEN];
	char from[ZMSG_COMM_STRLEN];
	char dest[ZMSG_COMM_STRLEN];
	int n_bytes;

	zmsg_resp_t resp_code;
	char reason_string[ZMSG_COMM_STRLEN];
} envelop_t;

typedef struct appls_ctx_t {
	char uuid[ZMSG_COMM_STRLEN];
	char svcname[ZMSG_COMM_STRLEN];
	char appname[ZMSG_COMM_STRLEN];
	size_t snd_cnt;
	size_t rcv_cnt;
	size_t snd_bytes;
	size_t rcv_bytes;
	time_t timestamp;
} appls_ctx_t;

#define MAX_ZMQMSG_SIZE 65535
typedef struct zmessage_t {
	envelop_t head;
	char body[MAX_ZMQMSG_SIZE];
} zmessage_t;

typedef struct zregimsg_t {
	envelop_t head;
	appls_ctx_t body;
} zregimsg_t;

#endif /* zmessage.h */
