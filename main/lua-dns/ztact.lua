

-- public domain 20080410 lua@ztact.com


pcall (require, 'lfs')      -- lfs may not be installed/necessary.
pcall (require, 'pozix')    -- pozix may not be installed/necessary.


local getfenv, ipairs, next, pairs, pcall, require, select, tostring, type =
      getfenv, ipairs, next, pairs, pcall, require, select, tostring, type
local unpack, xpcall =
      unpack, xpcall

local io, lfs, os, string, table, pozix = io, lfs, os, string, table, pozix

local assert, print = assert, print

local error		= error


module ((...) or 'ztact')    ------------------------------------- module ztact


-- dir -------------------------------------------------------------------- dir


function dir (path)    -- - - - - - - - - - - - - - - - - - - - - - - - - - dir
  local it = lfs.dir (path)
  return function ()
    repeat
      local dir = it ()
      if dir ~= '.' and dir ~= '..' then  return dir  end
    until not dir
    end  end


function is_file (path)    -- - - - - - - - - - - - - - - - - -  is_file (path)
  local mode = lfs.attributes (path, 'mode')
  return mode == 'file' and path
  end


-- network byte ordering -------------------------------- network byte ordering


function htons (word)    -- - - - - - - - - - - - - - - - - - - - - - - - htons
  return (word-word%0x100)/0x100, word%0x100
  end


-- pcall2 -------------------------------------------------------------- pcall2


getfenv ().pcall = pcall    -- store the original pcall as ztact.pcall


local argc, argv, errorhandler, pcall2_f


local function _pcall2 ()    -- - - - - - - - - - - - - - - - - - - - - _pcall2
  local tmpv = argv
  argv = nil
  return pcall2_f (unpack (tmpv, 1, argc))
  end


function seterrorhandler (func)    -- - - - - - - - - - - - - - seterrorhandler
  errorhandler = func
  end


function pcall2 (f, ...)    -- - - - - - - - - - - - - - - - - - - - - - pcall2

  pcall2_f = f
  argc = select ('#', ...)
  argv = { ... }

  if not errorhandler then
    local debug = require ('debug')
    errorhandler = debug.traceback
    end

  return xpcall (_pcall2, errorhandler)
  end


function append (t, ...)    -- - - - - - - - - - - - - - - - - - - - - - append
  local insert = table.insert
  for i,v in ipairs {...} do
    insert (t, v)
    end  end


function print_r (d, indent)    -- - - - - - - - - - - - - - - - - - -  print_r
  local rep = string.rep ('  ', indent or 0)
  if type (d) == 'table' then
    for k,v in pairs (d) do
      if type (v) == 'table' then
        io.write (rep, k, '\n')
        print_r (v, (indent or 0) + 1)
      else  io.write (rep, k, ' = ', tostring (v), '\n')  end
      end
  else  io.write (d, '\n')  end
  end


function tohex (s)    -- - - - - - - - - - - - - - - - - - - - - - - - -  tohex
  return string.format (string.rep ('%02x ', #s), string.byte (s, 1, #s))
  end


function tostring_r (d, indent, tab0)    -- - - - - - - - - - - - -  tostring_r

  tab1 = tab0 or {}
  local rep = string.rep ('  ', indent or 0)
  if type (d) == 'table' then
    for k,v in pairs (d) do
      if type (v) == 'table' then
        append (tab1, rep, k, '\n')
        tostring_r (v, (indent or 0) + 1, tab1)
      else  append (tab1, rep, k, ' = ', tostring (v), '\n')  end
      end
  else  append (tab1, d, '\n')  end

  if not tab0 then  return table.concat (tab1)  end
  end


-- queue manipulation -------------------------------------- queue manipulation


-- Possible queue states.  1 (i.e. queue.p[1]) is head of queue.
--
-- 1..2
-- 3..4  1..2
-- 3..4  1..2  5..6
-- 1..2        5..6
--             1..2


local function print_queue (queue, ...)    -- - - - - - - - - - - - print_queue
  for i=1,10 do  io.write ((queue[i]   or '.')..' ')  end
  io.write ('\t')
  for i=1,6  do  io.write ((queue.p[i] or '.')..' ')  end
  print (...)
  end


function dequeue (queue)    -- - - - - - - - - - - - - - - - - - - - -  dequeue

  local p = queue.p
  if not p and queue[1] then  queue.p = { 1, #queue }  p = queue.p  end

  if not p[1] then  return nil  end

  local element = queue[p[1]]
  queue[p[1]] = nil

  if p[1] < p[2] then  p[1] = p[1] + 1

  elseif p[4] then  p[1], p[2], p[3], p[4]  =  p[3], p[4], nil, nil

  elseif p[5] then  p[1], p[2], p[5], p[6]  =  p[5], p[6], nil, nil

  else  p[1], p[2]  =  nil, nil  end

  print_queue (queue, '  de '..element)
  return element
  end


function enqueue (queue, element)    -- - - - - - - - - - - - - - - - - enqueue

  local p = queue.p
  if not p then  queue.p = {}  p = queue.p  end

  if p[5] then    -- p3..p4 p1..p2 p5..p6
    p[6] = p[6]+1
    queue[p[6]] = element

  elseif p[3] then    -- p3..p4 p1..p2

    if p[4]+1 < p[1] then
      p[4] = p[4] + 1
      queue[p[4]] = element

    else
      p[5] = p[2]+1
      p[6], queue[p[5]] = p[5], element
      end

  elseif p[1] then    -- p1..p2
    if p[1] == 1 then
      p[2] = p[2] + 1
      queue[p[2]] = element

    else
        p[3], p[4], queue[1] = 1, 1, element
        end

  else    -- empty queue
    p[1], p[2], queue[1] = 1, 1, element
    end

  print_queue (queue, '     '..element)
  end


local function test_queue ()
  t = {}
  enqueue (t, 1)
  enqueue (t, 2)
  enqueue (t, 3)
  enqueue (t, 4)
  enqueue (t, 5)
  dequeue (t)
  dequeue (t)
  enqueue (t, 6)
  enqueue (t, 7)
  enqueue (t, 8)
  enqueue (t, 9)
  dequeue (t)
  dequeue (t)
  dequeue (t)
  dequeue (t)
  enqueue (t, 'a')
  dequeue (t)
  enqueue (t, 'b')
  enqueue (t, 'c')
  dequeue (t)
  dequeue (t)
  dequeue (t)
  dequeue (t)
  dequeue (t)
  enqueue (t, 'd')
  dequeue (t)
  dequeue (t)
  dequeue (t)
  end


-- test_queue ()


function queue_len (queue)
  end


function queue_peek (queue)
  end


-- tree manipulation ---------------------------------------- tree manipulation


function set (parent, ...)    --- - - - - - - - - - - - - - - - - - - - - - set

  -- print ('set', ...)

  local len = select ('#', ...)
  local key, value = select (len-1, ...)
  local cutpoint, cutkey

  for i=1,len-2 do

    local key = select (i, ...)
    local child = parent[key]

    if value == nil then
      if child == nil then  return
      elseif next (child, next (child)) then  cutpoint = nil  cutkey = nil
      elseif cutpoint == nil then  cutpoint = parent  cutkey = key  end

    elseif child == nil then  child = {}  parent[key] = child  end

    parent = child
    end

  if value == nil and cutpoint then  cutpoint[cutkey] = nil
  else  parent[key] = value  return value  end
  end


function get (parent, ...)    --- - - - - - - - - - - - - - - - - - - - - - get
  local len = select ('#', ...)
  for i=1,len do
    parent = parent[select (i, ...)]
    if parent == nil then  break  end
    end
  return parent
  end


-- misc ------------------------------------------------------------------ misc


function find (path, ...)    --------------------------------------------- find

  local dirs, operators = { path }, {...}
  for operator in ivalues (operators) do
    if not operator (path) then  break  end  end

  while next (dirs) do
    local parent = table.remove (dirs)
    for child in assert (pozix.opendir (parent)) do
      if  child  and  child ~= '.'  and  child ~= '..'  then
        local path = parent..'/'..child
	if pozix.stat (path, 'is_dir') then  table.insert (dirs, path)  end
        for operator in ivalues (operators) do
          if not operator (path) then  break  end  end
        end  end  end  end


function ivalues (t)    ----------------------------------------------- ivalues
  local i = 0
  return function ()  if t[i+1] then  i = i + 1  return t[i]  end  end
  end


function lson_encode (mixed, f, indent, indents)    --------------- lson_encode


  local capture
  if not f then
    capture = {}
    f = function (s)  append (capture, s)  end
    end

  indent = indent or 0
  indents = indents or {}
  indents[indent] = indents[indent] or string.rep (' ', 2*indent)

  local type = type (mixed)

  if type == 'number' then f (mixed)

  else if type == 'string' then f (string.format ('%q', mixed))

  else if type == 'table' then
    f ('{')
    for k,v in pairs (mixed) do
      f ('\n')
      f (indents[indent])
      f ('[')  f (lson_encode (k))  f ('] = ')
      lson_encode (v, f, indent+1, indents)
      f (',')
      end 
    f (' }')
    end  end  end

  if capture then  return table.concat (capture)  end
  end


function timestamp (time)    ---------------------------------------- timestamp
  return os.date ('%Y%m%d.%H%M%S', time)
  end


function values (t)    ------------------------------------------------- values
  local k, v
  return function ()  k, v = next (t, k)  return v  end
  end
