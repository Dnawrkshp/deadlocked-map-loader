/***************************************************
 * FILENAME :		deadlocked.c
 * 
 * DESCRIPTION :
 * 		Implements all functions declared in include\*.h
 * 
 * NOTES :
 * 		Each offset is determined per app id.
 * 		This is to ensure compatibility between versions of Deadlocked/Gladiator.
 * 		
 * AUTHOR :			Daniel "Dnawrkshp" Gerendasy
 */

#include <tamtypes.h>
#include "common.h"

int (*sprintf)(char *, const char *, ...) = (int(*)(char *, const char *, ...))0x0011D920;

int (*sha1)(const void *, int, void *, int) = (int(*)(const void *, int, void *, int))0x01EB30C8;


unsigned int strlen(const char * str)
{
    return ((unsigned int(*)(const char *))0x0011AB04)(str);
}


void memcpy(void * dst, const void * src, int n)
{
    ((void (*)(void *, const void *, int))0x0011A370)(dst, src, n);
}

void memset(void * dst, int v, int n)
{
    ((void (*)(void *, int, int))0x0011A518)(dst, v, n);
}

int memcmp(void * a, void * b, int n)
{
    return ((int (*)(void *, void *, int))0x0011A2DC)(a, b, n);
}

int (*printf)(const char *, ...) = (int (*)(const char *, ...))0x0011D5D8;
