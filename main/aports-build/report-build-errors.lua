local publish = require("mqtt.publish")
local json = require("cjson")

local hostname
local f = io.open("/proc/sys/kernel/hostname")
hostname = f:read()
f:close()

local m = {}

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

function run(args)
        local h = io.popen(shell_escape(args))
        local outstr = h:read("*a")
        return h:close(), outstr
end

function m.postbuild(conf, aport, success)
	-- upload log
	local loghost,logdirprefix = (conf.logtarget or ""):match("(.*):(.*)")
	if aport.logfile and loghost and logdirprefix then
		local logdir = logdirprefix.."/"..hostname.."/"..aport:get_repo_name().."/"..aport.pkgname.."/"
		run{"ssh", loghost, "mkdir", "-p", logdir}
		run{"scp", aport.logfile, loghost..":"..logdir}
	end

	if not conf.mqttbroker then
		return
	end

	local topic = ("build/%s/errors"):format(hostname)
	local payload = nil

	if not success then
		payload = json.encode{
			pkgname = aport.pkgname,
			hostname = hostname,
			reponame = aport:get_repo_name(),
			logurl = ("%s/%s/%s/%s-%s-r%s.log"):format(
					conf.logurlprefix or "https://build.alpinelinux.org/buildlogs",
					hostname,
					(string.match(aport.dir,"[^/]+/[^/]+$")),
					aport.pkgname, aport.pkgver, aport.pkgrel),
			status = success
		}
	end
	publish.single(topic, payload, nil, true, conf.mqttbroker)
end

return m

