yaml=require'yaml'

list={}
for line in io.stdin:lines() do
	local m = {}
	m.name=string.match(line, "http://([^/]+)")
	m.urls = { line }
	table.insert(list, m)
end
print(yaml.dump(list))
