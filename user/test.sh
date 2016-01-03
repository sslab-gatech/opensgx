#!/bin/bash

SGX=$(dirname "$0")/../opensgx
PYTHON=python

print_usage() {
  cat <<EOF
[usage] $0 [option]... [binary]
-a|--all  : test all cases
-h|--help : print help
-i|--icount : count the number of executed instructions
--perf|--performance-measure : measure SGX emulator performance metrics
[test]    : run a test case
EOF
  for f in test/*.c; do
    printf " %-30s: %s\n" "$f" "$(cat $f| head -1 | sed 's#//##g')"
  done
}

run_test() {
  FILE=$1
  if [ ! -f $FILE ];
  then
    echo -n "Binary $FILE missing. (Build and retry) "
  fi

  mkdir -p log
  BASE=log/$(basename $FILE)
  $SGX -t $@ >$BASE.stdout 2>$BASE.stderr
  EXIT=$?
  EXPECT=0

  if [[ $FILE =~ fault.* ]]; then
    EXPECT=139
  fi

  if [[ $FILE =~ exception-div-zero.* ]]; then
    EXPECT=136
  fi

  if [[ $EXIT == $EXPECT ]]; then
    echo -n "$(tput setaf 2)OK$(tput sgr0)"
  else
    echo -n "$(tput setaf 1)FAIL ($EXIT)$(tput sgr0)"
  fi
}

perf_test() {
  mkdir -p log
  BASE=log/$(basename $1)
  $SGX $@ >$BASE.stdout 2>$BASE.stderr
  echo "$1"
  echo "-----------------------"
  awk '/count/ {print}' $BASE.stdout
  awk '/region/ {print}' $BASE.stdout
}

if [[ $# == 0 ]]; then
  print_usage
  exit 0
fi

case "$1" in
  -h|--help)
    print_usage
    exit 0
    ;;
  -a|--all)
    for f in test/*.c; do
      OUT=${f%%.c}
      if [[ "$OUT" == "test/simple-arg" ]]; then
      printf "%-30s: please test it with one additional argument vector\n" "$OUT"
      continue
      fi
      if [[ "$OUT" == "test/simple-recv" ]]; then
      printf "%-30s: please test it with simple_send together\n" "$OUT"
      continue
      fi
      if [[ "$OUT" == "test/simple-attest" ]]; then
      printf "%-30s: please test it with attest_nonEnc together\n" "$OUT"
      continue
      fi
      if [[ "$OUT" == "test/simple-network" ]]; then
      printf "%-30s: please test it with attest_network together\n" "$OUT"
      continue
      fi
      if [[ "$OUT" == "test/simple-quote" ]]; then
      printf "%-30s: please test it with simple_send together\n" "$OUT"
      continue
      fi
      if [[ "$OUT" == "test/simple-server" ]]; then
      printf "%-30s: please test it with simple_client together\n" "$OUT"
      continue
      fi
      if [[ "$OUT" == "test/simple-quotingEnclave" ]]; then
      printf "%-30s: please test it with simple_targetEnclave together\n" "$OUT"
      continue
      fi
      if [[ "$OUT" == "test/simple-targetEnclave" ]]; then
      printf "%-30s: please test it with simple_quotingEnclave together\n" "$OUT"
      continue
      fi
      if [[ "$OUT" == "test/simple-openssl" ]]; then
      printf "%-30s: temporarily blocked\n" "$OUT"
      continue
      fi
      if [[ "$OUT" == "test/simple-aes" ]]; then
      printf "%-30s: temporarily blocked\n" "$OUT"
      continue
      fi
      printf "%-30s: %s\n" "$OUT" "$(run_test $OUT)"
    done
    ;;
  --perf|--performance-measure)
    MATCH=0
    for f in test/*.c; do
      TARGET=${f%%.c}
      if [[ "test/$2" == "$TARGET" ]]; then
         perf_test $TARGET
         MATCH=1
      elif [[ "$2" == "$TARGET" ]]; then
         perf_test $TARGET
         MATCH=1
      fi
    done
    if [ $MATCH -lt 1 ]; then
      echo "Usage: ./test.sh --perf app_name in test folder"
      echo "Ex) ./test.sh --perf simple"
    fi
    ;;
  -i|--icount)
    make $2
    shift
    $SGX -i $@
  ;;
  *)
    make $1
    $SGX -t $@
    ;;
esac
