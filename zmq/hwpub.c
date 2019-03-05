//  Hello World client
// gcc -std=c99 -o hwpaclient1.exe hwpaclient1.c -lczmq -lzmq


#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

int main (void)
{
    printf ("Connecting to hello world server...\n");
    void *context = zmq_ctx_new ();
    void *publisher = zmq_socket (context, ZMQ_PUB);
    // int rc = zmq_connect (publisher, "tcp://localhost:5555");
    int rc = zmq_connect (publisher, "tcp://localhost:5555");
    assert (rc==0);

    int request_nbr, bb;
    char *msg = "Z";
    for (request_nbr = 0; request_nbr != 10000000; request_nbr++) {
	uint32_t sndbuf[10];
	memset(sndbuf, 0, sizeof(sndbuf));
	sndbuf[0] = request_nbr;

#if(0)
        printf ("Sending Pub with message %d...\n", request_nbr);
        zmq_send (publisher, msg, sizeof(msg), ZMQ_SNDMORE);
        zmq_send (publisher, sndbuf, sizeof(sndbuf), 0);
#endif
#if(1)
        printf ("Sending Pub %d...\n", request_nbr);
	sndbuf[1] = 0xB0FF;
	sndbuf[2] = 0xDAD;
	sndbuf[3] = 0xDEAD;
        zmq_send (publisher, sndbuf, sizeof(sndbuf), 0);
#endif
	sleep(1);
    }
    zmq_close (publisher);
    zmq_ctx_destroy (context);
    return 0;
}
