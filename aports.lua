module(..., package.seeall)

function split(str)
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

function split_apkbuild(line)
	local r = {}
	local dir,pkgname, pkgver, pkgrel, arch, depends, makedepends, subpackages, source = string.match(line, "([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)")
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

-- parse the APKBUILDs and return an iterator
function parse_apkbuilds(dirs)
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


function target_packages(pkgdb, pkgname)
	local i,v
	local t = {}
	for i,v in ipairs(pkgdb[pkgname]) do
		table.insert(t, pkgname.."-"..v.pkgver.."-r"..v.pkgrel..".apk")
	end
	return t
end

function init_apkdb(repodirs)
	local pkgdb = {}
	local revdeps = {}
	local a
	for a in parse_apkbuilds(repodirs) do
	--	io.write(a.pkgname.." "..a.pkgver.."\t"..a.dir.."\n")
		if pkgdb[a.pkgname] == nil then
			pkgdb[a.pkgname] = {}
		end
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
		for k,v in pairs(a.makedepends) do
			if revdeps[v] == nil then
				revdeps[v] = {}
			end
			table.insert(revdeps[v], a)
		end
	end
	return pkgdb, revdeps
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


function recurs_until(apkdb, pn, func)
	local visited={}
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

