#ifndef _LIBGEN_H
#define _LIBGEN_H	1

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef HAVE_BASENAME
char *basename(const char *path);
#endif

#ifndef HAVE_DIRNAME
char *dirname(const char *path);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _LIBGEN_H */
