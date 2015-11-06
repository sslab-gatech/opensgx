#pragma once

//
// comment this to depress all debuging message
//
// ex) sgx_dbg(filter, msg), err(msg),
//     sgx_dbg(unlink, "failed:%d", error)
//

#define DEBUG (0)
#define TEST  (1)

#ifdef SGX_KERNEL
# define __SGX_DEBUG_MARK "K"
#else
# ifdef SGX_USERLIB
#  define __SGX_DEBUG_MARK "L"
# else
#  define __SGX_DEBUG_MARK "Q"
# endif
#endif

#ifdef SGX_DEBUG

 enum { sgx_dbg_welcome  = 0 }; // welcome
 enum { sgx_dbg_ttrace   = 0 }; // verbose trace msg
 enum { sgx_dbg_mtrace   = 0 }; // memory trace msg
 enum { sgx_dbg_trace    = 0 }; // light trace msg
 enum { sgx_dbg_info     = 0 }; // info
 enum { sgx_dbg_warn     = 1 }; // warning
 enum { sgx_dbg_dbg      = 0 }; // dbg msg
 enum { sgx_dbg_err      = 1 }; // err msg
 enum { sgx_dbg_rsa      = 0 }; // rsa-related msg
 enum { sgx_dbg_test     = 0 }; // unit test msg
 enum { sgx_dbg_eenter   = 0 }; // sgx eenter instruction
 enum { sgx_dbg_eadd     = 0 }; // sgx eadd instruction
 enum { sgx_dbg_kern     = 0 }; // sgx-kerl debug msg
 enum { sgx_dbg_user     = 0 }; // sgx-user debug msg

# define sgx_dbg( filter, msg, ... )            \
    do {                                        \
        if ( sgx_dbg_##filter ) {               \
            fprintf(stderr, __SGX_DEBUG_MARK    \
                    "> %s(%d): " msg            \
                    "\n",                       \
                    __FUNCTION__,               \
                    __LINE__,                   \
                    ##__VA_ARGS__ );            \
        }                                       \
    } while( 0 )

# define sgx_ifdbg( filter, statements )        \
    do {                                        \
        if ( sgx_dbg_##filter ) {               \
            (statements);                       \
        }                                       \
    } while ( 0 )

# define sgx_msg( filter, msg )                 \
        sgx_dbg( filter, "%s", msg )

#else

# define sgx_dbg( filter, msg, ... )            \
    do {                                        \
    } while( 0 )

# define sgx_ifdbg( filter, statements )        \
    do {                                        \
    } while ( 0 )

# define sgx_msg( filter, msg )                 \
    do {                                        \
    } while ( 0 )

#endif

#define sgx_err( msg, ... )                     \
    do {                                        \
        fprintf(stderr, "[!] %s(%d): " msg      \
                "\n",                           \
                __FUNCTION__,                   \
                __LINE__,                       \
                ##__VA_ARGS__ );                \
    } while( 0 )
