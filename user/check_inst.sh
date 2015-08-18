#! /bin/bash

ret=$(cat $1 | grep $2)

if [ -z "$ret" ];then
  exit 0
fi

exit 1
