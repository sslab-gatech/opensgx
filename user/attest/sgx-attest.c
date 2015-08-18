#include <sgx-lib.h>
#include <sgx.h>
#include "attest-lib.h"

#define DH_P_SIZE 32
#define GENERATOR "4"

#define KEY_SIZE 128
#define EXPONENT 3

//extern int sgx_entropy_func( void *data, unsigned char *output, size_t len );

/* main operation. communicate with tor-gencert & tor process */
//int main(int argc, char *argv[])
void enclave_main()
{
    int ret = 1;
    mpi G, P, Q;
    entropy_context entropy;
    ctr_drbg_context ctr_drbg;
    const char *pers = "dh_genprime";
    dhm_context dhm;
    aes_context aes;
    unsigned char *buf = sgx_malloc(2048);
    size_t n;

    char ip[] = "127.0.0.1";
    char port[] = "34444";
    char port2[] = "35555";
    char port3[] = "36666";


    char sendPort[] = "37001";
    char sendPort2[] = "37002";
    char sendPort3[] = "37003";

    char recvPort[] = "38001";
    char recvPort2[] = "38002";
    char recvPort3[] = "38003";

    int wait_n = 0;

    /* -------------- Prime number generation ---------------*/
    sgx_mpi_init( &G ); sgx_mpi_init( &P ); sgx_mpi_init( &Q );
    sgx_entropy_init( &entropy );

//    sgx_aes_gen_tables();

    if( ( ret = sgx_mpi_read_string( &G, 10, GENERATOR ) ) != 0 )
    {
//        sgx_printf( " failed! mpi_read_string returned %d\n", ret );
        goto exit;
    }

//    if( ( ret = sgx_ctr_drbg_init( &ctr_drbg, sgx_entropy_func, &entropy,
    if( ( ret = sgx_ctr_drbg_init( &ctr_drbg, &entropy,
                               (const unsigned char *) pers,
                               sgx_strlen( pers ) ) ) != 0 )
    {
//        sgx_printf( " failed! ctr_drbg_init returned %d\n", ret );
        goto exit;
    }

//    if( ( ret = mpi_gen_prime( &P, DH_P_SIZE, 1,
//                               ctr_drbg_random, &ctr_drbg ) ) != 0 )
    if( ( ret = sgx_mpi_gen_prime( &P, DH_P_SIZE, 1,
                               &ctr_drbg ) ) != 0 )
    {
//        sgx_printf( " failed! mpi_gen_prime returned %d\n\n", ret );
        goto exit;
    }

    /* -------------- Prime number verification ---------------*/
    if( ( ret = sgx_mpi_sub_int( &Q, &P, 1 ) ) != 0 )
    {
//        sgx_printf( " failed! mpi_sub_int returned %d\n\n", ret );
        goto exit;
    }

    if( ( ret = sgx_mpi_div_int( &Q, NULL, &Q, 2 ) ) != 0 )
    {
//        sgx_printf( " failed! mpi_div_int returned %d\n\n", ret );
        goto exit;
    }

//    if( ( ret = sgx_mpi_is_prime( &Q, ctr_drbg_random, &ctr_drbg ) ) != 0 )
    if( ( ret = sgx_mpi_is_prime( &Q, &ctr_drbg ) ) != 0 )
    {
//        sgx_printf( " failed! mpi_is_prime returned %d\n\n", ret );
        goto exit;
    }

    char *p_str = sgx_malloc(32);
    char *g_str = sgx_malloc(32);
    size_t n1 = sizeof( p_str );   size_t n2 = sizeof( g_str );
    sgx_memset( p_str, 0, n1 );    sgx_memset( g_str, 0, n2 );
    n1 -= 2;                       n2 -= 2;

    /* -------------- Print prime number ---------------*/
/*
    int i;
    for(i=0;i<sizeof(P.p);i++) {
        if(P.p[i] == '\0')
            break;
//        sgx_printf("%X", P.p[i]);
    }
    sgx_printf("\n");

    for(i=0;i<sizeof(G.p);i++) {
        if(G.p[i] == '\0')
            break;
//        sgx_printf("%X", G.p[i]);
    }
//    sgx_printf("\n");
*/
    /* -------------- Set DH params ---------------*/
    sgx_dhm_init( &dhm );
    sgx_mpi_copy(&dhm.P, &P);
    sgx_mpi_copy(&dhm.G, &G);

//    if( ( ret = dhm_make_params( &dhm, (int) mpi_size( &dhm.P ), buf, &n,
//                                 ctr_drbg_random, &ctr_drbg ) ) != 0 )
    sgx_memset(buf, 0, sizeof(buf));
    int size = (int)sgx_mpi_size(&dhm.P);

    if( ( ret = sgx_dhm_make_params( &dhm, size, buf, &n,
                                 &ctr_drbg ) ) != 0 )
    {
//        sgx_printf( "failed! dhm_make_params returned %d\n", ret );
        goto exit;
    }

    /*--------------- Send server data to client -------------*/
//    for( n = 0; n < 16; n++ )
//        sgx_printf( "%02x", buf[n] );
//    sgx_printf("\n");

    char *targetinfo = sgx_malloc(sizeof(targetinfo_t));
    char *tmp_report_target = sgx_malloc(sizeof(report_t));
    char *tmp_report_mine = sgx_malloc(sizeof(report_t));
    char *reportdata;
    unsigned char *outputdata;

    // get report
    reportdata = sgx_memalign(128, 64);
    sgx_memset(reportdata, 0, 64);
    outputdata = sgx_memalign(512, 512);
    sgx_memset(outputdata, 0, 512);

    report_t *report_ptr = (report_t *)tmp_report_target;
//    targetinfo_t *targetinfo = (targetinfo_t *)sgx_malloc(sizeof(report_t));
    targetinfo_t *target_ptr = (targetinfo_t *)targetinfo ;

    char * report_buf = sgx_malloc(512);

    sgx_recv(recvPort, report_buf);

    sgx_memcpy(report_ptr, report_buf, 512);
    sgx_memcpy(&target_ptr->miscselect , &report_ptr->miscselect,4);
    sgx_memcpy(&target_ptr->attributes , &report_ptr->attributes,16);
    sgx_memcpy(&target_ptr->measurement, &report_ptr->mrenclave,32);
    
//    char target_mr= "target_mr";
//    sgx_puts(target_mr);
    sgx_puts((char *)&report_ptr->mrenclave);
    sgx_puts((char *)&report_ptr->attributes);
    sgx_memcpy(reportdata, buf, 16);
    char gb_prime[] = "gb_prime";
    sgx_puts(gb_prime);
    sgx_puts(buf);
    sgx_report(target_ptr, reportdata, outputdata);

    while(wait_n < 10000000)
    {
    wait_n ++;
    }
    wait_n = 0;

    sgx_send(ip, sendPort, outputdata, 512);

    /*--------------- Receive data from client ---------------*/
    sgx_memset( buf, 0, sizeof( buf ) );
    n = dhm.len;

//    sgx_printf("%d\n", dhm.len);

//    sgx_recv(port2, buf);

    sgx_memset(report_buf, 0, sizeof(report_buf));
    sgx_recv(recvPort2, report_buf); 

//    sgx_memcpy(report_buf_mac, report_buf, 512);

    char *tmp_report_ori = sgx_malloc(512);
    char *tmp_report_mac = sgx_malloc(512);


    sgx_memcpy(tmp_report_ori, report_buf, 512);
    sgx_memcpy(tmp_report_mac, report_buf, 512);

    report_t *report_ori_ptr = (report_t *)tmp_report_ori;
    report_t *report_mac_ptr = (report_t *)tmp_report_mac;



    char *keyreq = sgx_malloc(sizeof(keyrequest_t));
    char *outputdata_key;

    keyrequest_t *keyreq_ptr = (keyrequest_t *)keyreq;

    keyreq_ptr->keyname = REPORT_KEY;
    sgx_memcpy(&keyreq_ptr->keyid, &report_ori_ptr->keyid, 16);
    sgx_memcpy(&keyreq_ptr->miscmask,&report_ori_ptr->miscselect,4);
//    sgx_puts((char *)keyreq.keyid);
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
    sgx_memcpy(remac,&report_mac_ptr->mac, 16);

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


//    sgx_send(ip, sendPort2, outputdata, 512);


    char *tmp_report_recv = sgx_malloc(512);
    report_t *report_recv_ptr = (report_t *)tmp_report_recv;
//    report_t * tmp_report_recv = sgx_malloc(512);
    sgx_memset(tmp_report_recv, 0 , 512);
    sgx_memcpy(tmp_report_recv,report_buf,512);
    sgx_memcpy(buf,&report_recv_ptr->reportData, 16);

//    char ga_prime[] = "ga_prime";
//    sgx_puts(ga_prime);
//    sgx_puts(buf);

//    for( n = 0; n < 16; n++ )
//        sgx_printf( "%02x", buf[n] );
//    sgx_printf("\n");

    if( ( ret = sgx_dhm_read_public( &dhm, buf, dhm.len ) ) != 0 )
    {
//        sgx_printf( " failed! dhm_read_public returned %d\n", ret );
        goto exit;
    }

    /*--------------- Get shared secret ---------------*/
//    if( ( ret = dhm_calc_secret( &dhm, buf, &n,
//                                 ctr_drbg_random, &ctr_drbg ) ) != 0 )
    if( ( ret = sgx_dhm_calc_secret( &dhm, buf, &n,
                                 &ctr_drbg ) ) != 0 )
    {
//        sgx_printf( " failed! dhm_calc_secret returned %d\n", ret );
        goto exit;
    }

    sgx_puts(buf);

//    sgx_memcpy(buf, "testtesttesttest", 16);
//    for( n = 0; n < 16; n++ )
//        sgx_printf( "%02x", buf[n] );
//    sgx_printf("\n");

    /*--------------- Encrypt plaintext and send ---------------*/
    sgx_aes_init( &aes );
    sgx_aes_setkey_enc( &aes, buf, 256 );
    sgx_memset( buf, 0, sizeof( buf ) );
//    sgx_memcpy( buf, PLAINTEXT, 128 );
    sgx_memcpy( buf, "==AAAAA BBBBB!====AAAAA BBBBB!====AAAAA BBBBB!====AAAAA BBBBB!====AAAAA BBBBB!====AAAAA BBBBB!====AAAAA BBBBB!====AAAAA BBBBB!==", 128 );

    for(int i=0;i<8;i++)
        sgx_aes_crypt_ecb( &aes, AES_ENCRYPT, buf+i*16, buf+i*16);



/*
    for( n = 0; n < 128; n++ )
        sgx_printf( "%02x", buf[n] );
    sgx_printf("\n");
*/
    // TODO : Send encrypted plaintext
    while(wait_n < 10000000)
    {
        wait_n ++;
    }
    wait_n = 0;

    sgx_send(ip, sendPort2, buf, 128);

//    sgx_memset(buf, 0, sizeof(buf));
//    sgx_recv(recvPort3, buf);

//    sgx_send(ip, port3, buf, 128);
exit:
    sgx_aes_free( &aes );
    sgx_dhm_free( &dhm );
    sgx_ctr_drbg_free( &ctr_drbg );
    sgx_entropy_free( &entropy );

    sgx_exit(NULL);
}
