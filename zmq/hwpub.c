// 
// gcc -std=c99 -o hwpub.exe hwpub.c -lczmq -lzmq

#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int main (void)
{
    printf ("Connecting to server...\n");
    void *context = zmq_ctx_new ();
    void *publisher = zmq_socket (context, ZMQ_PUB);
    zmq_connect (publisher, "tcp://localhost:5557");

    int request_nbr, bb;
    for (request_nbr = 0; request_nbr != 10000000; request_nbr++) {
	uint32_t sndbuf[10];
	sndbuf[0] = request_nbr;
        printf ("Sending Pub %d...\n", request_nbr);
        zmq_send (publisher, sndbuf, sizeof(sndbuf), 0);
	sleep(1);
    }
    zmq_close (publisher);
    zmq_ctx_destroy (context);
    return 0;
}
