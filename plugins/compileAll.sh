#!/bin/bash
set -e
MYDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BZDIR=$1
echo $BZDIR

if [ "$#" != "1" ] || [ ! -d $BZDIR ] ; then
	echo "Usage: $0 /full/path/to/src/bzflag"
	exit
fi

# Fancy text formatting
bold=`tput bold`
normal=`tput sgr0`
green=`tput setf 2`

cd $MYDIR

echo $bold"Compiling "$green"pyrJumpHelper"$normal$bold"..."
echo $normal
./compilePlugin.sh ../PyrJumpHelper/pyrJumpHelper $BZDIR

echo $bold"Compiling "$green"mapChange"$normal$bold"..."
echo $normal
./compilePlugin.sh mapChange $BZDIR
