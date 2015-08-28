OpenSGX: An open platform for Intel SGX
=======================================

Contribution Guides
-------------------
- coding style
- make sure all the test/unit cases pass
- license

Environments & Prerequisites
----------------------------
- Tested: Ubuntu 14.04-15.04, Arch
- Requisite: 
  - Ubuntu: apt-get build-dep qemu
  - Fedora: yum-builddep qemu

~~~~~{.sh}
$ cd qemu
$ ./configure-arch
$ make -j $(nproc)

$ cd ../user/polarssl_sgx
$ make
$ cd ../..
$ make
~~~~~

Run your first OpenSGX program
------------------------------

- Take user/demo/hello.c as an example.

~~~~~{.c}
#include <sgx-lib.h>

void enclave_main()
{
    char *hello = "hello sgx"\n";
    sgx_puts(hello);
    sgx_exit(NULL);
}
~~~~~

~~~~~{.sh}
$ ./opensgx -k
generate sign.key
$ ./opensgx -c user/demo/hello.c
generate hello.sgx
$ ./opensgx -s user/demo/hello.sgx --key sign.key
generate hello.conf
$ ./opensgx user/demo/hello.sgx user/demo/hello.conf
run the program
$ ./opensgx -i user/demo/hello.sgx user/demo/hello.conf
run the program with counting the number of executed guest instructions
~~~~~

Testing
-------

~~~~~{.sh}
$ cd user
$ ./test.sh test/simple
...
$ ./test.sh --help
[usage] ./test.sh [option]... [binary]
-a|--all  : test all cases
-h|--help : print help
-i|--instuct-test : run an instruction test
-ai|--all-instruction-tests  : run all instruction test cases
--perf|--performance-measure : measure SGX emulator performance metrics
[test]
 test/exception-div-zero.c     :  An enclave test case for divide by zero exception.
 test/fault-enclave-access.c   :  An enclave test case for faulty enclave access.
 test/simple-aes.c             :  An enclave test case for simple encryption/decryption using openssl library.
 test/simple-attest.c          :  test network send
 test/simple.c                 :  The simplest enclave enter/exit.
 test/simple-func.c            :  The simplest function call inside the enclave.
 test/simple-getkey.c          :  hello world
 test/simple-global.c          :  The simplest enclave which accesses a global variable
 test/simple-hello.c           :  Hello world enclave program.
 test/simple-network.c         :  test network recv
 test/simple-openssl.c         :  test openssl api
 test/simple-quote.c           :  test network recv
 test/simple-recv.c            :  An enclave test case for sgx_recv.
 test/simple-send.c            :  An enclave test case for sgx_send.
 test/simple-sgxlib.c          :  An enclave test case for sgx library.
 test/simple-stack.c           :  The simplest enclave enter/exit with stack.
 test/stub.c                   :  An enclave test case for stub & trampoline interface.
 test/stub-malloc.c            :  An enclave test case for using heap
 test/stub-realloc.c           :  An enclave test case for sgx_realloc
~~~~~

Pointers
--------

- QEMU side
    - qemu/target-i386/helper.h    : Register sgx helper functions (sgx_encls, sgx_enclu, ...).
    - qemu/target-i386/cpu.h       : Add sgx-specific cpu registers (see refs-rev2 5.1.4).
    - qemu/target-i386/translate.c : Emulates enclave mode memory access semantics.
    - qemu/target-i386/sgx.h       : Define sgx and related data structures.
    - qemu/target-i386/sgx-dbg.h   : Define debugging function.
    - qemu/target-i386/sgx-utils.h : Define utils functions.
    - qemu/target-i386/sgx-perf.h  : Perforamce evaluation.
    - qemu/target-i386/sgx_helper.c: Implement sgx instructions.

- User side
    - user/sgx-kern.c         : Emulates kernel-level functions.
    - user/sgx-user.c         : Emulates user-level functions.
    - user/sgxLib.c           : Implements user-level API.
    - user/sgx-utils.c        : Implements utils functions.
    - user/sgx-signature.c    : Implements crypto related functions.
    - user/sgx-runtime.c      : sgx runtime.
    - user/sgx-test-runtime.c : sgx runtime for test cases.
    - user/include/ : Headers.
    - user/conf/    : Configuration files.
    - user/test/    : Test cases.
    - user/demo/    : Demo case.

Contact
-------

Email: [OpenSGX team](sgx@cc.gatech.edu).

Authors
-------

- Prerit Jain <pjain43@gatech.edu>
- Soham Desai <sdesai1@gatech.edu>
- Seongmin Kim <dallas1004@gmail.com>
- Ming-Wei Shih <mingwei.shih@gatech.edu>
- JaeHyuk Lee <jhl9105@kaist.ac.kr>
- Changho Choi <zpzigi@kaist.ac.kr>
- Taesoo Kim <taesoo@gatech.edu>
- Dongsu Han <dongsu.han@gmail.com>
- Brent Kang <brentkang@gmail.com>

NOTE. All authors at Gatech and KAIST equally contributed to the project
