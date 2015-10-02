local publish = require("mqtt.publish")
local json = require("cjson")

local hostname
local f = io.open("/proc/sys/kernel/hostname")
hostname = f:read()
f:close()

local urlprefix=("http://build.alpinelinux.org/buildlogs/%s"):format(hostname)

local m = {}

function m.postbuild(aport, success, repodest, arch, logfile)
	if not success then
		local topic = ("build/%s/errors"):format(hostname)
		local payload =	json.encode{
			pkgname = aport.pkgname,
			hostname = hostname,
			logurl = ("%s/%s/%s-%s-r%s.log"):format(
					urlprefix,
					(string.match(aport.dir,"[^/]+/[^/]+$")),
					aport.pkgname, aport.pkgver, aport.pkgrel),
			status = success
		}
		publish.single(topic, payload, nil, nil, "msg.alpinelinux.org")
	end
end

return m

