#!/bin/bash

EXPECTED_ARGS=2
E_BADARGS=65

if [ $# -ne $EXPECTED_ARGS ]
then
  echo "Usage: `basename $0` trace output"
  exit $E_BADARGS
fi


grep mm $1 | grep -v set | grep -v port | grep -v lock | grep -v queue | grep -v evict > $2