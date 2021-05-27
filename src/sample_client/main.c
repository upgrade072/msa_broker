#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <zutil.h>

int main (void)
{
	zutil_init("test_1.0", "client", 2);
	char *test_data = malloc(4096);

	int count = 0;
	while (1) {
		count++;
		if (count > 1000) {
			usleep(10);
		}
		zutil_regi();
		//zutil_send(ZMSG_ROUTE_SVC, "test_1.0", "hello", strlen("hello"), NULL);
		zutil_send(ZMSG_ROUTE_SVC, "test_1.0", test_data, 4096, NULL);
		envelop_t head = {0,};
		char buffer[1024] = {0,};
		while(zutil_recv(&head, buffer, 1024) >= 0) {
		}
	}
}
