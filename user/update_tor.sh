#!/bin/bash

DISASM=sgx-tor.dump
INST=sgx-tor.inst

objdump -d --start-address=0x20008000 tor/sgx-tor > $DISASM
python get_inst.py $DISASM $INST
