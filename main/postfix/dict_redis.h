#ifndef _DICT_REDIS_H_INCLUDED_
#define _DICT_REDIS_H_INCLUDED_

/*++
/* NAME
/*	dict_redis 3h
/* SUMMARY
/*	dictionary manager interface to redis databases
/* SYNOPSIS
/*	#include <dict_redis.h>
/* DESCRIPTION
/* .nf

 /*
  * Utility library.
  */
#include <dict.h>

 /*
  * External interface.
  */
#define DICT_TYPE_REDIS	"redis"

extern DICT *dict_redis_open(const char *, int, int);

/* LICENSE
/* .ad
/* .fi
/*	The Secure Mailer license must be distributed with this software.
/* AUTHOR(S)
/*	Titus Jose
/*	titus.nitt@gmail.com
/*
/*	Updated by:
/*	Duncan Bellamy
/*	dunk@denkimushi.com
/*--*/

#endif
