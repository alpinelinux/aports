#!/usr/bin/lua


-- those should be read from some config file
aportsdir = "~/aports"
repos = { "main", "testing" }


function split(str)
	local t = {}
	if (str == nil) then
		return nil
	end
	for e in string.gmatch(str, "%S+") do
		table.insert(t, e)
	end
	return t
end

function split_apkbuild(line)
	local r = {}
	local dir, pkgname, pkgver, pkgrel, depends, makedepends, subpackages, source = string.match(line, "([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)")
	r.dir = dir
	r.pkgname = pkgname
	r.pkgver = pkgver
	r.pkgrel = pkgrel
	r.depends = split(depends)
	r.makedepends = split(makedepends)
	r.subpackages = split(subpackages)
	r.source = split(source)
	return r
end

-- parse the APKBUILDs and return a list
function parse_apkbuilds(dir, repos)
	local i,v, p
	local str=""
	if repos == nil then
		return
	end
	--expand repos
	for i,v in ipairs(repos) do
		str = str..dir.."/"..v.."/*/APKBUILD "
	end

	local p = io.popen([[
		for i in ]]..str..[[; do
			pkgname=
			pkgver=
			pkgrel=
			depends=
			makedepends=
			subpackages=
			source=
			dir="${i%/APKBUILD}"
			cd "$dir"
			. ./APKBUILD
			echo $dir\|$pkgname\|$pkgver\|$pkgrel\|$depends\|$makedepends\|$subpackages\|$source
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

function target_packages(pkgdb, pkgname)
	local i,v
	local t = {}
	for i,v in ipairs(pkgdb[pkgname]) do
		table.insert(t, pkgname.."-"..v.pkgver.."-r"..v.pkgrel..".apk")
	end
	return t
end

function list_depends( pkg, pkgdb )
	local dl = {}
	local s
	if pkg and not pkg.added then
		pkg.added = true

		if pkg.depends then
			for i,v in ipairs(pkg.depends) do
				--print("v = <"..v..">")
				s = list_depends( pkgdb[v], pkgdb )
				if #s > 0 then
					dl[#dl + 1] = s
				end
			end
		end
		if pkg.makedepends then
			for i,v in ipairs(pkg.makedepends) do
				--print("v = {"..v.."}")
				s = list_depends( pkgdb[v], pkgdb )
				if #s > 0 then
					dl[#dl + 1] = s
				end
			end
		end
		dl[#dl + 1] = pkg.pkgname
	end

	s = table.concat(dl," ")
	--print("s = ["..s.."]")
	return s
end

function init_apkdb(aportsdir, repos)
	local pkgdb = {}
	local revdeps = {}
	local a

	for a in parse_apkbuilds(aportsdir, repos) do
	--	io.write(a.pkgname.." "..a.pkgver.."\t"..a.dir.."\n")
		if pkgdb[a.pkgname] == nil then
			pkgdb[a.pkgname] = {}
		end
		--table.insert(pkgdb[a.pkgname], a)
		pkgdb[a.pkgname] = a
		--print("pk "..a.pkgname.." is a "..type(a).." ("..pkgdb[a.pkgname].pkgname..")")
		-- add subpackages to package db
		local k,v
		for k,v in ipairs(a.subpackages) do
			if pkgdb[v] == nil then
				pkgdb[v] = {}
			end
			--table.insert(pkgdb[v], a)
			pkgdb[v] = a
		end
		-- add to reverse dependencies
		for k,v in ipairs(a.makedepends) do
			if revdeps[v] == nil then
				revdeps[v] = {}
			end
			table.insert(revdeps[v], a)
		end
	end
	return pkgdb, revdeps
end

-- PKGBUILD import
function split_pkgbuild(line)
	local r = {}
	local pkgname, pkgver = string.match(line, "([^|]*)|([^|]*)")
	r.pkgname = pkgname
	r.pkgver = pkgver
	return r
end

function parse_pkgbuilds(dir, repos)
	local i,v, p
	local str=""
	if repos == nil then
		return
	end
	--expand repos
	for i,v in ipairs(repos) do
		str = str..dir.."/"..v.."/*/PKGBUILD "
	end

	local p = io.popen([[/bin/bash -c '
		for i in ]]..str..[[; do
			pkgname=
			pkgver=
			source $i
			echo $pkgname\|$pkgver
		done
		' 2>/dev/null
	]])
	return function()
		local line = p:read("*line")
		if line == nil then
			p:close()
			return nil
		end
		return split_pkgbuild(line)
	end
end

function init_absdb(dir, repos)
	local p
	local db = {}
	for p in parse_pkgbuilds(dir, repos) do
		if db[p.pkgname] == nil then
			db[p.pkgname] = {}
		end
		table.insert(db[p.pkgname], p.pkgver)
	end
	return db
end


-- Applets -----------------------
applet = {}
function applet.revdep(arg)
	local pkg = arg[2]
	if pkg == nil then
		-- usage?
		return nil
	end
	local apkdb, rev = init_apkdb(aportsdir, repos)
	local _,p
	for _,p in ipairs(rev[pkg] or {}) do
		print(p.pkgname)
	end
end
--absdb = init_absdb("/var/abs", { "core", "extra", "community" })

function applet.deplist(arg)
	local apkdb, rev = init_apkdb(arg[2],{ arg[3] })

	local deplist = {}
	local nm,pk
	for nm,pk in pairs(apkdb) do
		local dl
		--print("pk "..nm.." is a "..type(pk).." ("..apkdb[nm].pkgname..")")
		--deplist[#deplist + 1] = "***"
		dl = list_depends(pk,apkdb)
		-- print("deplist for "..nm..": "..deplist)
		if #dl > 0 then
			deplist[#deplist + 1] = dl
		end
	end
	print(table.concat(deplist," "))
end

cmd = arg[1]

if cmd == nil then
	-- usage
	io.stderr:write( "no command given\n" );
	return
end

if type(applet[cmd]) == "function" then
	applet[cmd](arg)
else
	io.stderr:write(cmd..": invalid applet\n")
end

