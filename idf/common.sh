VERSION=v4.4
# One of NODEMCU or TTGO
BOARD=TTGO

if [ -e ~esp/${VERSION} ]
then
    base=~esp/${VERSION}
elif [ -e ~/esp/${VERSION} ]
then
    base=~/esp/${VERSION}
fi

. $base/export.sh

if [ "$BOARD" == "TTGO" ]
then
    build_dir="build-ttgo"
elif [ "$BOARD" == "NODEMCU" ]
    build_dir="build-nodemcu"
else
    echo "Unknown board!"
    exit 1
fi
