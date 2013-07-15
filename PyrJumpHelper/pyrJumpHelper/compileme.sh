#!/bin/bash
set -e
MYDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BZDIR=$1
echo $BZDIR
if [ "$#" != "1" ] || [ ! -d $BZDIR ] ; then
	echo "Usage: $0 /full/path/to/src/bzflag"
else
	cd $BZDIR/plugins
	./newplug.sh pyrJumpHelper
	#rm pyrJumpHelper
	#ln -s $MYDIR/pyrJumpHelper
	rm pyrJumpHelper/pyrJumpHelper.cpp
	#rm pyrJumpHelper/Makefile.am
	ln -s $MYDIR/pyrJumpHelper.cpp pyrJumpHelper/
	#ln -s $MYDIR/Makefile.am pyrJumpHelper/
	cd $BZDIR/plugins
	make
	cd $MYDIR
	ln -s $BZDIR/plugins/pyrJumpHelper/.libs/pyrJumpHelper.so
fi
#to undo:
#remove "plugins/pyrJumpHelper/Makefile" from $BZDIR/configure.ac
#remove "pyrJumpHelper \" from $BZDIR/plugins/Makefile.am
#rm -rf $BZDIR/plugins/pyrJumpHelper
