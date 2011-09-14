#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "apk_defines.h"
#include "apk_version.h"
#include "apk_blob.h"

#define LIBNAME "apk"

int apk_verbosity;
unsigned int apk_flags;


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


static const luaL_reg R[] = {
	{"version_validate",	Pversion_validate},
	{"version_compare",	Pversion_compare},
	{"version_is_less",	Pversion_is_less},
	{NULL,		NULL}
};

LUALIB_API int luaopen_apk(lua_State *L)
{
	luaL_register(L, LIBNAME, R);
	lua_pushliteral(L, "version");
	lua_pushliteral(L, APK_VERSION);
	lua_settable(L, -3);
	return 1;
}

