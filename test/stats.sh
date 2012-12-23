#! /usr/bin/env sh

set -o errexit -o nounset -o noclobber


STATS_FILE=/tmp/kroki-stats.test.$$
EXPECT=$[$(getconf _NPROCESSORS_ONLN) * 3]

KROKI_STATS_FILE=$STATS_FILE ./stats &

for ((i = 0; i < 50; ++i)); do
    kill -0 %1
    if [ -e $STATS_FILE ]; then
        MATCHES=$(../src/kroki-stats $STATS_FILE \
                  | grep -c '^\[[0-9]\+\] kroki\..*: [^0]' || :)
        test $MATCHES -eq $EXPECT && break || :
    fi
    sleep 0.2
done
../src/kroki-stats $STATS_FILE
test $MATCHES -eq $EXPECT

kill -0 %1
# kill && wait should be in one shell command.
kill -TERM %1 && wait %1 2>/dev/null || RC=$?
test $[RC - 128] -eq $(kill -l TERM)

rm $STATS_FILE
