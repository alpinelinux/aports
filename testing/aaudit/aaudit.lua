local M = {}

local posix = require 'posix'

function M.repohome(repo)
	return ("%s/%s.git"):format(os.getenv("HOME"), repo)
end

function M.write_file(filename, content)
	assert(io.open(filename, "w")):write(content):close()
end

local function load_config(filename)
	local F = assert(io.open(filename, "r"))
	local cfg = "return {" .. F:read("*all").. "}"
	F:close()
	return loadstring(cfg, "config:"..filename)()
end

local function merge_bool(a, b) return a or b end
local function merge_dict(a, b) for k, v in pairs(b) do a[k] = v end return a end
local function merge_array(a, b) for i=1,#b do a[#a+1] = b[i] end return a end

local function load_repo_configs(repohome)
	local G = load_config(("%s/aaudit.conf"):format(os.getenv("HOME")))
	local R = load_config(("%s/aaudit.conf"):format(repohome))
	-- merge global and per-repository group configs
	local RG = (G.groups or {}).all
	for g in pairs(R.groups or {}) do
		RG.notify_emails = merge_dict(RG.notify_emails, g.notify_emails)
		RG.track_filemode = merge_bool(RG.track_filemode, g.track_filemode)
		RG.no_track = merge_array(RG.no_track, g.no_track)
		RG.no_notify = merge_array(RG.no_notify, g.no_notify)
		RG.no_diff = merge_array(RG.no_diff, g.no_diff)
	end
	return G, R, RG
end

local function match_file(fn, match_list)
	if not match_list then return false end
	local i, m
	for i, pattern in ipairs(match_list) do
		if posix.fnmatch(pattern, fn) then return true end
	end
	return false
end

local function sortedpairs(t)
	local i, keys, k = 0, {}
	for k in pairs(t) do keys[#keys+1] = k end
	table.sort(keys)
	return function()
		i = i + 1
		if keys[i] then return keys[i], t[keys[i]] end
	end
end

local function checksum_header(block)
	local sum = 256
	for i = 1,148 do sum = sum + block:byte(i) end
	for i = 157,500 do sum = sum + block:byte(i) end
	return sum
end

local function nullterm(s) return s:match("^[^%z]*") end
local function octal_to_number(str) return tonumber(nullterm(str), 8) end

local function read_header_block(block)
	local header = {
		name = nullterm(block:sub(1,100)),
		mode = octal_to_number(block:sub(101,108)),
		uid = octal_to_number(block:sub(109,116)),
		gid = octal_to_number(block:sub(117,124)),
		size = octal_to_number(block:sub(125,136)),
		mtime = octal_to_number(block:sub(137,148)),
		chksum = octal_to_number(block:sub(149,156)),
		typeflag = block:sub(157,157),
		linkname = nullterm(block:sub(158,257)),
		magic = block:sub(258,263),
		version = block:sub(264,265),
		uname = nullterm(block:sub(266,297)),
		gname = nullterm(block:sub(298,329)),
		devmajor = octal_to_number(block:sub(330,337)),
		devminor = octal_to_number(block:sub(338,345)),
		prefix = nullterm(block:sub(346,500)),
	}
	if header.magic ~= "ustar " and header.magic ~= "ustar\0" then
		return false, "Invalid header magic "..header.magic
	end
	if header.version ~= "00" and header.version ~= " \0" then
		return false, "Unknown version "..header.version
	end
	if not checksum_header(block) == header.chksum then
		return false, "Failed header checksum"
	end
	return header
end

local function import_tar(TAR, GIT, CI, RG)
	local branch_ref = "refs/heads/import"
	local from_ref = "refs/heads/master"
	local blocksize = 512
	local zeroblock = string.rep("\0", blocksize)
	local nextmark = 1
	local author_time = 0
	local all_files = {}
	local long_name, long_link_name
	local symlinkmode = tonumber('0120000', 8)
	local rwmode = tonumber('0755', 8)
	local romode = tonumber('0644', 8)
	local wandmode = tonumber('0111', 8)

	while true do
		local block = TAR:read(blocksize)
		if not block then
			return false, "Premature end of archive"
		end
		if block == zeroblock then break end

		local header, err = read_header_block(block)
		if not header then return false, err end

		local file_data = TAR:read(math.ceil(header.size / blocksize) * blocksize):sub(1,header.size)
		if header.typeflag == "L" then
			long_name = nullterm(file_data)
		elseif header.typeflag == "K" then
			long_link_name = nullterm(file_data)
		else
			if long_name then
				header.name = long_name
				long_name = nil
			end
			if long_link_name then
				header.linkname = long_link_name
				long_link_name = nil
			end
		end

		if header.typeflag:match("^[0-46]$") and
		   not match_file(header.name, RG.no_track) then
			GIT:write('blob\n', 'mark :', nextmark, '\n')
			if header.typeflag == "2" then
				GIT:write('data ', tostring(#header.linkname), '\n', header.linkname, '\n')
				header.mode = symlinkmode
			else
				GIT:write('data ', tostring(header.size), '\n', file_data, '\n')
			end
			local fn = header.prefix..header.name
			all_files[fn] = { mark=nextmark, mode=header.mode, uname=header.uname, gname=header.gname }
			nextmark = nextmark + 1
			if header.mtime > author_time then author_time = header.mtime end
		end
	end
	if RG.track_filemode then
		GIT:write('blob\n', 'mark :', nextmark, '\n',
			'data <<END_OF_PERMISSONS\n')
		for path, v in sortedpairs(all_files) do
			GIT:write(("%o %s:%s %s\n"):format(v.mode, v.uname, v.gname, path))
		end
		GIT:write('END_OF_PERMISSONS\n')
	end

	GIT:write(([[
commit %s
author %s <%s> %d +0000
committer %s <%s> %d +0000
data <<END_OF_COMMIT_MESSAGE
%s
END_OF_COMMIT_MESSAGE

]]):format(branch_ref,
	CI.identity_name, CI.identity_email, author_time,
	CI.identity_name, CI.identity_email, os.time(),
	CI.message or "Changes"))

	if not CI.initial then GIT:write(("from %s^0\n"):format(from_ref)) end
	GIT:write("deleteall\n")
	if RG.track_filemode then
		GIT:write(("M %o :%i %s\n"):format(romode, nextmark, '.permissions.txt'))
	end
	local path, v
	for path, v in pairs(all_files) do
		local mode = v.mode
		if mode ~= symlinkmode then
			if bit32.band(mode, wandmode) then
				mode = rwmode
			else
				mode = romode
			end
		end
		GIT:write(("M %o :%i %s\n"):format(mode, v.mark, path))
	end
	GIT:write("\n")

	return true
end

local function generate_diff(repohome, commit, RG)
	local DIFF = io.popen(("cd %s; git show %s --"):format(repohome, commit), "r")
	local visible = true
	local has_changes, has_visible_changes = false, false
	local text = {}
	for l in DIFF:lines() do
		local fn = l:match("^diff [^ \t]* a/([^ \t]*)")
		if fn then
			has_changes = true
			visible = not match_file(fn, RG.no_notify)
			if visible then
				has_visible_changes = true
				visible = not match_file(fn, RG.no_diff)
				if not visible then
					table.insert(text, "Private file "..fn.." changed")
				end
			end
		end
		if visible then table.insert(text, l) end
	end
	DIFF:close()
	if not has_visible_changes then text = nil end
	return has_changes, text
end

local function send_email(addresses, body, CI, G, R, RG)
	if not body then return end
	if not RG.notify_emails then return end

	local EMAIL = io.popen(("sendmail -t -S %s"):format(G.smtp_server), "w")
	EMAIL:write(([[
From: %s <%s>
To: %s
Subject: apkovl changed - %s (%s)
Date: %s

]]):format(	CI.identity_name, CI.identity_email,
		table.concat(RG.notify_emails, ", "),
		R.description, R.address,
		os.date("%a, %d %b %Y %H:%M:%S")))

	for _, l in ipairs(body) do EMAIL:write(l,'\n') end
	EMAIL:close()
end

function M.import_commit(repohome, CI)
	local G, R, RG = load_repo_configs(repohome)

	CI.identity_name, CI.identity_email = table.unpack(G.identities[CI.identity])
	CI.identity_name  = CI.identity_name  or "Alpine Auditor"
	CI.identity_email = CI.identity_email or "auditor@alpine.local"

	local TAR = io.popen(("ssh root@%s 'lbu package -' | gunzip"):format(R.address), "r")
	local GIT = io.popen(("cd %s; git fast-import --quiet"):format(repohome), "w")
	local rc, err = import_tar(TAR, GIT, CI, RG)
	GIT:close()
	TAR:close()
	if not rc then return rc, err end

	local has_changes, email_body = generate_diff(repohome, "import", RG)
	if has_changes then
		if not CI.initial then send_email(CONF, email_body, CI, G, R, RG) end
		os.execute(("cd %s; git branch --quiet --force master import; git branch --quiet -D import"):format(repohome))
	else
		os.execute(("cd %s; git branch --quiet -D import; git gc --quiet --prune=now"):format(repohome))
	end

end

return M
