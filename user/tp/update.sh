#!/bin/bash

cd ssl
rm *.o
cp ~/sgx/user/openssl/libssl.a ./
ar -x libssl.a
cd ..

cd crypto
rm *.o
cp ~/sgx/user/openssl/libcrypto.a ./
ar -x libcrypto.a
cd ..
