module(..., package.seeall)

abuild_conf_file = "/etc/abuild.conf"

local abuild_conf = {}

function get_abuild_conf(var)
	if abuild_conf[var] == nil then
		local f = io.popen(" . "..abuild_conf_file..' ; echo -n "$'..var..'"')
		abuild_conf[var] = f:read("*all")
		f:close()
	end
	return abuild_conf[var]
end

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

function get_maintainer(pkg)
	if pkg == nil or pkg.dir == nil then
		return nil
	end
	local f = io.open(pkg.dir.."/APKBUILD")
	if f == nil then
		return nil
	end
	local line
	for line in f:lines() do
		local maintainer = line:match("^%s*#%s*Maintainer:%s*(.*)")
		if maintainer then
			f:close()
			return maintainer
		end
	end
	f:close()
	return nil
end

function get_repo_name(pkg)
	if pkg == nil or pkg.dir == nil then
		return nil
	end
	return string.match(pkg.dir, ".*/(.*)/.*")
end

function get_apk_filename(pkg)
	return pkg.pkgname.."-"..pkg.pkgver.."-r"..pkg.pkgrel..".apk"
end

function get_apk_file_path(pkg)
	local pkgdest = get_abuild_conf("PKGDEST")
	if pkgdest ~= nil and pkgdest ~= "" then
		return pkgdest.."/"..get_apk_filename(pkg)
	end
	local repodest = get_abuild_conf("REPODEST")
	if repodest ~= nil and repodest ~= "" then
		local arch = get_abuild_conf("CARCH")
		return repodest.."/"..get_repo_name(pkg).."/"..arch.."/"..get_apk_filename(pkg)
	end
	return pkg.dir.."/"..get_apk_filename(pkg)
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

function Aports:foreach_aport(f)
	self:foreach(function(pkgname)
		self:foreach_pkg(pkgname, function(i, pkg)
			if pkgname == pkg.pkgname then
				f(pkg)
			end
		end)
	end)
end

function new(repodirs)
	local h = Aports
	h.repodirs = repodirs
	h.apks, h.revdeps = init_apkdb(repodirs)
	return h
end

