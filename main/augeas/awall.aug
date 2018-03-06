(* Copyright (C) 2018 Kaarle Ritvanen *)

module Awall =

autoload xfm

let xfm = transform Json.lns (
	incl "/etc/awall/*.json" . incl "/etc/awall/*/*.json"
)
