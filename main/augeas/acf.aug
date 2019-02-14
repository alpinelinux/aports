(* Copyright (C) 2016 Kaarle Ritvanen *)

module Acf =

autoload xfm

let xfm = transform IniFile.lns_loose (incl "/etc/acf/acf.conf")
