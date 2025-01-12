// tbl.c: simpler wrappers for core table functions
//
// Written by Tony Finch <dot@dotat.at>
// You may do anything with this. It has no warranty.
// <http://creativecommons.org/publicdomain/zero/1.0/>

#include "platform.h"
#include "tbl.h"

void *
Tgetl(Tbl *tbl, const char *key, int len)
{
	const char *rkey = nil;
	void *rval = nil;
	return Tgetkv(tbl, key, len, &rkey, &rval) ? rval : nil;
}

void *
Tget(Tbl *tbl, const char *key)
{
	return Tgetl(tbl, key, strlen(key));
}

Tbl *
Tset(Tbl *tbl, const char *key, void *value)
{
	return Tsetl(tbl, key, strlen(key), value);
}

Tbl *
Tdell(Tbl *tbl, const char *key, int len)
{
	const char *rkey = nil;
	void *rval = nil;
	return Tdelkv(tbl, key, len, &rkey, &rval);
}

Tbl *
Tdel(Tbl *tbl, const char *key)
{
	return Tdell(tbl, key, strlen(key));
}

bool
Tnext(Tbl *tbl, const char **pkey, void **pvalue)
{
	int len = *pkey == nil ? 0 : strlen(*pkey);
	return Tnextl(tbl, pkey, &len, pvalue);
}

const char *
Tnxt(Tbl *tbl, const char *key)
{
	void *value = nil;
	Tnext(tbl, &key, &value);
	return key;
}
