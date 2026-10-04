/*
 * Minimal libintl for builds without GNU gettext (used by the Windows
 * cross build). Reads GNU .mo catalogs; messages are returned as stored
 * in the catalog (UTF-8 for 7kaa's catalogs).
 */
#ifndef MINI_LIBINTL_H
#define MINI_LIBINTL_H

#include <locale.h>

/* Windows C runtimes have no LC_MESSAGES; handle it like GNU libintl */
#ifndef LC_MESSAGES
#define LC_MESSAGES 1729
#define MINI_LIBINTL_LC_MESSAGES 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

char *libintl_setlocale(int category, const char *locale);
#undef setlocale
#define setlocale libintl_setlocale

char *gettext(const char *msgid);
char *dgettext(const char *domain, const char *msgid);
char *dcgettext(const char *domain, const char *msgid, int category);
char *ngettext(const char *msgid, const char *msgid_plural, unsigned long n);
char *dngettext(const char *domain, const char *msgid, const char *msgid_plural, unsigned long n);
char *dcngettext(const char *domain, const char *msgid, const char *msgid_plural, unsigned long n, int category);
char *textdomain(const char *domain);
char *bindtextdomain(const char *domain, const char *dirname);
char *bind_textdomain_codeset(const char *domain, const char *codeset);

#ifdef __cplusplus
}
#endif

#endif
