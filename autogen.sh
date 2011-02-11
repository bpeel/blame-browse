#!/bin/sh

which gnome-autogen.sh > /dev/null 2>/dev/null;

if test "$?" -eq "0"; then
    exec gnome-autogen.sh "$@";
fi;

echo "Please install gnome-autogen.sh and use that instead.";

exit 1;
