-----------------
-- Microlight - a very compact Lua utilities module
--
-- Steve Donovan, 2012; License MIT
-- @module ml

local ml = {}

--- String utilties.
-- @section string

--- split a string into a list of strings separated by a delimiter.
-- @param s The input string
-- @param re A Lua string pattern; defaults to '%s+'
-- @param n optional maximum number of splits
-- @return a list
function ml.split(s,re,n)
    local find,sub,append = string.find, string.sub, table.insert
    local i1,ls = 1,{}
    if not re then re = '%s+' end
    if re == '' then return {s} end
    while true do
        local i2,i3 = find(s,re,i1)
        if not i2 then
            local last = sub(s,i1)
            if last ~= '' then append(ls,last) end
            if #ls == 1 and ls[1] == '' then
                return {}
            else
                return ls
            end
        end
        append(ls,sub(s,i1,i2-1))
        if n and #ls == n then
            ls[#ls] = sub(s,i1)
            return ls
        end
        i1 = i3+1
    end
end

--- escape any 'magic' characters in a string
-- @param s The input string
-- @return an escaped string
function ml.escape(s)
    return (s:gsub('[%-%.%+%[%]%(%)%$%^%%%?%*]','%%%1'))
end

--- expand a string containing any ${var} or $var.
-- @param s the string
-- @param subst either a table or a function (as in `string.gsub`)
-- @return expanded string
function ml.expand (s,subst)
    local res = s:gsub('%${([%w_]+)}',subst)
    return (res:gsub('%$([%w_]+)',subst))
end

--- return the contents of a file as a string
-- @param filename The file path
-- @param is_bin open in binary mode, default false
-- @return file contents
function ml.readfile(filename,is_bin)
    local mode = is_bin and 'b' or ''
    local f,err = io.open(filename,'r'..mode)
    if not f then return nil,err end
    local res,err = f:read('*a')
    f:close()
    if not res then return nil,err end
    return res
end

--- File and Path functions
-- @section file

--~ exists(filename)
--- Does a file exist?
-- @param filename a file path
-- @return the file path, otherwise nil
-- @usage exists 'readme' or exists 'readme.txt' or exists 'readme.md'
function ml.exists (filename)
    local f = io.open(filename)
    if not f then
        return nil
    else
        f:close()
        return filename
    end
end

local sep, other_sep = package.config:sub(1,1),'/'


--- split a file path.
-- if there's no directory part, the first value will be the empty string
-- @param P A file path
-- @return the directory part
-- @return the file part
function ml.splitpath(P)
    local i = #P
    local ch = P:sub(i,i)
    while i > 0 and ch ~= sep and ch ~= other_sep do
        i = i - 1
        ch = P:sub(i,i)
    end
    if i == 0 then
        return '',P
    else
        return P:sub(1,i-1), P:sub(i+1)
    end
end

--- given a path, return the root part and the extension part.
-- if there's no extension part, the second value will be empty
-- @param P A file path
-- @return the name part
-- @return the extension
function ml.splitext(P)
    local i = #P
    local ch = P:sub(i,i)
    while i > 0 and ch ~= '.' do
        if ch == sep or ch == other_sep then
            return P,''
        end
        i = i - 1
        ch = P:sub(i,i)
    end
    if i == 0 then
        return P,''
    else
        return P:sub(1,i-1),P:sub(i)
    end
end

--- Extended table functions.
-- 'list' here is shorthand for 'list-like table'; these functions
-- only operate over the numeric `1..#t` range of a table and are
-- particularly efficient for this purpose.
-- @section table

local function quote (v)
    if type(v) == 'string' then
        return ('%q'):format(v)
    else
        return tostring(v)
    end
end

local tbuff
function tbuff (t,buff,k)
    buff[k] = "{"
    k = k + 1
    for key,value in pairs(t) do
        key = quote(key)
        if type(value) ~= 'table' then
            value = quote(value)
            buff[k] = ('[%s]=%s'):format(key,value)
            k = k + 1
            if buff.limit and k > buff.limit then
                buff[k] = "..."
                error("buffer overrun")
            end
        else
            if not buff.tables then buff.tables = {} end
            if not buff.tables[value] then
                k = tbuff(value,buff,k)
                buff.tables[value] = true
            else
                buff[k] = "<cycle>"
                k = k + 1
            end
        end
        buff[k] = ","
        k = k + 1
    end
    if buff[k-1] == "," then k = k - 1 end
    buff[k] = "}"
    k = k + 1
    return k
end

--- return a string representation of a Lua table.
-- Cycles are detected, and a limit on number of items can be imposed.
-- @param t the table
-- @param limit the limit on items, default 1000
-- @return a string
function ml.tstring (t,limit)
    local buff = {limit = limit or 1000}
    pcall(tbuff,t,buff,1)
    return table.concat(buff)
end

--- dump a Lua table to a file object.
-- @param t the table
-- @param f the file object (anything supporting f.write)
function ml.tdump(t,...)
    local f = select('#',...) > 0 and select(1,...) or io.stdout
    f:write(ml.tstring(t),'\n')
end

--- map a function over a list.
-- The output must always be the same length as the input, so
-- any `nil` values are mapped to `false`.
-- @param f a function of one or more arguments
-- @param t the table
-- @param ... any extra arguments to the function
-- @return a list with elements `f(t[i])`
function ml.imap(f,t,...)
    f = ml.function_arg(f)
    local res = {}
    for i = 1,#t do
        local val = f(t[i],...)
        if val == nil then val = false end
        res[i] = val
    end
    return res
end

--- filter a list using a predicate.
-- @param t a table
-- @param pred the predicate function
-- @param ... any extra arguments to the predicate
-- @return a list such that `pred(t[i])` is true
function ml.ifilter(t,pred,...)
    local res,k = {},1
    pred = ml.function_arg(pred)
    for i = 1,#t do
        if pred(t[i],...) then
            res[k] = t[i]
            k = k + 1
        end
    end
    return res
end

--- find an item in a list using a predicate.
-- @param t the list
-- @param pred a function of at least one argument
-- @param ... any extra arguments
-- @return the item value
function ml.ifind(t,pred,...)
    pred = ml.function_arg(pred)
    for i = 1,#t do
        if pred(t[i],...) then
            return t[i]
        end
    end
end

--- return the index of an item in a list.
-- @param t the list
-- @param value item value
-- @return index, otherwise `nil`
function ml.index (t,value)
    for i = 1,#t do
        if t[i] == value then return i end
    end
end

--- return a slice of a list.
-- Like string.sub, the end index may be negative.
-- @param t the list
-- @param i1 the start index
-- @param i2 the end index, default #t
function ml.sub(t,i1,i2)
    if not i2 or i2 > #t then
        i2 = #t
    elseif i2 < 0 then
        i2 = #t + i2 + 1
    end
    local res,k = {},1
    for i = i1,i2 do
        res[k] = t[i]
        k = k + 1
    end
    return res
end

--- map a function over a Lua table.
-- @param f a function of one or more arguments
-- @param t the table
-- @param ... any optional arguments to the function
function ml.tmap(f,t,...)
    f = ml.function_arg(f)
    local res = {}
    for k,v in pairs(t) do
        res[k] = f(v,...)
    end
    return res
end

--- filter a table using a predicate.
-- @param t a table
-- @param pred the predicate function
-- @param ... any extra arguments to the predicate
-- @usage tfilter({a=1,b='boo'},tonumber) == {a=1}
function ml.tfilter (t,pred,...)
    local res = {}
    pred = ml.function_arg(pred)
    for k,v in pairs(t) do
        if pred(v,...) then
            res[k] = v
        end
    end
    return res
end

--- add the key/value pairs of `other` to `t`.
-- For sets, this is their union. For the same keys,
-- the values from the first table will be overwritten
-- @param t table to be updated
-- @param other table
-- @return the updated table
function ml.update(t,other)
    for k,v in pairs(other) do
        t[k] = v
    end
    return t
end

--- extend a list using values from another.
-- @param t the list to be extended
-- @param other a list
-- @return the extended list
function ml.extend(t,other)
    local n = #t
    for i = 1,#other do
        t[n+i] = other[i]
    end
    return t
end

--- make a set from a list.
-- @param t a list of values
-- @return a table where the keys are the values
-- @usage set{'one','two'} == {one=true,two=true}
function ml.set(t)
    local res = {}
    for i = 1,#t do
        res[t[i]] = true
    end
    return res
end

--- extract the keys of a table as a list.
-- This is the opposite operation to tset
-- @param t a table
-- @param a list of keys
function ml.keys(t)
    local res,k = {},1
    for key in pairs(t) do
        res[k] = key
        k = k + 1
    end
    return res
end

--- is `other` a subset of `t`?
-- @param t a set
-- @param other a possible subset
-- @return true or false
function ml.subset(t,other)
    for k,v in pairs(other) do
        if t[k] ~= v then return false end
    end
    return true
end

--- are these two tables equal?
-- This is shallow equality.
-- @param t a table
-- @param other a table
-- @return true or false
function ml.tequal(t,other)
    return ml.subset(t,other) and ml.subset(other,t)
end

--- the intersection of two tables.
-- Works as expected for sets, otherwise note that the first
-- table's values are preseved
-- @param t a table
-- @param other a table
-- @return the intersection of the tables
function ml.intersect(t,other)
    local res = {}
    for k,v in pairs(t) do
        if other[k] then
            res[k] = v
        end
    end
    return res
end

--- collect the values of an iterator into a list.
-- @param iter a single or double-valued iterator
-- @param count an optional number of values to collect
-- @return a list of values.
-- @usage collect(ipairs{10,20}) == {{1,10},{2,20}}
function ml.collect (iter, count)
    local res,k = {},1
    local v1,v2 = iter()
    local dbl = v2 ~= nil
    while v1 do
        if dbl then v1 = {v1,v2} end
        res[k] = v1
        k = k + 1
        if count and k > count then break end
        v1,v2 = iter()
    end
    return res
end

--- Functional helpers.
-- @section function

--- create a function which will throw an error on failure.
-- @param f a function that returns nil,err if it fails
-- @return an equivalent function that raises an error
function ml.throw(f)
    f = ml.function_arg(f)
    return function(...)
        local res,err = f(...)
        if err then error(err) end
        return res
    end
end

--- create a function which will never throw an error.
-- This is the opposite situation to throw; if the
-- original function throws an error e, then this
-- function will return nil,e.
-- @param f a function which can throw an error
-- @return a function which returns nil,error when it fails
function ml.safe(f)
    f = ml.function_arg(f)
    return function(...)
        local ok,r1,r2,r3 = pcall(f,...)
        if ok then return r1,r2,r3
        else
            return nil,r1
        end
    end
end
--memoize(f)

--- bind the value `v` to the first argument of function `f`.
-- @param f a function of at least one argument
-- @param v a value
-- @return a function of one less argument
-- @usage (bind1(string.match,'hello')('^hell') == 'hell'
function ml.bind1(f,v)
    f = ml.function_arg(f)
    return function(...)
        return f(v,...)
    end
end

--- compose two functions.
-- For instance, `printf` can be defined as `compose(io.write,string.format)`
-- @param f1 a function
-- @param f2 a function
-- @return f1(f2(...))
function ml.compose(f1,f2)
    f1 = ml.function_arg(f1)
    f2 = ml.function_arg(f2)
    return function(...)
        return f1(f2(...))
    end
end

--- is the object either a function or a callable object?.
-- @param obj Object to check.
-- @return true if callable
function ml.callable (obj)
    return type(obj) == 'function' or getmetatable(obj) and getmetatable(obj).__call
end

function ml.function_arg(f)
    assert(ml.callable(f),"expecting a function or callable object")
    return f
end

--- Classes.
-- @section class

--- create a class with an optional base class.
-- The resulting table has a new() function for invoking
-- the constructor, which must be named `_init`. If the base
-- class has a constructor, you can call it as the `super()` method.
-- The `__tostring` metamethod is also inherited, but others need
-- to be brought in explicitly.
-- @param base optional base class
-- @return the metatable representing the class
function ml.class(base)
    local klass, base_ctor = {}
    klass.__index = klass
    if base then
        setmetatable(klass,base)
        klass._base = base
        base_ctor = rawget(base,'_init')
        klass.__tostring = base.__tostring
    end
    function klass.new(...)
        local self = setmetatable({},klass)
        if rawget(klass,'_init') then
            klass.super = base_ctor -- make super available for ctor
            klass._init(self,...)
        elseif base_ctor then -- call base ctor automatically
            base_ctor(self,...)
        end
        return self
    end
    return klass
end

--- is an object derived from a class?
-- @param self the object
-- @param klass a class created with `class`
-- @return true or false
function ml.is_a(self,klass)
    local m = getmetatable(self)
    if not m then return false end --*can't be an object!
    while m do
        if m == klass then return true end
        m = rawget(m,'_base')
    end
    return false
end

local _type = type

--- extended type of an object.
-- The type of a table is its metatable, otherwise works like standard type()
-- @param obj a value
-- @return the type, either a string or the metatable
function ml.type (obj)
    if _type(obj) == 'table' then
        return getmetatable(obj) or 'table'
    else
        return _type(obj)
    end
end

return ml
