#!/bin/bash
for obj_file in "$@"
do
  if [[ -x "$obj_file" ]]
  then
    ADDR=$(readelf -WS $obj_file | grep .text | awk '{ print "0x"$5 }')
    echo "add-symbol-file $obj_file ${ADDR}"
  fi
done