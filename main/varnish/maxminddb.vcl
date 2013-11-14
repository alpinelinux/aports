# Maxmind GeoIP2 API bindings
# Copyright (c) 2013 Timo Teräs
# GPLv2 applies

# Use from your VCL by adding to your vcl_recv():
#  call maxminddb_lookup;

# You also need to modify the cc_command parameter to include
# Maxmind database library (-lmaxminddb). In Alpine Linux this is
# done by uncomminting the relevant VARNISHD_PLUGIN_CFLAGS line in
# /etc/conf.d/varnishd.

C{

#include <maxminddb.h>
#include <string.h>

static MMDB_s mmdb;

static void __attribute__((constructor)) __geoip_on_load(void)
{
	if (MMDB_open("/var/lib/libmaxminddb/GeoLite2-City.mmdb", MMDB_MODE_MMAP, &mmdb) != MMDB_SUCCESS)
		mmdb.filename = NULL;
}

static void __attribute__((destructor)) __geoip_on_unload(void)
{
	if (mmdb.filename != NULL)
		MMDB_close(&mmdb);
}

static inline void __geoip_set_header(struct sess *sp, const char *hdr, MMDB_lookup_result_s *res, const char * const* path)
{
	MMDB_entry_data_s entry;
	const char *value = "FAIL";
	char buf[64];
	int s;

	if (!res->found_entry)
		goto set_and_exit;

	s = MMDB_aget_value(&res->entry, &entry, path);
	if (s != MMDB_SUCCESS || !entry.has_data)
		goto set_and_exit;

	if (entry.type != MMDB_DATA_TYPE_UTF8_STRING || entry.data_size+1 >= sizeof(buf))
		goto set_and_exit;

	memcpy(buf, entry.utf8_string, entry.data_size);
	buf[entry.data_size] = 0;
	value = buf;

set_and_exit:
	VRT_SetHdr(sp, HDR_REQ, hdr, value, vrt_magic_string_end);
}

static void __geoip_set_headers(struct sess *sp)
{
	static const char *continent_path[] = { "continent", "code", NULL };
	static const char *country_path[] = { "country", "iso_code", NULL };
	MMDB_lookup_result_s res = {0};
	int e = MMDB_FILE_OPEN_ERROR;

	if (mmdb.filename != NULL)
		res = MMDB_lookup_sockaddr(&mmdb, (struct sockaddr*) VRT_r_client_ip(sp), &e);

	if (e != MMDB_SUCCESS)
		VRT_SetHdr(sp, HDR_REQ, "\016X-GeoIP-Error:", MMDB_strerror(e), vrt_magic_string_end);

	__geoip_set_header(sp, "\022X-GeoIP-Continent:", &res, continent_path);
	__geoip_set_header(sp, "\020X-GeoIP-Country:", &res, country_path);
}

}C

sub maxminddb_lookup {
	C{ __geoip_set_headers(sp); }C
}
