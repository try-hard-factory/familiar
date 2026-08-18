#!/bin/sh

die () {
    echo >&2 "$@"
    exit 1
}

echo -n "Version " >ChangeLog.tmp
git describe --tags >>ChangeLog.tmp
echo "==============\n" >>ChangeLog.tmp
git log --pretty=format:'  - %s' $1..HEAD >>ChangeLog.tmp
echo "\n" >> ChangeLog.tmp
cat ChangeLog >>ChangeLog.tmp
mv ChangeLog.tmp ChangeLog