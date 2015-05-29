#include <nonEncLib.h>

void main()
{
    char ip[] = "127.0.0.1";
    char sendPort[] = "35555";
    char recvPort[] = "34444";
    char buf[512] = "";
    char msg[512] = "FROM NON_ENCLAVE: HELLO ENCLAVE";
    int i = 0;
    recv_func(recvPort, buf, 512);
    printf("%s\n",buf);
    while(1){
        i ++;
        if(i > 100000000) break;
    }
    send_func(ip, sendPort, msg, strlen(msg));
    memset(buf, 0, 512);
    recv_func(recvPort, buf, 512);
    printf("%s\n",buf);
    
    close_socket();
}
