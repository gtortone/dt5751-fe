//  Hello World client
// gcc -std=c99 -o hwpaclient1.exe hwpaclient1.c -lczmq -lzmq


#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int main (void)
{
    printf ("Connecting to hello world server...\n");
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    zmq_connect (requester, "tcp://pierre4v.triumf.ca:5555");

    int request_nbr, bb;
    for (request_nbr = 0; request_nbr != 10000000; request_nbr++) {
        uint32_t rcvbuf[10];
	uint32_t sndbuf[10000];
	memset(sndbuf, request_nbr, sizeof(sndbuf));
	
	//        printf ("Sending Hello %d...\n", request_nbr);
        zmq_send (requester, sndbuf, sizeof(sndbuf), 0);
	bb = zmq_recv (requester, rcvbuf, 10, 0);
	//printf ("Received status: %d World %d\n", bb, request_nbr);
    }
    zmq_close (requester);
    zmq_ctx_destroy (context);
    return 0;
}
