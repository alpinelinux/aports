#!/usr/bin/lua
-- This script takes a mlmmj archive "maildir format" directory and
-- writes out an mbox formatted file to stdout
-- Copyright (c) 2009 N. Angelacos under the  GPL 2 License

require "posix"

-- command line parser, or exit 
check_command_line = function () 
	local source_dir = arg[1]
	local source_time = arg[2]
	
	if (source_dir == nil ) then
		io.stderr:write("mdir2mbox  source_dir [hours]\n" ..
			"Writes an mbox formatted file to stdout from the files in source_dir\n" ..
			"If [hours] is given, then only files newer then [hours] are processed\n")
		os.exit(-1)
	end

	if (posix.stat(source_dir, "type") ~= "directory") then
		io.stderr:write(source_dir .. " is not a directory\n")
		os.exit(-1)
	end

	return source_dir, source_time
end

-- Get candidates 
get_candidates = function (source, hours)
	local all = posix.dir(source)
	local candidates = {}
	local timestamp = 0

	if (hours) then
		timestamp = os.time() - hours * 3600
	end

	for k,v in ipairs(all) do
		local st = posix.stat(source .. "/" .. v)
		if (st) and (st.type == "regular") and (st.mtime > timestamp) then
			table.insert(candidates,source .. "/" .. v)
		end
	end
	
	return candidates
end

file_to_mbox = function (path)
	local fh = io.open(path)
	if (fh == nil) then 
		return 
	end
	local headers = ""
	local l = ""
	-- get headers
	repeat
		headers = headers .. l
		l = (fh:read("*l") or "" ) .. "\n"
	until (#l == 1)

	local from = string.match("\n" .. headers, "\nFrom: ([^\n]*)")
	if from == nil then
		from = string.match("\n" .. headers, "\nReply-To: ([^\n]*)")
	end
	if from == nil then
		from = "<nobody@nowhere.com>"
	end
	from = string.match(from, "<([^>]*)>") or string.match(from, "([^ ]*)")


	local date = string.match("\n" .. headers, "\nDate: ([^\n]*)")
	if date == nil then
		date = os.date ("%c", posix.stat(path, "mtime"))
	end
	local weekday,day,month,year,time,offset = string.match(date, "([^,]*), +(%d+) (%a+) (%d+) ([%d:]*) ([%d]*)")

	print ("From " .. from .. " " .. string.format("%s %s %s %s %s", weekday, month, day, time, year, offset ))
	print (headers)
	
	-- get rest of message
	repeat
		local foo  = fh:read("*l")
		if foo then 
			print(foo)
		end
	until (foo == nil)
	
fh:close()
end

candidates =  get_candidates(check_command_line ())

for k,v in ipairs(candidates) do
	file_to_mbox(v)
end
print ("")
