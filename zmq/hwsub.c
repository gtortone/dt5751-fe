// gcc -std=c99 -o hwsub.exe hwsub.c -lczmq -lzmq

#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

uint32_t rcvbuf [100];
int main (void)
{
  //  Socket to talk to clients
  void *context = zmq_ctx_new ();
  void *subscriber = zmq_socket (context, ZMQ_SUB);
  int rc = zmq_connect(subscriber, "tcp://chronobox:5555");
  zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
  printf ("*context: %p *subscriber: %p rc:%d \n", context, subscriber, rc);
  
  printf ("Listing in blocking mode the chronobox:5555 port\n");
  while (1) {
    int stat = zmq_recv (subscriber, rcvbuf, sizeof(rcvbuf), 0);
    printf ("stat: %d - rcvbuf[0]: %d\n", stat, rcvbuf[0]);
  }
  
  zmq_close (subscriber);
  zmq_ctx_destroy (context);
  
  return 0;
}
