#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "apk_blob.h"
#include "apk_database.h"
#include "apk_defines.h"
#include "apk_version.h"

#define LIBNAME "apk"
#define APKDB_META "apk_database"

struct flagmap {
       const char *name;
       int flag;
};

struct flagmap opendb_flagmap[] = {
       {"read",                APK_OPENF_READ},
       {"write",               APK_OPENF_WRITE},
       {"create",              APK_OPENF_CREATE},
       {"no_installed",        APK_OPENF_NO_INSTALLED},
       {"no_scripts",          APK_OPENF_NO_SCRIPTS},
       {"no_world",            APK_OPENF_NO_WORLD},
       {"no_sys_repos",        APK_OPENF_NO_SYS_REPOS},
       {"no_installed_repo",   APK_OPENF_NO_INSTALLED_REPO},
       {"no_repos",            APK_OPENF_NO_REPOS},
       {"no_state",            APK_OPENF_NO_STATE},
       {"no_scripts",          APK_OPENF_NO_SCRIPTS},
       {"no_world",            APK_OPENF_NO_WORLD},
       {NULL, 0}
};

static apk_blob_t check_blob(lua_State *L, int index)
{
	apk_blob_t blob;
	blob.ptr = (char *)luaL_checklstring(L, index, (size_t *)&blob.len);
	return blob;
}

/* version_validate(verstr) */
/* returns boolean */
static int Pversion_validate(lua_State *L)
{
	apk_blob_t ver = check_blob(L, 1);
	lua_pushboolean(L, apk_version_validate(ver));
	return 1;
}

/* version_compare(verstr1, verstr2)
   returns either '<', '=' or '>'
*/
static int Pversion_compare(lua_State *L)
{
	apk_blob_t a, b;
	a = check_blob(L, 1);
	b = check_blob(L, 2);
	lua_pushstring(L, apk_version_op_string(apk_version_compare_blob(a, b)));
	return 1;
}

/* version_is_less(verstr1, verstr2)
   returns either '<', '=' or '>'
*/
static int Pversion_is_less(lua_State *L)
{
	apk_blob_t a, b;
	a = check_blob(L, 1);
	b = check_blob(L, 2);
	lua_pushboolean(L, apk_version_compare_blob(a, b) == APK_VERSION_LESS);
	return 1;
}

//static getfield(lua_State *L, const char *key)
//{
static const char *get_opt_string_field(lua_State *L, int index,
				        const char *key, const char *def)
{
	const char *value;
	lua_getfield(L, index, key);
	value = luaL_optstring(L, -1, def);
	lua_pop(L, 1);
	return value;
}

static int get_opt_int_field(lua_State *L, int index, const char *key, int def)
{
	int value;
	lua_getfield(L, index, key);
	value = luaL_optinteger(L, -1, def);
	lua_pop(L, 1);
	return value;
}

static int get_boolean_field(lua_State *L, int index, const char *key)
{
	int value;
	lua_getfield(L, index, key);
	value = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return value;
}

static int get_dbopts(lua_State *L, int i, struct apk_db_options *o)
{
	struct flagmap *f;
	o->root = (char *)get_opt_string_field(L, i, "root", NULL);
	o->repositories_file = (char *)get_opt_string_field(L, i, "repositories_file", NULL);
	o->keys_dir = (char *)get_opt_string_field(L, i, "keys_dir", NULL);
	o->lock_wait = get_opt_int_field(L, i, "lock_wait", 0);
	for (f = opendb_flagmap; f->name != NULL; f++)
		if (get_boolean_field(L, i, f->name))
			o->open_flags |= f->flag;
	return 0;
}

static struct apk_database *checkdb(lua_State *L, int index)
{
	struct apk_database *db;
	luaL_checktype(L, index, LUA_TUSERDATA);
	db = (struct apk_database  *) luaL_checkudata(L, index, APKDB_META);
	if (db == NULL)
		luaL_typerror(L, index, APKDB_META);
	return db;
}

static int Papk_db_open(lua_State *L)
{
	struct apk_db_options opts;
	struct apk_database *db;
	int r;

	memset(&opts, 0, sizeof(opts));
	list_init(&opts.repository_list);
	if (lua_istable(L, 1))
		get_dbopts(L, 1, &opts);

	db = lua_newuserdata(L, sizeof(struct apk_database));
	luaL_getmetatable(L, APKDB_META);
	lua_setmetatable(L, -2);

	r = apk_db_open(db, &opts);
	if (r != 0)
		luaL_error(L, "apk_db_open() failed");
	return 1;
}

static int Papk_db_close(lua_State *L)
{
	struct apk_database *db = checkdb(L, 1);
	apk_db_close(db);
	return 0;
}


static const luaL_reg reg_apk_methods[] = {
	{"version_validate",	Pversion_validate},
	{"version_compare",	Pversion_compare},
	{"version_is_less",	Pversion_is_less},
	{"db_open",		Papk_db_open},
	{NULL,		NULL}
};

static const luaL_reg reg_apk_db_meta_methods[] = {
	{"__gc",	Papk_db_close},
	{NULL, NULL}
};

LUALIB_API int luaopen_apk(lua_State *L)
{
	luaL_register(L, LIBNAME, reg_apk_methods);
	lua_pushliteral(L, "version");
	lua_pushliteral(L, APK_VERSION);
	lua_settable(L, -3);

	luaL_newmetatable(L, APKDB_META);
	luaL_register(L, NULL, reg_apk_db_meta_methods);
	lua_pushliteral(L, "__index");
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);

	lua_pushliteral(L, "__metatable");
	lua_pushvalue(L, -3);
	lua_rawset(L, -3);
	lua_pop(L, 1);

	return 1;
}

