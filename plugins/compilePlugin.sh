#!/bin/bash
set -e
MYDIR=$1
BZDIR=$2
echo $BZDIR
if [ "$#" != "2" ] || [ ! -d $MYDIR ] || [ ! -d $BZDIR ] ; then
	echo "Usage: $0 /path/to/plugin /path/to/src/bzflag"
	exit
fi

# Convert $MYDIR to an absolute path
MYDIR=`readlink -fn $MYDIR`

# Find the plugin's name
NAME=`echo -n $MYDIR | awk -F  "/" '{print $NF}'`
echo $NAME

cd $BZDIR/plugins

# Run ./newplug.sh, unless the plugin folder already exists
if [ ! -d $NAME ] ; then
	./newplug.sh $NAME
fi

# Create symbolic links to all .cpp files
for file in `find $MYDIR/*.cpp` ;do
    ln -fs $file $NAME/
done

make
cd $MYDIR
ln -s $BZDIR/plugins/$NAME/.libs/$NAME.so

#to undo:
#remove "plugins/pyrJumpHelper/Makefile" from $BZDIR/configure.ac
#remove "pyrJumpHelper \" from $BZDIR/plugins/Makefile.am
#rm -rf $BZDIR/plugins/pyrJumpHelper
