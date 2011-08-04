module(..., package.seeall)

local function split_subpkgs(str)
	local t = {}
	local e
	if (str == nil) then
		return nil
	end
	for e in string.gmatch(str, "%S+") do
		t[#t + 1] = string.gsub(e, ":.*", "")
	end
	return t
end

local function split(str)
	local t = {}
	local e
	if (str == nil) then
		return nil
	end
	for e in string.gmatch(str, "%S+") do
		t[#t + 1] = e
	end
	return t
end

local function split_apkbuild(line)
	local r = {}
	local dir,pkgname, pkgver, pkgrel, arch, depends, makedepends, subpackages, source = string.match(line, "([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)")
	r.dir = dir
	r.pkgname = pkgname
	r.pkgver = pkgver
	r.pkgrel = pkgrel
	r.depends = split(depends)
	r.makedepends = split(makedepends)
	r.subpackages = split_subpkgs(subpackages)
	r.source = split(source)
	return r
end

-- parse the APKBUILDs and return an iterator
local function parse_apkbuilds(dirs)
	local i,v, p
	local str=""
	if dirs == nil then
		return nil
	end
	--expand repos
	for i,v in ipairs(dirs) do
		str = str..v.."/*/APKBUILD "
	end	
		
	local p = io.popen([[ 
		for i in ]]..str..[[; do
			pkgname=
			pkgver=
			pkgrel=
			arch=
			depends=
			makedepends=
			subpackages=
			source=
			dir="${i%/APKBUILD}"
			cd "$dir"
			. ./APKBUILD
			echo $dir\|$pkgname\|$pkgver\|$pkgrel\|$arch\|$depends\|$makedepends\|$subpackages\|$source
		done
	]])
	return function()
		local line = p:read("*line")
		if line == nil then
			p:close()
			return nil
		end
		return split_apkbuild(line) 
	end
end



-- return a key list with makedepends and depends
function all_deps(p)
	local m = {}
	local k,v
	if p == nil then
		return m
	end
	if type(p.depends) == "table" then
		for k,v in pairs(p.depends) do
			m[v] = true
		end
	end
	if type(p.makedepends) == "table" then
		for k,v in pairs(p.makedepends) do
			m[v] = true
		end
	end
	return m
end

function is_remote(url)
	local _,pref
	for _,pref in pairs{ "^http://", "^ftp://", "^https://" } do
		if string.match(url, pref) then
			return true
		end
	end
	return false
end

-- iterate over all entries in source and execute the function for remote
function foreach_remote_source(p, func)
	local _,s
	if p == nil or type(p.source) ~= "table" then
		return
	end
	for _,url in pairs(p.source) do
		if is_remote(url) then
			func(url)
		end
	end
end

local function init_apkdb(repodirs)
	local pkgdb = {}
	local revdeps = {}
	local a
	for a in parse_apkbuilds(repodirs) do
	--	io.write(a.pkgname.." "..a.pkgver.."\t"..a.dir.."\n")
		if pkgdb[a.pkgname] == nil then
			pkgdb[a.pkgname] = {}
		end
		a.all_deps = all_deps
		table.insert(pkgdb[a.pkgname], a)
		-- add subpackages to package db
		local k,v
		for k,v in pairs(a.subpackages) do
			if pkgdb[v] == nil then
				pkgdb[v] = {}
			end
			table.insert(pkgdb[v], a)
		end
		-- add to reverse dependencies
		for v in pairs(all_deps(a)) do
			if revdeps[v] == nil then
				revdeps[v] = {}
			end
			table.insert(revdeps[v], a)
		end
	end
	return pkgdb, revdeps
end

local Aports = {}
function Aports:recurs_until(pn, func)
	local visited={}
	local apkdb = self.apks
	function recurs(pn)
		if pn == nil or visited[pn] or apkdb[pn] == nil then
			return false
		end
		visited[pn] = true
		local _, p
		for _, p in pairs(apkdb[pn]) do
			local d
			for d in pairs(all_deps(p)) do
				if recurs(d) then
					return true
				end
			end
		end
		return func(pn)
	end
	return recurs(pn)
end

function Aports:target_packages(pkgname)
	local i,v
	local t = {}
	for k,v in pairs(self.apks[pkgname]) do
		table.insert(t, pkgname.."-"..v.pkgver.."-r"..v.pkgrel..".apk")
	end
	return t
end

function Aports:foreach(f)
	local k,v
	for k,v in pairs(self.apks) do
		f(k,v) 
	end
end

function Aports:foreach_revdep(pkg, f)
	local k,v
	for k,v in pairs(self.revdeps[pkg] or {}) do
		f(k,v)
	end
end

function Aports:foreach_pkg(pkg, f)
	local k,v
	if self.apks[pkg] == nil then
		io.stderr:write("WARNING: "..pkg.." has no data\n")
	end
	for k,v in pairs(self.apks[pkg]) do
		f(k,v)
	end
end

function new(repodirs)
	local h = Aports
	h.repodirs = repodirs
	h.apks, h.revdeps = init_apkdb(repodirs)
	return h
end

