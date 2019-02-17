#!/bin/sh
JAVA_HOME=/usr/lib/jvm/default-jvm
SPARKDIR=/usr/share/spark
BIN_DIRECTORY=$SPARKDIR/bin
RESOURCE_DIRECTORY=$SPARKDIR/resources
PLUGIN_DIRECTORY=$SPARKDIR/plugins

$JAVA_HOME/bin/java -Dappdir=$SPARKDIR \
-cp $SPARKDIR/lib/log4j.jar:\
$SPARKDIR/lib/jdom.jar:\
$SPARKDIR/lib/fmj.jar:\
$SPARKDIR/lib/startup.jar:\
$SPARKDIR/lib/linux/jdic.jar:\
$SPARKDIR/resources \
org.jivesoftware.launcher.Startup
