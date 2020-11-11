#!/bin/sh

# unit tests for find_ifaces and find_running_ifaces in networking.initd

cfgfile=/tmp/openrc-test-network.$$
sourcefile=$cfgfile.source
sourcedir=$cfgfile.d
ifstate=$cfgfile.state

cat >$cfgfile<<EOF
auto eth0
iface eth0 inet dhcp

source $sourcefile

source-directory $sourcedir
EOF

cat >$sourcefile<<EOF
auto eth1
iface eth1 inet dhcp
EOF

mkdir -p $sourcedir
cat >$sourcedir/a<<EOF
auto eth2
iface eth2 inet dhcp
EOF

cat >$ifstate<<EOF
eth4=eth4 1
EOF

errors=0
fail() {
	echo "$@"
	errors=$(( $errors + 1))
}

# test fallback, when ifquery does not exist
ifquery=does-not-exist
. ./networking.initd

find_ifaces | grep -q -w eth0 || fail "Did not find eth0"
find_ifaces | grep -q -E '(eth1|eth2)' && fail "Unexpectedly found eth1 or eth2"

# test that ifquery finds source and source-directory
unset ifquery
. ./networking.initd
for i in eth0 eth1 eth2; do
	find_ifaces | grep -q -w "$i" || fail "Did not find $i"
done

# test that ifquery picks up the running state file
find_running_ifaces | grep -q -w "eth4" || fail "Did not detect eth4 running"


# test /etc/init.d/net.eth5
RC_SVCNAME=net.eth5
. ./networking.initd
find_ifaces | grep -q -w "eth5" || fail "Did not detect eth5"
find_running_ifaces | grep -q -w "eth5" || fail "Did not detect eth5 running"

rm -rf $cfgfile $sourcefile $sourcedir $ifstate
exit $errors
