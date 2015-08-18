#include <sgx-lib.h>
#include <sgx.h>
#include "attest-lib.h"

#define DH_P_SIZE 32
#define GENERATOR "4"

//extern int sgx_entropy_func( void *data, unsigned char *output, size_t len );

/* main operation. communicate with tor-gencert & tor process */
//int main(int argc, char *argv[])
void enclave_main()
{
    int ret;
    size_t n, buflen;

    unsigned char *p, *end;
    unsigned char *buf = sgx_malloc(2048);
    const char *pers = "dh_client";

    char ip[] = "127.0.0.1";
    char port[] = "34444";
    char port2[] = "35555";
    char port3[] = "36666";

    char recvPort[] = "37001";
    char recvPort2[] = "37002";
    char recvPort3[] = "37003";

    char sendPort[] = "38001";
    char sendPort2[] = "38002";
    char sendPort3[] = "38003";
    
    int wait_n = 0;

    entropy_context entropy;
    ctr_drbg_context ctr_drbg;
    dhm_context dhm;
    aes_context aes;

    sgx_dhm_init( &dhm );
    sgx_aes_init( &aes );

    /* -------------- Setup RNG ---------------*/
    sgx_entropy_init( &entropy );
//    if( ( ret = ctr_drbg_init( &ctr_drbg, entropy_func, &entropy,
//                               (const unsigned char *) pers,
//                               strlen( pers ) ) ) != 0 )
    if( ( ret = sgx_ctr_drbg_init( &ctr_drbg, &entropy,
                               (const unsigned char *) pers,
                               sgx_strlen( pers ) ) ) != 0 )
    {
//        sgx_printf( "failed! ctr_drbg_init returned %d\n", ret );
        goto exit;
    }

    char *targetinfo = sgx_malloc(sizeof(targetinfo_t));
    char * reportdata; 
    unsigned char * outputdata; 
    unsigned char * outputdata2; 

    reportdata =  sgx_memalign(128,64); 
    sgx_memset(reportdata, 0, 64); 
    outputdata = sgx_memalign(512,512); 
    outputdata2 = sgx_memalign(512,512); 
    sgx_memset(outputdata, 0 , 512); 
    sgx_memset(outputdata2, 0 , 512); 
    
    sgx_report((targetinfo_t *)targetinfo, reportdata, outputdata); 

//send report .      
 
    sgx_send(ip, sendPort, outputdata, 512); 

    char *report_buf     =  sgx_memalign(512,512);
    char *tmp_report_ori = sgx_malloc(512);
    char *tmp_report_mac = sgx_malloc(512);

    /*--------------- Recv data from server -------------*/
    sgx_memset( buf, 0, sizeof( buf ) );
//    sgx_recv(port, buf);
    sgx_recv(recvPort, report_buf);

    sgx_memcpy(tmp_report_ori, report_buf, 512);
    sgx_memcpy(tmp_report_mac, report_buf, 512);

    report_t *report_ori_ptr = (report_t *)tmp_report_ori;
    report_t *report_mac_ptr = (report_t *)tmp_report_mac;
    sgx_memcpy(buf, &report_ori_ptr->reportData, 16);

    char gb_prime[] = "gb_prime";
    sgx_puts(gb_prime);

    /*--------------- Set DH Params -------------*/
    p = buf, end = buf + 16;

    if( ( ret = sgx_dhm_read_params( &dhm, &p, end ) ) != 0 )
    {
//        sgx_printf( " failed! dhm_read_params returned %d\n\n", ret );
        goto exit;
    }

//    sgx_printf("%d\n", dhm.len);
/*
    if( dhm.len < 64 || dhm.len > 512 )
    {
        ret = 1;
        sgx_printf( " failed! Invalid DHM modulus size\n\n" );
        goto exit;
    }
*/

    /*-------------- Send client public value to server -------------*/
    sgx_memset( buf, 0, sizeof( buf ) );
    n = dhm.len;
//    if( ( ret = dhm_make_public( &dhm, (int) dhm.len, buf, n,
//                                 ctr_drbg_random, &ctr_drbg ) ) != 0 )
    if( ( ret = sgx_dhm_make_public( &dhm, (int) dhm.len, buf, n,
                                 &ctr_drbg ) ) != 0 )
    {
//        sgx_printf( " failed! dhm_make_public returned %d\n", ret );
        goto exit;
    }

    char *tmp_report_target = sgx_malloc(sizeof(report_t));
    targetinfo_t *targetinfo_ptr = (targetinfo_t *)targetinfo;
    report_t *report_ptr = (report_t *)tmp_report_target;
    
//    report_t tmp_report_target;
    sgx_memcpy(tmp_report_target, report_buf, 512);
    sgx_memcpy(&targetinfo_ptr->miscselect , &report_ptr->miscselect,4);
    sgx_memcpy(&targetinfo_ptr->attributes , &report_ptr->attributes,16);
    sgx_memcpy(&targetinfo_ptr->measurement, &report_ptr->mrenclave,32);

    sgx_memset(reportdata, 0 , sizeof(reportdata));
    sgx_memcpy(reportdata, buf, 16);
    sgx_memset(outputdata, 0 , 512);

    char ga_prime[] = "ga_prime";
    sgx_puts(ga_prime); 
    sgx_puts(buf);
    sgx_report((targetinfo_t *)targetinfo, reportdata, outputdata);

    //egetkey
//    keyrequest_t keyreq;
    char *keyreq = sgx_malloc(sizeof(keyrequest_t));
    char *outputdata_key;

    keyrequest_t *keyreq_ptr = (keyrequest_t *)keyreq;

    keyreq_ptr->keyname = REPORT_KEY;
    sgx_memcpy(&keyreq_ptr->keyid, &report_ori_ptr->keyid, 16);
    sgx_memcpy(&keyreq_ptr->miscmask,&report_ori_ptr->miscselect,4);
    outputdata_key = sgx_memalign(128,128);

    sgx_getkey(keyreq, outputdata_key);

    //reportkey
    uint8_t *tmp_reportkey = sgx_malloc(16);
    //test key
//    uint8_t tmp_reportkey_test[16] = "1234567890";
   

    sgx_memcpy(tmp_reportkey, outputdata_key, 16);
    
    unsigned char *mac = sgx_malloc(16);
    unsigned char *remac = sgx_malloc(16);

    aes_cmac128_context * ctx = sgx_malloc(sizeof(aes_cmac128_context));   
    sgx_aes_cmac128_starts(ctx, tmp_reportkey);
    sgx_aes_cmac128_update(ctx, (uint8_t *)report_mac_ptr, 416);
    sgx_aes_cmac128_final(ctx, report_mac_ptr->mac);
     
    sgx_memcpy(mac, &report_ori_ptr->mac, 16);
    sgx_memcpy(remac,   &report_mac_ptr->mac, 16);

    char error_mac[] ="error_mac";
    if(sgx_memcmp(mac, remac, 16)!=0)
    {
        sgx_puts(error_mac);
	goto exit;
    }


    mac[15] = '\0';
    remac[15] = '\0';

    sgx_puts(mac);
    sgx_puts(remac);

    wait_n = 0;

    while(wait_n < 10000000)
    {
        wait_n ++;
    }

    sgx_send(ip, sendPort2, outputdata, 512);

    /*--------------- Get shared secret ---------------*/
//    if( ( ret = dhm_calc_secret( &dhm, buf, &n,
//                                 ctr_drbg_random, &ctr_drbg ) ) != 0 )
    sgx_memset( buf, 0, sizeof( buf ) );
    n = dhm.len;
    if( ( ret = sgx_dhm_calc_secret( &dhm, buf, &n,
                                 &ctr_drbg ) ) != 0 )
    {
//        sgx_printf( " failed! dhm_calc_secret returned %d\n", ret );
        goto exit;
    }

    sgx_puts(buf);

//    sgx_memcpy(buf, "testtesttesttest", 16);

    /*--------------- Setup decryption key ---------------*/
    sgx_aes_setkey_dec( &aes, buf, 256 );
    sgx_memset( buf, 0, sizeof( buf ) );

    // TODO : Recv buf(encrypted data)
//    sgx_recv(port3, buf);
    sgx_recv(recvPort2, buf);
    sgx_puts((char *)buf);

    for(int i=0;i<8;i++)
        sgx_aes_crypt_ecb( &aes, AES_DECRYPT, buf+i*16, buf+i*16);
    buf[127] = '\0';
    sgx_puts(buf);

exit:
    sgx_aes_free( &aes );
    sgx_dhm_free( &dhm );
    sgx_ctr_drbg_free( &ctr_drbg );
    sgx_entropy_free( &entropy );

    sgx_exit(NULL);
}
