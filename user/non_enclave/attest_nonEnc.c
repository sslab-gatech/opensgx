#include <nonEncLib.h>

void main()
{
    char ip[] = "127.0.0.1";
    char sendPort[] = "35555";
    char recvPort[] = "34444";
    char buf[512] = "";
    char msg[512] = "FROM NON_ENCLAVE: HELLO ENCLAVE";
    int i = 0;
    int n_recv = 0, n_send;

    n_send = send_func(ip, sendPort, msg, strlen(msg));
    printf("%d bytes are sent !!\n",n_send);
    n_recv = recv_func(recvPort, buf, 512);
    printf("%d bytes are received !!\n",n_recv);
    hexdump(stdout, buf, n_recv);

    close_socket();
}
