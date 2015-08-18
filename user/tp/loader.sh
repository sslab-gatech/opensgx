#!/bin/bash

SGX=$(dirname "$0")/../../sgx
LOADER=$(dirname "$0")/loader

loading() {
  size=$(stat -c%s $1)

  offset=$(readelf -S $1 | grep .enc_text)
  array=($offset)
  offset=${array[4]}

  code_start=$(nm $1 | grep ENCT_START)
  array=($code_start)
  code_start=${array[0]}

  code_end=$(nm $1 | grep ENCT_END)
  array=($code_end)
  code_end=${array[0]}

  data_start=$(nm $1 | grep ENCD_START)
  array=($data_start)
  data_start=${array[0]}

  data_end=$(nm $1 | grep ENCD_END)
  array=($data_end)
  data_end=${array[0]}

  entry=$(nm $1 | grep enclave_start)
  array=($entry)
  entry=${array[0]}

  $SGX $LOADER $1 $size $offset $code_start $code_end $data_start $data_end $entry
}

loading $1