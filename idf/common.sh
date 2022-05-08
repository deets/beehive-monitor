VERSION=v4.4

if [ -e ~esp/${VERSION} ]
then
    base=~esp/${VERSION}
elif [ -e ~/esp/${VERSION} ]
then
    base=~/esp/${VERSION}
fi

. $base/export.sh
