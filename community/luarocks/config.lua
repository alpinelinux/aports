rocks_trees = {
   -- User-local Lua and Lua/C modules.
   { name = 'user',
     root = home..'/.luarocks' },
   -- System-wide Lua and Lua/C modules for specific Lua version installed by apk.
   { name = 'distro-modules',
     root = '/usr' },
   -- System-wide Lua modules compatible with Lua 5.1-5.3 installed by apk.
   { name = 'distro-modules-common',
     root = '/usr',
     lua_dir = '/usr/share/lua/common',
     rocks_dir = '/usr/lib/luarocks/rocks-common' },
   -- System-wide Lua and Lua/C modules installed by user.
   { name = 'system',
     root = '/usr/local' },
}
deps_mode = 'all'
