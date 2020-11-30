#!/bin/sh
echo "Content-type: octet/stream"
echo "Transfer-Encoding: chunked"
echo ""
/tmp/streamer-arm /proc/892/mem 0xb5f27000 $((128*1536*6)) 2> /dev/null
