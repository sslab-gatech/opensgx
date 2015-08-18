#!/bin/bash

DISASM=sgx-ssl.dump
INST=sgx-ssl.inst

objdump -d --start-address=0x20008000 tp/sgx-ssl > $DISASM
python get_inst.py $DISASM $INST
