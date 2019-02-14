local M = {}

local posix = require 'posix'
local json = require 'cjson'
local zlib = require 'zlib'
local aac = require 'aaudit.common'
local smtp = require 'socket.smtp'

local HOME = os.getenv("HOME")

M.serverconfig = aac.readconfig(("%s/aaudit-server.json"):format(HOME)) or {}

local function merge_bool(a, b) return a or b end
local function merge_array(a, b) a = a or {} if b then for i=1,#b do a[#a+1] = b[i] end end return a end

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

local function rfc822_address(id)
	local identities = M.serverconfig.identities
	if id == nil then id = "_default" end
	if identities and identities[id] then id = identities[id] end
	local name, email = id:match("^(.-) *(<.*>)$")
	if not email then return ("<%s>"):format(id) end
	return ("%s %s"):format(name, email)
end

local function rfc822_email(rfc822)
	return rfc822:match("(<.*>)$")

end

function M.sendemail(mail)
	local to = {}
	local tocheck = {}
	local m = {
		headers = {
			["Content-Type"] = 'text/plain; charset=utf8',
			["X-RT-Command"] = mail.rtheader,
			from = rfc822_address(mail.from),
			subject = mail.subject,
		},
		body = mail.message,
	}
	local rcpt = {}
	for _, addr in ipairs(mail.to) do
		if tocheck[addr] == nil then
			tocheck[addr] = true
			local rfc822 = rfc822_address(addr)
			table.insert(to, rfc822)
			table.insert(rcpt, rfc822_email(rfc822))
		end
	end
	m.headers.to = table.concat(to, ", ")
	return smtp.send{
		from = rfc822_email(m.headers.from),
		rcpt = rcpt,
		source = smtp.message(m)
	}
end

local rt_keywords = {
	fix = true,
	fixes = true,
	close = true,
	closes = true,
	ref = false,
	refs = false,
	rt = false,
}

local function sendcommitdiff(body, req, R, G)
	if not body then return true end
	if not G.notify_emails then return true end
	if #G.notify_emails == 0 then return true end

	local subject = ("config change - %s (%s)"):format(R.description, R.address)

	local mail = {
		from = req.committer,
		to = G.notify_emails,
		subject = subject,
		message = subject .. "\n\n" .. table.concat(body, '\n')
	}

	-- Set Request Tracker headers if relevant
	local rtqueue = M.serverconfig.rtqueue
	if rtqueue then
		for k,no in req.message:gmatch("(%a+) #(%d+)") do
			local action = rt_keywords[k:lower()]
			if action ~= nil then
				mail.subject = ("[%s #%s] %s"):format(rtqueue, no, mail.subject)
				if action == true then
					mail.rtheader = "Status: resolved"
				end
				break
			end
		end
	end

	-- Send email
	return M.sendemail(mail)
end

local function import_tar(TAR, GIT, req, G)
	local branch_ref = "refs/heads/import"
	local from_ref = "refs/heads/master"
	local blocksize = 512
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
		if not block then return false, "Premature end of archive" end
		if not block:match("[^%z]") then break end

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

		if header.name == "etc/aaudit/aaudit.json" then
			local success, res = pcall(json.decode, file_data)
			if success and res.contact then
				local contact = res.contact
				G.notify_emails = merge_array(G.notify_emails, {contact})
				if req.local_change then
					req.author = rfc822_address(res.contact)
				end
			end
		end

		if header.typeflag:match("^[0-46]$") and
		   not match_file(header.name, G.no_track) then
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
	if G.track_filemode then
		GIT:write('blob\n', 'mark :', nextmark, '\n',
			'data <<END_OF_PERMISSONS\n')
		for path, v in sortedpairs(all_files) do
			GIT:write(("%o %s:%s %s\n"):format(v.mode, v.uname, v.gname, path))
		end
		GIT:write('END_OF_PERMISSONS\n')
	end

	GIT:write(([[
commit %s
author %s %d +0000
committer %s %d +0000
data <<END_OF_COMMIT_MESSAGE
%s
END_OF_COMMIT_MESSAGE

]]):format(branch_ref,
	req.author, author_time,
	req.committer, os.time(),
	req.message))

	if not req.initial then GIT:write(("from %s^0\n"):format(from_ref)) end
	GIT:write("deleteall\n")
	if G.track_filemode then
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

local function generate_diff(repodir, commit, G)
	local DIFF = io.popen(("git --git-dir='%s' show --patch-with-stat '%s' --"):format(repodir, commit), "r")
	local visible = true
	local has_changes, has_visible_changes = false, false
	local text = {}
	for l in DIFF:lines() do
		local fn = l:match("^diff [^ \t]* a/([^ \t]*)")
		if fn then
			has_changes = true
			visible = not match_file(fn, G.no_notify)
			if visible then
				has_visible_changes = true
				visible = not match_file(fn, G.no_diff)
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

function M.loadrepoconfig(repohome)
	return aac.readconfig(("%s/aaudit-repo.json"):format(repohome))
end

local function merge_configs(a, b)
	a = a or {}
	b = b or {}
	a.notify_emails = merge_array(a.notify_emails, b.notify_emails)
	a.track_filemode = merge_bool(a.track_filemode, b.track_filemode)
	a.no_track = merge_array(a.no_track, b.no_track)
	a.no_notify = merge_array(a.no_notify, b.no_notify)
	a.no_diff = merge_array(a.no_diff, b.no_diff)
	return a
end

local function load_repo_configs(repohome)
	local R = M.loadrepoconfig(repohome)
	-- merge global and per-repository group configs
	local G = merge_configs(nil, (M.serverconfig.groups or {}).all)
	for _, name in pairs(R.groups or {}) do
		if name ~= "all" then
			G = merge_configs(G, M.serverconfig.groups[name])
		end
	end
	return R, G
end

function M.repo_update(req,clientstream)
	local repodir = req.repositorydir
	local R, G = load_repo_configs(repodir)

	req.committer = rfc822_address(req.identity)
	req.author = req.committer

	local TAR
	if req.apkovl_follows then
		TAR = zlib.inflate(clientstream)
	else
		TAR = io.popen(("ssh -T root@%s 'lbu package -' | gunzip"):format(R.address), "r")
	end

	local GIT = io.popen(("git --git-dir='%s' fast-import --quiet"):format(repodir), "w")
	local rc, err = import_tar(TAR, GIT, req, G)
	GIT:close()
	TAR:close()
	if not rc then return rc, err end

	local stampfile = ("%s/lastcheck"):format(repodir)
	if posix.utime(stampfile) ~= 0 then
		posix.close(posix.open(stampfile, posix.O_CREAT, "0644"))
	end

	local has_changes, email_body = generate_diff(repodir, "import", G)
	if has_changes then
		if not req.initial then
			local res, err = sendcommitdiff(email_body, req, R, G)
			if not res then
				os.execute(("git --git-dir='%s' branch --quiet -D import;"..
					    "git --git-dir='%s' gc --quiet --prune=now")
					:format(repodir, repodir))
				return false, err or "Sending commitdiff e-mail failed"
			end
		end
		os.execute(("git --git-dir='%s' branch --quiet --force master import;"..
			    "git --git-dir='%s' branch --quiet -D import")
			:format(repodir, repodir))
		return true, "Committed"
	end

	os.execute(("git --git-dir='%s' branch --quiet -D import;"..
		    "git --git-dir='%s' gc --quiet --prune=now")
		:format(repodir, repodir))
	return true, "No changes detected"
end

function M.repo_create(req)
	-- Create repository + write config
	local repodir = req.repositorydir
	os.execute(("mkdir -p '%s'; git init --quiet --bare '%s'")
		:format(repodir, repodir))
	aac.writefile(
		("%s (%s)"):format(req.description, req.target_address),
		("%s/description"):format(repodir))
	aac.writeconfig(
		{ address=req.target_address,
		  description=req.description,
		  groups=req.groups },
		("%s/aaudit-repo.json"):format(repodir))

	-- Inject ssh identity to known_hosts
	if req.ssh_host_key then
		local f = io.open(("%s/.ssh/known_hosts"):format(HOME), "a")
		f:write(("%s %s\n"):format(req.target_address, req.ssh_host_key))
		f:close()
	end
end

function M.handle(req,clientstream)
	req.target_address = req.target_address or req.remote_ip
	req.repositorydir = ("%s/%s.git"):format(HOME, req.target_address)
	req.initial = false
	if req.command == "create" then
		if posix.access(req.repositorydir, "rwx") then
			return false, "Repository exists already"
		end
		M.repo_create(req)
		req.initial = true
		req.command = "commit"
	end
	if req.command == "commit" then
		req.message = req.message or "Configuration change"
		if not posix.access(req.repositorydir, "rwx") then
			return false, "No such repository"
		end
		return M.repo_update(req,clientstream)
	else
		return false,"Invalid request command"
	end
end

return M
