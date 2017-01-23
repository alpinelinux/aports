
rev = {}
--[[ read from subpkgname.list file ]]--
--[[
for i = 1,#arg do
	pname = arg[i]
	local f = io.popen(([[
		xargs lddtree -l < %s | sort -u | while read f; do
			if [ -e pkg/*"$f" ]; then
				echo $f
			fi
		done
	] ]):format(pname))

	for filename in f:lines() do
		if rev[filename] == nil then
			rev[filename] = {}
		end
		table.insert(rev[filename], (pname:gsub(".list", "")))
	end
	f:close()
end
--]]


function shell_escape(args)
	local ret = {}
	for _,a in pairs(args) do
		s = tostring(a)
		if s:match("[^A-Za-z0-9_/:=-]") then
			s = "'"..s:gsub("'", "'\\''").."'"
		end
		table.insert(ret,s)
	end
	return table.concat(ret, " ")
end

-- read lines from stdin with: <subpackage> <filename>
pkgs = {}
revhash = {}
for line in io.lines() do
	local p, filename = line:match("(.*) (.*)")
	if pkgs[p] == nil then
		pkgs[p] = {bins={}, hasfile={}, needsfile={}}
	end
	table.insert(pkgs[p].bins, filename)
	pkgs[p].hasfile[filename] = true
end

for pkg, r in pairs(pkgs) do
	print("### ".. pkg)
	for _,binfile in pairs(r.bins) do
		local f = io.popen(([[
			lddtree -l %s | sort -u | while read f; do
				if [ -e pkg/*"$f" ]; then
					echo $f
				fi
			done
		]]):format(("pkg/"..pkg.."/"..binfile)))

		for filename in f:lines() do
			if rev[filename] == nil then
				rev[filename] = {}
				revhash[filename] = {}
			end
			if not revhash[filename][pkg] then
				revhash[filename][pkg]=true
				table.insert(rev[filename], pkg)
			end
			r.needsfile[filename]=true
		end
	end
end

-- sort list
keylist = {}
for k,v in pairs(rev) do
	table.insert(keylist, k)
	print(":::: "..k)
	table.sort(v)
end

combos = {}
for _,filename in pairs(keylist) do
	plist = rev[filename]
	s = ""
	for i = 1, #plist do
		if plist[i] ~= "samba-test" then
			s=s..plist[i]..":"
		end
	end
	if combos[s] == nil then
		combos[s] = {}
	end
	table.insert(combos[s], filename)
--	print(#rev[filename], s, filename)
end


combokeys = {}
for pn, filelist in pairs(combos) do
	table.insert(combokeys, pn)
	table.sort(filelist)
end

table.sort(combokeys, function(a,b)
	local _, counta = a:gsub(":", "")
	local _, countb = b:gsub(":", "")
	if counta == countb then
		return a < b
	end
	return counta < countb
end)

for i = 1,#combokeys do
	pn = combokeys[i]
	filelist = combos[pn]

	print(pn)
	for j = 1,#filelist do
		print("", filelist[j])
	end
end
