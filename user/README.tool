SGX TOOL DOCUMENTATION

1. Generate RSA key with give bits
   sgx-tool -k BITS
e.g.,
   sgx-tool -k 3072 # For enclave key pair
   sgx-tool -k 128 # For device key pair

2. Generate sigstruct format (with reserved fields filled with 0s)
   sgx-tool -S

3. Measure binary (generate enclave hash)
   sgx-tool -m path/to/binary --begin=STARTADDR --end=ENDADDR --entry=ENTRYADDR
e.g.,
   sgx-tool -m test/simple --begin=0x426000 --end=0x426018 --entry=0x426000

Note: offset can be obtained from using "readelf -S" and find the offset of
      correspoding section.

4. Sign on sigstruct format with given key (after manually fill the fields)
   sgx-tool -s path/to/sigstructfile --key=path/to/enclavekeyfile
e.g.,
   sgx-tool -s sgx-sigstruct.conf --key=sgx-test.conf

   Note, user should manually fill the sign information in the sigstruct file
   after sign.

5. Generate einittoken format (with reserved fields filled with 0s)
   sgx-tool -E

6. Generate MAC over einittoken format with given key (after manually fill the fields)
   sgx-tool -M path/to/einittokenfile --key=path/to/devicekeyfile
e.g.,
   sgx-tool -M sig-einittoken.conf --key=sgx-intel-processor.conf

   Note, user should manually fill the mac into einittoken file.

