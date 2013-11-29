#!/usr/bin/lua

yaml = require('yaml')

data = yaml.load(io.stdin:read("*all"))
for _,m in pairs(data) do
	for _,url in pairs(m.urls) do
		if (url):match("^http://") then
			print(url)
		end
	end
end
