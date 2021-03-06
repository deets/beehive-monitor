VERSION=v4.4
# One of NODEMCU or TTGO
BOARD=TTGO
# If board is TTGO, we can use
# the LoRa module
USE_LORA=true

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
    BUILD_DIR="build-ttgo"
    if [ "$USE_LORA" == true ]
    then
        BUILD_DIR="build-ttgo-lora"
    fi
elif [ "$BOARD" == "NODEMCU" ]
then
    if [ "$USE_LORA" == true ]
    then
        echo "Can't use LoRa with NodeMCU!"
        exit 1
    fi
    BUILD_DIR="build-nodemcu"
else
    echo "Unknown board!"
    exit 1
fi
export BUILD_DIR
