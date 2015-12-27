#!/bin/bash
#
# Usage: 
#    cd YOUR-CHUTNEY-DIRECTORY
#    tools/hsaddress.sh [hs_node]
# Output: for each HS outputs its onion address. If the argument node is
#    specified, it only shows the onion address of that node.
# Examples: tools/hsaddress.sh
#           tools/hsaddress.sh 025h

NAME=$(basename "$0")
DEST=net/nodes
TARGET=hidden_service/hostname

function usage() {
    echo "Usage: $NAME [hs_node]"
    exit 1 
}

function show_address() {
    cat $1
}

[ -d $DEST ] || { echo "$NAME: no nodes available"; exit 1; }
if [ $# -eq 0 ];
then
    for dir in $DEST/*h;
    do
        FILE=${dir}/$TARGET
        [ -e $FILE ] || continue
        echo -n "Node `basename ${dir}`: "
        show_address $FILE
    done
elif [ $# -eq 1 ];
then
    [ -d $DEST/$1 ] || { echo "$NAME: $1 not found"; exit 1; }
    [[ $1 =~ .*h$ ]] || { echo "$NAME: $1 is not a HS"; exit 1; }
    FILE=$DEST/$1/$TARGET
    [ -e $FILE ] || { echo "$NAME: $FILE not found"; exit 1; }
    show_address $FILE
else
    usage
fi
