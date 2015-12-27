#!/bin/bash
#
# Usage:
#    cd YOUR-CHUTNEY-DIRECTORY
#    tools/warnings.sh [node]
# Output: for each node outputs its warnings and the number of times that
# warning has ocurred. If the argument node is specified, it only shows
# the warnings of that node.
# Examples: tools/warnings.sh 
#           tools/warnings.sh 000a

function show_warnings() {
    echo "${GREEN}Node `basename $1`:${NC}"
    sed -n 's/^.*\[warn\]//p' $1/info.log | sort | uniq -c | \
    sed -e 's/^\s*//' -e "s/\([0-9][0-9]*\) \(.*\)/${YELLOW}Warning:${NC}\2${YELLOW} Number: \1${NC}/"
    echo ""
}

function usage() {
    echo "Usage: $NAME [node]"
    exit 1
}

NC=$(tput sgr0)
YELLOW=$(tput setaf 3)
GREEN=$(tput setaf 2)
CHUTNEY=./chutney
NAME=$(basename "$0")
DEST=net/nodes

[ -d net/nodes ] || { echo "$NAME: no logs available"; exit 1; }
if [ $# -eq 0 ]; 
then    
    for dir in $DEST/*;
    do
        [ -e ${dir}/info.log ] || continue
        show_warnings $dir 
    done
elif [ $# -eq 1 ];
then 
    [ -e $DEST/$1/info.log ] || { echo "$NAME: no log available"; exit 1; }
    show_warnings $DEST/$1
else
    usage
fi
