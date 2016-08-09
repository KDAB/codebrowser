#! /bin/sh

#
# This script can be used to generate the compile_commands.json file.
# Configure this script as the compiler, e.g. with the CC/CXX environment variables.
# Export the $COMPILATION_COMMANDS environement variable to the full path of the compile_commands.json file.
# set $FORWARD_COMPILER to the path of the actual compiler to perform the actual compilation.
#
# Example using configure (similar when using qmake):
#
# export COMPILATION_COMMANDS=/path/to/compile_commands.json
# export FORWARD_COMPILER=g++
# CC=/path/to/fake_compiler.sh CXX=/path/to/fake_compiler.sh ./configure
# echo "[" > $COMPILATION_COMMANDS
# make -j1
# echo " { \"directory\": \".\", \"command\": \"true\", \"file\": \"/dev/null\" } ]" >> $COMPILATION_COMMANDS


directory=$PWD
args=$@
file=`echo $args | sed 's/.* \([^ ]*\)/\1/'`
new_file=`cd $directory && readlink -f $file 2>/dev/null | xargs echo -n`
args=`echo $args | sed "s, -I\.\./, -I$directory/../,g" | sed "s, -I\. , -I$directory ,g" | sed "s, -I\./, -I$directory,g" | sed "s, -I\(/]\), -I$directory/\1,g" | sed 's,\\\\,\\\\\\\\,g' | sed 's/"/\\\\"/g'`
echo "{ \"directory\": \"$directory\", \"command\": \"c++ $args\", \"file\": \"$new_file\" } , " >> $COMPILATION_COMMANDS

if [ -z $FORWARD_COMPILER ]; then
    true
else
    $FORWARD_COMPILER "$@"
fi
