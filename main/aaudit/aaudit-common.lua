local M = {}

local posix = require 'posix'
local json = require 'cjson'

M.config = "/etc/aaudit/aaudit.json"

function M.readfile(fn)
	local F = io.open(fn, "r")
	if F == nil then return nil end
	local ret = F:read("*all")
	F:close()
	return ret
end

function M.readconfig(fn)
	fn = fn or M.config
	local success, res = pcall(json.decode, M.readfile(fn))
	if not success then io.stderr:write(("Error reading %s: %s\n"):format(fn, res)) end
	return res
end

function M.writefile(content, fn)
	assert(io.open(fn, "w")):write(content):close()
end

function M.writeconfig(config, fn)
	M.writefile(json.encode(config), fn or M.config)
end

return M
