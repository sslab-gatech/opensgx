#include "test.h"

//target enclave
void enclave_main()
{
    int quote_port = 10000;
    char quote_ip[] = "127.0.0.1";

    //port 10001 for challenger
    int challenger_port = 10001;
    char challenger_ip[] = "127.0.0.1";

    struct sockaddr_in quote_addr;
    struct sockaddr_in challenger_addr;

    quote_addr.sin_family = AF_INET;
    quote_addr.sin_port = sgx_htons(quote_port);
    
    challenger_addr.sin_family = AF_INET;
    challenger_addr.sin_port = sgx_htons(challenger_port);


    if(sgx_inet_pton(AF_INET, quote_ip, &quote_addr.sin_addr) <= 0)
    {
    	sgx_exit(NULL);
    }

    sgx_attest_target((struct sockaddr *)&quote_addr, sizeof(quote_addr), 
			(struct sockaddr *)&challenger_addr, sizeof(challenger_addr));

}
