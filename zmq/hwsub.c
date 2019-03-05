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
    void *subscriber = zmq_socket (context, ZMQ_SUB);
    int rc = zmq_bind (subscriber, "tcp://*:5555");
    // With message
    //    zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "Z", 1);
    // Without message
    zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
    printf ("*context: %p *subscriber: %p rc:%d \n", context, subscriber, rc);
    //    printf("%s\n", strerror(errno));

    while (1) {
      uint32_t rcvbuf [100];
      int stat = zmq_recv (subscriber, rcvbuf, sizeof(rcvbuf), 0);
      //      int stat = zmq_recv (subscriber, rcvbuf, sizeof(rcvbuf), ZMQ_DONTWAIT);
      //      printf("%s -- ", strerror(errno));
      printf ("stat: %d - rcvbuf[0]: %d\n", stat, rcvbuf[0]);
    }
    return 0;
}
