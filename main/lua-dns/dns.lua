

-- public domain 20080404 lua@ztact.com


-- todo: quick (default) header generation
-- todo: nxdomain, error handling
-- todo: cache results of encodeName


-- reference: http://tools.ietf.org/html/rfc1035
-- reference: http://tools.ietf.org/html/rfc1876 (LOC)


require 'socket'
require 'ztact'


local coroutine, io, math, socket, string, table =
      coroutine, io, math, socket, string, table

local ipairs, next, pairs, print, setmetatable, tostring =
      ipairs, next, pairs, print, setmetatable, tostring

local get, set = ztact.get, ztact.set


local dns = _G    -------------------------------------------------- module dns
module ((...) or 'dns')
dns = dns[(...) or 'dns']


-- dns type & class codes ------------------------------ dns type & class codes


local append = table.insert


local function highbyte (i)    -- - - - - - - - - - - - - - - - - - -  highbyte
  return (i-(i%0x100))/0x100
  end


local function augment (t)    -- - - - - - - - - - - - - - - - - - - -  augment
  local a = {}
  for i,s in pairs (t) do  a[i] = s  a[s] = s  a[string.lower (s)] = s  end
  return a
  end


local function encode (t)    -- - - - - - - - - - - - - - - - - - - - -  encode
  local code = {}
  for i,s in pairs (t) do
    local word = string.char (highbyte (i), i %0x100)
    code[i] = word
    code[s] = word
    code[string.lower (s)] = word
    end
  return code
  end


dns.types = {
  'A', 'NS', 'MD', 'MF', 'CNAME', 'SOA', 'MB', 'MG', 'MR', 'NULL', 'WKS',
  'PTR', 'HINFO', 'MINFO', 'MX', 'TXT',
  [ 28] = 'AAAA', [ 29] = 'LOC',   [ 33] = 'SRV',
  [252] = 'AXFR', [253] = 'MAILB', [254] = 'MAILA', [255] = '*' }


dns.classes = { 'IN', 'CS', 'CH', 'HS', [255] = '*' }


dns.type      = augment (dns.types)
dns.class     = augment (dns.classes)
dns.typecode  = encode  (dns.types)
dns.classcode = encode  (dns.classes)



local function standardize (qname, qtype, qclass)    -- - - - - - - standardize
  if string.byte (qname, -1) ~= 0x2E then  qname = qname..'.'  end
  qname = string.lower (qname)
  return qname, dns.type[qtype or 'A'], dns.class[qclass or 'IN']
  end


local function prune (rrs, time, soft)    -- - - - - - - - - - - - - - -  prune

  time = time or socket.gettime ()
  for i,rr in pairs (rrs) do

    if rr.tod then
      -- rr.tod = rr.tod - 50    -- accelerated decripitude
      rr.ttl = math.floor (rr.tod - time)
      if rr.ttl <= 0 then  rrs[i] = nil  end

    elseif soft == 'soft' then    -- What is this?  I forget!
      assert (rr.ttl == 0)
      rrs[i] = nil
      end  end  end


-- metatables & co. ------------------------------------------ metatables & co.


local resolver = {}
resolver.__index = resolver


local SRV_tostring


local rr_metatable = {}    -- - - - - - - - - - - - - - - - - - -  rr_metatable
function rr_metatable.__tostring (rr)
  local s0 = string.format (
    '%2s %-5s %6i %-28s', rr.class, rr.type, rr.ttl, rr.name )
  local s1 = ''
  if rr.type == 'A' then  s1 = ' '..rr.a
  elseif rr.type == 'MX' then
    s1 = string.format (' %2i %s', rr.pref, rr.mx)
  elseif rr.type == 'CNAME' then  s1 = ' '..rr.cname
  elseif rr.type == 'LOC'   then  s1 = ' '..resolver.LOC_tostring (rr)
  elseif rr.type == 'NS'    then  s1 = ' '..rr.ns
  elseif rr.type == 'SRV'   then  s1 = ' '..SRV_tostring (rr)
  elseif rr.type == 'TXT'   then  s1 = ' '..rr.txt
  else  s1 = ' <UNKNOWN RDATA TYPE>'  end
  return s0..s1
  end


local rrs_metatable = {}    -- - - - - - - - - - - - - - - - - -  rrs_metatable
function rrs_metatable.__tostring (rrs)
  t = {}
  for i,rr in pairs (rrs) do  append (t, tostring (rr)..'\n')  end
  return table.concat (t)
  end


local cache_metatable = {}    -- - - - - - - - - - - - - - - -  cache_metatable
function cache_metatable.__tostring (cache)
  local time = socket.gettime ()
  local t = {}
  for class,types in pairs (cache) do
    for type,names in pairs (types) do
      for name,rrs in pairs (names) do
        prune (rrs, time)
        append (t, tostring (rrs))  end  end  end
  return table.concat (t)
  end


function resolver:new ()    -- - - - - - - - - - - - - - - - - - - - - resolver
  local r = { active = {}, cache = {}, unsorted = {} }
  setmetatable (r, resolver)
  setmetatable (r.cache, cache_metatable)
  setmetatable (r.unsorted, { __mode = 'kv' })
  return r
  end


-- packet layer -------------------------------------------------- packet layer


function dns.random (...)    -- - - - - - - - - - - - - - - - - - -  dns.random
  math.randomseed (10000*socket.gettime ())
  dns.random = math.random
  return dns.random (...)
  end


local function encodeHeader (o)    -- - - - - - - - - - - - - - -  encodeHeader

  o = o or {}

  o.id = o.id or		-- 16b	(random) id
    dns.random (0, 0xffff)

  o.rd = o.rd or 1		--  1b  1 recursion desired
  o.tc = o.tc or 0		--  1b	1 truncated response
  o.aa = o.aa or 0		--  1b	1 authoritative response
  o.opcode = o.opcode or 0	--  4b	0 query
				--      1 inverse query
				--	2 server status request
				--	3-15 reserved
  o.qr = o.qr or 0		--  1b	0 query, 1 response

  o.rcode = o.rcode or 0	--  4b  0 no error
				--	1 format error
				--	2 server failure
				--	3 name error
				--	4 not implemented
				--	5 refused
				--	6-15 reserved
  o.z  = o.z  or 0		--  3b  0 resvered
  o.ra = o.ra or 0		--  1b  1 recursion available

  o.qdcount = o.qdcount or 1	-- 16b	number of question RRs
  o.ancount = o.ancount or 0	-- 16b	number of answers RRs
  o.nscount = o.nscount or 0	-- 16b	number of nameservers RRs
  o.arcount = o.arcount or 0	-- 16b  number of additional RRs

  -- string.char() rounds, so prevent roundup with -0.4999
  local header = string.char (
    highbyte (o.id),  o.id %0x100,
    o.rd + 2*o.tc + 4*o.aa + 8*o.opcode + 128*o.qr,
    o.rcode + 16*o.z + 128*o.ra,
    highbyte (o.qdcount),  o.qdcount %0x100,
    highbyte (o.ancount),  o.ancount %0x100,
    highbyte (o.nscount),  o.nscount %0x100,
    highbyte (o.arcount),  o.arcount %0x100 )

  return header, o.id
  end


local function encodeName (name)    -- - - - - - - - - - - - - - - - encodeName
  local t = {}
  for part in string.gmatch (name, '[^.]+') do
    append (t, string.char (string.len (part)))
    append (t, part)
    end
  append (t, string.char (0))
  return table.concat (t)
  end


local function encodeQuestion (qname, qtype, qclass)    -- - - - encodeQuestion
  qname  = encodeName (qname)
  qtype  = dns.typecode[qtype or 'a']
  qclass = dns.classcode[qclass or 'in']
  return qname..qtype..qclass;
  end


function resolver:byte (len)    -- - - - - - - - - - - - - - - - - - - - - byte
  len = len or 1
  local offset = self.offset
  local last = offset + len - 1
  if last > #self.packet then
    error (string.format ('out of bounds: %i>%i', last, #self.packet))  end
  self.offset = offset + len
  return string.byte (self.packet, offset, last)
  end


function resolver:word ()    -- - - - - - - - - - - - - - - - - - - - - -  word
  local b1, b2 = self:byte (2)
  return 0x100*b1 + b2
  end


function resolver:dword ()    -- - - - - - - - - - - - - - - - - - - - -  dword
  local b1, b2, b3, b4 = self:byte (4)
  -- print ('dword', b1, b2, b3, b4)
  return 0x1000000*b1 + 0x10000*b2 + 0x100*b3 + b4
  end


function resolver:sub (len)    -- - - - - - - - - - - - - - - - - - - - - - sub
  len = len or 1
  local s = string.sub (self.packet, self.offset, self.offset + len - 1)
  self.offset = self.offset + len
  return s
  end


function resolver:header (force)    -- - - - - - - - - - - - - - - - - - header

  local id = self:word ()
  -- print (string.format (':header  id  %x', id))
  if not self.active[id] and not force then  return nil  end

  local h = { id = id }

  local b1, b2 = self:byte (2)

  h.rd      = b1 %2
  h.tc      = b1 /2%2
  h.aa      = b1 /4%2
  h.opcode  = b1 /8%16
  h.qr      = b1 /128

  h.rcode   = b2 %16
  h.z       = b2 /16%8
  h.ra      = b2 /128

  h.qdcount = self:word ()
  h.ancount = self:word ()
  h.nscount = self:word ()
  h.arcount = self:word ()

  for k,v in pairs (h) do  h[k] = v-v%1  end

  return h
  end


function resolver:name ()    -- - - - - - - - - - - - - - - - - - - - - -  name
  local remember, pointers = nil, 0
  local len = self:byte ()
  local n = {}
  while len > 0 do
    if len >= 0xc0 then    -- name is "compressed"
      pointers = pointers + 1
      if pointers >= 20 then  error ('dns error: 20 pointers')  end
      local offset = ((len-0xc0)*0x100) + self:byte ()
      remember = remember or self.offset
      self.offset = offset + 1    -- +1 for lua
    else    -- name is not compressed
      append (n, self:sub (len)..'.')
      end
    len = self:byte ()
    end
  self.offset = remember or self.offset
  return table.concat (n)
  end


function resolver:question ()    -- - - - - - - - - - - - - - - - - -  question
  local q = {}
  q.name  = self:name ()
  q.type  = dns.type[self:word ()]
  q.class = dns.type[self:word ()]
  return q
  end


function resolver:A (rr)    -- - - - - - - - - - - - - - - - - - - - - - - -  A
  local b1, b2, b3, b4 = self:byte (4)
  rr.a = string.format ('%i.%i.%i.%i', b1, b2, b3, b4)
  end


function resolver:CNAME (rr)    -- - - - - - - - - - - - - - - - - - - -  CNAME
  rr.cname = self:name ()
  end


function resolver:MX (rr)    -- - - - - - - - - - - - - - - - - - - - - - -  MX
  rr.pref = self:word ()
  rr.mx   = self:name ()
  end


function resolver:LOC_nibble_power ()    -- - - - - - - - - -  LOC_nibble_power
  local b = self:byte ()
  -- print ('nibbles', ((b-(b%0x10))/0x10), (b%0x10))
  return ((b-(b%0x10))/0x10) * (10^(b%0x10))
  end


function resolver:LOC (rr)    -- - - - - - - - - - - - - - - - - - - - - -  LOC
  rr.version = self:byte ()
  if rr.version == 0 then
    rr.loc           = rr.loc or {}
    rr.loc.size      = self:LOC_nibble_power ()
    rr.loc.horiz_pre = self:LOC_nibble_power ()
    rr.loc.vert_pre  = self:LOC_nibble_power ()
    rr.loc.latitude  = self:dword ()
    rr.loc.longitude = self:dword ()
    rr.loc.altitude  = self:dword ()
    end  end


local function LOC_tostring_degrees (f, pos, neg)    -- - - - - - - - - - - - -
  f = f - 0x80000000
  if f < 0 then  pos = neg  f = -f  end
  local deg, min, msec
  msec = f%60000
  f    = (f-msec)/60000
  min  = f%60
  deg = (f-min)/60
  return string.format ('%3d %2d %2.3f %s', deg, min, msec/1000, pos)
  end


function resolver.LOC_tostring (rr)    -- - - - - - - - - - - - -  LOC_tostring

  local t = {}

  --[[
  for k,name in pairs { 'size', 'horiz_pre', 'vert_pre',
			'latitude', 'longitude', 'altitude' } do
    append (t, string.format ('%4s%-10s: %12.0f\n', '', name, rr.loc[name]))
    end
  --]]

  append ( t, string.format (
    '%s    %s    %.2fm %.2fm %.2fm %.2fm',
    LOC_tostring_degrees (rr.loc.latitude, 'N', 'S'),
    LOC_tostring_degrees (rr.loc.longitude, 'E', 'W'),
    (rr.loc.altitude - 10000000) / 100,
    rr.loc.size / 100,
    rr.loc.horiz_pre / 100,
    rr.loc.vert_pre / 100 ) )

  return table.concat (t)
  end


function resolver:NS (rr)    -- - - - - - - - - - - - - - - - - - - - - - -  NS
  rr.ns = self:name ()
  end


function resolver:SOA (rr)    -- - - - - - - - - - - - - - - - - - - - - -  SOA
  end


function resolver:SRV (rr)    -- - - - - - - - - - - - - - - - - - - - - -  SRV
  rr.srv = {}
  rr.srv.priority = self:word ()
  rr.srv.weight   = self:word ()
  rr.srv.port     = self:word ()
  rr.srv.target   = self:name ()
  end


function SRV_tostring (rr)    -- - - - - - - - - - - - - - - - - - SRV_tostring
  local s = rr.srv
  return string.format ( '%5d %5d %5d %s',
                         s.priority, s.weight, s.port, s.target )
  end


function resolver:TXT (rr)    -- - - - - - - - - - - - - - - - - - - - - -  TXT
  rr.txt = self:sub (rr.rdlength)
  end


function resolver:rr ()    -- - - - - - - - - - - - - - - - - - - - - - - -  rr
  local rr = {}
  setmetatable (rr, rr_metatable)
  rr.name     = self:name (self)
  rr.type     = dns.type[self:word ()] or rr.type
  rr.class    = dns.class[self:word ()] or rr.class
  rr.ttl      = 0x10000*self:word () + self:word ()
  rr.rdlength = self:word ()

  if rr.ttl == 0 then  -- pass
  else  rr.tod = self.time + rr.ttl  end

  local remember = self.offset
  local rr_parser = self[dns.type[rr.type]]
  if rr_parser then  rr_parser (self, rr)  end
  self.offset = remember
  rr.rdata = self:sub (rr.rdlength)
  return rr
  end


function resolver:rrs (count)    -- - - - - - - - - - - - - - - - - - - - - rrs
  local rrs = {}
  for i = 1,count do  append (rrs, self:rr ())  end
  return rrs
  end


function resolver:decode (packet, force)    -- - - - - - - - - - - - - - decode

  self.packet, self.offset = packet, 1
  local header = self:header (force)
  if not header then  return nil  end
  local response = { header = header }

  response.question = {}
  local offset = self.offset
  for i = 1,response.header.qdcount do
    append (response.question, self:question ())  end
  response.question.raw = string.sub (self.packet, offset, self.offset - 1)

  if not force then
    if not self.active[response.header.id] or
       not self.active[response.header.id][response.question.raw] then
      return nil  end  end

  response.answer     = self:rrs (response.header.ancount)
  response.authority  = self:rrs (response.header.nscount)
  response.additional = self:rrs (response.header.arcount)

  return response
  end


-- socket layer -------------------------------------------------- socket layer


resolver.delays = { 1, 3, 11, 45 }


function resolver:addnameserver (address)    -- - - - - - - - - - addnameserver
  self.server = self.server or {}
  append (self.server, address)
  end


function resolver:setnameserver (address)    -- - - - - - - - - - setnameserver
  self.server = {}
  self:addnameserver (address)
  end


function resolver:adddefaultnameservers ()    -- - - - -  adddefaultnameservers
  for line in io.lines ('/etc/resolv.conf') do
    address = string.match (line, 'nameserver%s+(%d+%.%d+%.%d+%.%d+)')
    if address then  self:addnameserver (address)  end
    end  end


function resolver:getsocket (servernum)    -- - - - - - - - - - - - - getsocket

  self.socket = self.socket or {}
  self.socketset = self.socketset or {}

  local sock = self.socket[servernum]
  if sock then  return sock  end

  sock = socket.udp ()
  if self.socket_wrapper then  sock = self.socket_wrapper (sock)  end
  sock:settimeout (0)
  -- todo: attempt to use a random port, fallback to 0
  sock:setsockname ('*', 0)
  sock:setpeername (self.server[servernum], 53)
  self.socket[servernum] = sock
  self.socketset[sock] = sock
  return sock
  end


function resolver:socket_wrapper_set (func)  -- - - - - - - socket_wrapper_set
  self.socket_wrapper = func
  end


function resolver:closeall ()    -- - - - - - - - - - - - - - - - - -  closeall
  for i,sock in ipairs (self.socket) do  self.socket[i]:close ()  end
  self.socket = {}
  end


function resolver:remember (rr, type)    -- - - - - - - - - - - - - -  remember

  -- print ('remember', type, rr.class, rr.type, rr.name)

  if type ~= '*' then
    type = rr.type
    local all = get (self.cache, rr.class, '*', rr.name)
    -- print ('remember all', all)
    if all then  append (all, rr)  end
    end

  self.cache = self.cache or setmetatable ({}, cache_metatable)
  local rrs = get (self.cache, rr.class, type, rr.name) or
    set (self.cache, rr.class, type, rr.name, setmetatable ({}, rrs_metatable))
  append (rrs, rr)

  if type == 'MX' then  self.unsorted[rrs] = true  end
  end


local function comp_mx (a, b)    -- - - - - - - - - - - - - - - - - - - comp_mx
  return (a.pref == b.pref) and (a.mx < b.mx) or (a.pref < b.pref)
  end


function resolver:peek (qname, qtype, qclass)    -- - - - - - - - - - - -  peek
  qname, qtype, qclass = standardize (qname, qtype, qclass)
  local rrs = get (self.cache, qclass, qtype, qname)
  if not rrs then  return nil  end
  if prune (rrs, socket.gettime ()) and qtype == '*' or not next (rrs) then
    set (self.cache, qclass, qtype, qname, nil)  return nil  end
  if self.unsorted[rrs] then  table.sort (rrs, comp_mx)  end
  return rrs
  end


function resolver:purge (soft)    -- - - - - - - - - - - - - - - - - - -  purge
  if soft == 'soft' then
    self.time = socket.gettime ()
    for class,types in pairs (self.cache or {}) do
      for type,names in pairs (types) do
        for name,rrs in pairs (names) do
          prune (rrs, time, 'soft')
          end  end  end
  else  self.cache = {}  end
  end


function resolver:query (qname, qtype, qclass)    -- - - - - - - - - - -- query

  qname, qtype, qclass = standardize (qname, qtype, qclass)

  if not self.server then  self:adddefaultnameservers ()  end

  local question = question or encodeQuestion (qname, qtype, qclass)
  local peek = self:peek (qname, qtype, qclass)
  if peek then  return peek  end

  local header, id = encodeHeader ()
  -- print ('query  id', id, qclass, qtype, qname)
  local o = { packet = header..question,
              server = 1,
              delay  = 1,
              retry  = socket.gettime () + self.delays[1] }
  self:getsocket (o.server):send (o.packet)

  -- remember the query
  self.active[id] = self.active[id] or {}
  self.active[id][question] = o

  -- remember which coroutine wants the answer
  local co = coroutine.running ()
  if co then
    set (self.wanted, qclass, qtype, qname, co, true)
    set (self.yielded, co, qclass, qtype, qname, true)
    end  end


function resolver:receive (rset)    -- - - - - - - - - - - - - - - - -  receive

  -- print 'receive'  print (self.socket)
  self.time = socket.gettime ()
  rset = rset or self.socket

  local response
  for i,sock in pairs (rset) do

    if self.socketset[sock] then
    local packet = sock:receive ()
    if packet then

    response = self:decode (packet)
    if response then
    -- print 'received response'
    -- self.print (response)

    for i,section in pairs { 'answer', 'authority', 'additional' } do
      for j,rr in pairs (response[section]) do
        self:remember (rr, response.question[1].type)  end  end

    -- retire the query
    local queries = self.active[response.header.id]
    if queries[response.question.raw] then
      queries[response.question.raw] = nil  end
    if not next (queries) then  self.active[response.header.id] = nil  end
    if not next (self.active) then  self:closeall ()  end

    -- was the query on the wanted list?
    local q = response.question
    local cos = get (self.wanted, q.class, q.type, q.name)
    if cos then
      for co in pairs (cos) do
        set (self.yielded, co, q.class, q.type, q.name, nil)
	if not self.yielded[co] then  coroutine.resume (co)  end
        end
      set (self.wanted, q.class, q.type, q.name, nil)
      end  end  end  end  end

  return response
  end


function resolver:pulse ()    -- - - - - - - - - - - - - - - - - - - - -  pulse

  -- print ':pulse'
  while self:receive () do end
  if not next (self.active) then  return nil  end

  self.time = socket.gettime ()
  for id,queries in pairs (self.active) do
    for question,o in pairs (queries) do
      if self.time >= o.retry then

        o.server = o.server + 1
        if o.server > #self.server then
          o.server = 1
          o.delay = o.delay + 1
          end

        if o.delay > #self.delays then
          print ('timeout')
          queries[question] = nil
          if not next (queries) then  self.active[id] = nil  end
          if not next (self.active) then  return nil  end
        else
          -- print ('retry', o.server, o.delay)
          self.socket[o.server]:send (o.packet)
          o.retry = self.time + self.delays[o.delay]
          end  end  end  end

  if next (self.active) then  return true  end
  return nil
  end


function resolver:lookup (qname, qtype, qclass)    -- - - - - - - - - -  lookup
  self:query (qname, qtype, qclass)
  while self:pulse () do  socket.select (self.socket, nil, 4)  end
  -- print (self.cache)
  return self:peek (qname, qtype, qclass)
  end


-- print ---------------------------------------------------------------- print


local hints = {    -- - - - - - - - - - - - - - - - - - - - - - - - - - - hints
  qr = { [0]='query', 'response' },
  opcode = { [0]='query', 'inverse query', 'server status request' },
  aa = { [0]='non-authoritative', 'authoritative' },
  tc = { [0]='complete', 'truncated' },
  rd = { [0]='recursion not desired', 'recursion desired' },
  ra = { [0]='recursion not available', 'recursion available' },
  z  = { [0]='(reserved)' },
  rcode = { [0]='no error', 'format error', 'server failure', 'name error',
            'not implemented' },

  type = dns.type,
  class = dns.class, }


local function hint (p, s)    -- - - - - - - - - - - - - - - - - - - - - - hint
  return (hints[s] and hints[s][p[s]]) or ''  end


function resolver.print (response)    -- - - - - - - - - - - - - resolver.print

  for s,s in pairs { 'id', 'qr', 'opcode', 'aa', 'tc', 'rd', 'ra', 'z',
		     'rcode', 'qdcount', 'ancount', 'nscount', 'arcount' } do
    print ( string.format ('%-30s', 'header.'..s),
            response.header[s], hint (response.header, s) )
    end

  for i,question in ipairs (response.question) do
    print (string.format ('question[%i].name         ', i), question.name)
    print (string.format ('question[%i].type         ', i), question.type)
    print (string.format ('question[%i].class        ', i), question.class)
    end

  local common = { name=1, type=1, class=1, ttl=1, rdlength=1, rdata=1 }
  local tmp
  for s,s in pairs {'answer', 'authority', 'additional'} do
    for i,rr in pairs (response[s]) do
      for j,t in pairs { 'name', 'type', 'class', 'ttl', 'rdlength' } do
        tmp = string.format ('%s[%i].%s', s, i, t)
        print (string.format ('%-30s', tmp), rr[t], hint (rr, t))
        end
      for j,t in pairs (rr) do
        if not common[j] then
          tmp = string.format ('%s[%i].%s', s, i, j)
          print (string.format ('%-30s  %s', tmp, t))
          end  end  end  end  end


-- module api ------------------------------------------------------ module api


local function resolve (func, ...)    -- - - - - - - - - - - - - - resolver_get
  dns._resolver = dns._resolver or dns.resolver ()
  return func (dns._resolver, ...)
  end


function dns.resolver ()    -- - - - - - - - - - - - - - - - - - - - - resolver

  -- this function seems to be redundant with resolver.new ()

  r = { active = {}, cache = {}, unsorted = {}, wanted = {}, yielded = {} }
  setmetatable (r, resolver)
  setmetatable (r.cache, cache_metatable)
  setmetatable (r.unsorted, { __mode = 'kv' })
  return r
  end


function dns.lookup (...)    -- - - - - - - - - - - - - - - - - - - - -  lookup
  return resolve (resolver.lookup, ...)  end


function dns.purge (...)    -- - - - - - - - - - - - - - - - - - - - - -  purge
  return resolve (resolver.purge, ...)  end

function dns.peek (...)    -- - - - - - - - - - - - - - - - - - - - - - -  peek
  return resolve (resolver.peek, ...)  end


function dns.query (...)    -- - - - - - - - - - - - - - - - - - - - - -  query
  return resolve (resolver.query, ...)  end


function dns:socket_wrapper_set (...)    -- - - - - - - - -  socket_wrapper_set
  return resolve (resolver.socket_wrapper_set, ...)  end


return dns
