#!/bin/sh

LANG=C

status=0

if ! which readelf 2>/dev/null >/dev/null; then
	echo "'readelf' not found; skipping test"
	exit 0
fi

SKIP='\<g_'

for so in .libs/lib*.so; do
	echo Checking $so for local PLT entries
	readelf -r $so | grep 'JU\?MP_SLOT\?' | grep '\<g_' | grep -v $SKIP && status=1
done

exit $status
