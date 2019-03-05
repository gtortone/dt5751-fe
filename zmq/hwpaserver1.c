//  Hello World server

#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

int main (void)
{
    //  Socket to talk to clients
    void *context = zmq_ctx_new ();
    void *responder = zmq_socket (context, ZMQ_REP);
    int rc = zmq_bind (responder, "tcp://*:5555");
    printf ("*context: %p *responder: %p rc:%d \n", context, responder, rc);
    printf("%s\n", strerror(errno));
    assert (rc == 0);

    while (1) {
      uint32_t rcvbuf [10];
      zmq_recv (responder, rcvbuf, sizeof(rcvbuf), 0);
      printf ("Received buffer\n");
      for (int i=0;i<10;i++) {
	  printf ("rcvbuf[%i]: %d\n", i, rcvbuf[i]);
	  //	  sleep (1);          //  Do some 'work'
	  zmq_send (responder, "Done", 5, 0);
	}
    }
    return 0;
}
