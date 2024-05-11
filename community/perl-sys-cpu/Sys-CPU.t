#!/usr/bin/perl

# Taken from https://sources.debian.org/patches/libsys-cpu-perl/0.61-3/Test-More.patch/

use Test::More tests => 4;

BEGIN { use_ok('Sys::CPU'); }

$number = &Sys::CPU::cpu_count();
ok( defined($number), "CPU Count: $number" );

TODO: {
    local $TODO = "/proc/cpuinfo doesn't always report 'cpu MHz' or 'clock' or 'bogomips' ...";
    $speed = &Sys::CPU::cpu_clock();
    ok( defined($speed), "CPU Speed: $speed" );
}

TODO: {
    local $TODO = "/proc/cpuinfo doesn't always report 'model name' or 'machine' ...";
    $type = &Sys::CPU::cpu_type();
    ok( defined($type), "CPU Type:  $type" );
}
