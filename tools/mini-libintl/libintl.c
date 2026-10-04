/*
 * Minimal libintl for builds without GNU gettext (used by the Windows
 * cross build). Supports one bound catalog per domain, plural forms and
 * the LANGUAGE/LC_ALL/LC_MESSAGES/LANG environment variables.
 *
 * This file is distributed under the same license as 7kaa (GPL v2+).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "libintl.h"
#undef setlocale

/* symbols probed by the gettext autoconf macros */
int _nl_msg_cat_cntr;
int *_nl_domain_bindings;
const char *_nl_expand_alias(const char *name) { (void)name; return NULL; }

struct catalog
{
	char domain[64];
	char dir[512];
	int loaded;
	unsigned char *data;
	size_t size;
	uint32_t count, orig_off, trans_off;
	int swap;
	const char *plural;   /* Plural-Forms expression, or NULL */
	unsigned long nplurals;
};

#define MAX_DOMAINS 8
static struct catalog catalogs[MAX_DOMAINS];
static char cur_domain[64] = "messages";

static struct catalog *find_catalog(const char *domain, int create)
{
	int i;
	for( i=0; i<MAX_DOMAINS; i++ )
		if( catalogs[i].domain[0] && !strcmp(catalogs[i].domain, domain) )
			return &catalogs[i];
	if( !create )
		return NULL;
	for( i=0; i<MAX_DOMAINS; i++ )
	{
		if( !catalogs[i].domain[0] )
		{
			strncpy(catalogs[i].domain, domain, sizeof(catalogs[i].domain)-1);
			strcpy(catalogs[i].dir, ".");
			return &catalogs[i];
		}
	}
	return NULL;
}

static uint32_t rd32(const struct catalog *c, uint32_t off)
{
	const unsigned char *p = c->data + off;
	if( c->swap )
		return ((uint32_t)p[0]<<24) | ((uint32_t)p[1]<<16) | ((uint32_t)p[2]<<8) | p[3];
	return ((uint32_t)p[3]<<24) | ((uint32_t)p[2]<<16) | ((uint32_t)p[1]<<8) | p[0];
}

static int load_file(struct catalog *c, const char *path)
{
	FILE *f = fopen(path, "rb");
	long len;
	if( !f )
		return 0;
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if( len < 28 )
	{
		fclose(f);
		return 0;
	}
	c->data = (unsigned char *)malloc(len);
	if( !c->data || fread(c->data, 1, len, f) != (size_t)len )
	{
		fclose(f);
		free(c->data);
		c->data = NULL;
		return 0;
	}
	fclose(f);
	c->size = len;
	if( c->data[0]==0xde && c->data[1]==0x12 && c->data[2]==0x04 && c->data[3]==0x95 )
		c->swap = 0;
	else if( c->data[0]==0x95 && c->data[1]==0x04 && c->data[2]==0x12 && c->data[3]==0xde )
		c->swap = 1;
	else
	{
		free(c->data);
		c->data = NULL;
		return 0;
	}
	c->count = rd32(c, 8);
	c->orig_off = rd32(c, 12);
	c->trans_off = rd32(c, 16);
	if( c->orig_off + c->count*8 > c->size || c->trans_off + c->count*8 > c->size )
	{
		free(c->data);
		c->data = NULL;
		return 0;
	}
	return 1;
}

static const char *lookup(struct catalog *c, const char *msgid, uint32_t *len);

static void parse_header(struct catalog *c)
{
	uint32_t len;
	const char *h = lookup(c, "", &len);
	const char *p;
	c->plural = NULL;
	c->nplurals = 2;
	if( !h )
		return;
	p = strstr(h, "Plural-Forms:");
	if( !p )
		return;
	p = strstr(p, "nplurals=");
	if( p )
		c->nplurals = strtoul(p+9, NULL, 10);
	p = strstr(p ? p : h, "plural=");
	if( p )
		c->plural = p+7;
}

static void load_catalog(struct catalog *c)
{
	const char *vars[] = { "LANGUAGE", "LC_ALL", "LC_MESSAGES", "LANG" };
	char lang[128], path[1024];
	int i;

	c->loaded = 1;
	for( i=0; i<4; i++ )
	{
		const char *v = getenv(vars[i]);
		char *list, *tok;
		if( !v || !v[0] )
			continue;
		strncpy(lang, v, sizeof(lang)-1);
		lang[sizeof(lang)-1] = 0;
		list = lang;
		while( (tok = strtok(list, ":")) )
		{
			char name[128], *cut;
			list = NULL;
			strcpy(name, tok);
			if( (cut = strchr(name, '@')) ) *cut = 0;
			if( (cut = strchr(name, '.')) ) *cut = 0;
			for( ;; )
			{
				snprintf(path, sizeof(path), "%s/%s/LC_MESSAGES/%s.mo", c->dir, name, c->domain);
				if( load_file(c, path) )
				{
					parse_header(c);
					_nl_msg_cat_cntr++;
					return;
				}
				if( !(cut = strchr(name, '_')) )
					break;
				*cut = 0;
			}
		}
		return; /* only the first set variable decides, like GNU gettext */
	}
}

static const char *lookup(struct catalog *c, const char *msgid, uint32_t *len)
{
	uint32_t lo = 0, hi = c->count;
	while( lo < hi )
	{
		uint32_t mid = (lo+hi)/2;
		uint32_t off = rd32(c, c->orig_off + mid*8 + 4);
		int r;
		if( off >= c->size )
			return NULL;
		r = strcmp(msgid, (const char *)c->data + off);
		if( r == 0 )
		{
			uint32_t tlen = rd32(c, c->trans_off + mid*8);
			uint32_t toff = rd32(c, c->trans_off + mid*8 + 4);
			if( toff + tlen >= c->size || tlen == 0 )
				return NULL;
			*len = tlen;
			return (const char *)c->data + toff;
		}
		if( r < 0 )
			hi = mid;
		else
			lo = mid+1;
	}
	return NULL;
}

/* ---- plural expression evaluator (C syntax subset used by Plural-Forms) ---- */

static const char *pp;
static unsigned long pn;
static unsigned long p_cond(void);

static void skip(void) { while( *pp==' ' || *pp=='\t' || *pp=='\n' || *pp=='\\' ) pp++; }

static unsigned long p_primary(void)
{
	unsigned long v = 0;
	skip();
	if( *pp == '(' )
	{
		pp++;
		v = p_cond();
		skip();
		if( *pp == ')' ) pp++;
	}
	else if( *pp == 'n' )
	{
		pp++;
		v = pn;
	}
	else if( *pp == '!' )
	{
		pp++;
		v = !p_primary();
	}
	else
		v = strtoul(pp, (char **)&pp, 10);
	return v;
}

static unsigned long p_mul(void)
{
	unsigned long v = p_primary(), r;
	for( ;; )
	{
		skip();
		if( *pp=='*' ) { pp++; v *= p_primary(); }
		else if( *pp=='/' ) { pp++; r = p_primary(); v = r ? v/r : 0; }
		else if( *pp=='%' ) { pp++; r = p_primary(); v = r ? v%r : 0; }
		else return v;
	}
}

static unsigned long p_add(void)
{
	unsigned long v = p_mul();
	for( ;; )
	{
		skip();
		if( *pp=='+' ) { pp++; v += p_mul(); }
		else if( *pp=='-' ) { pp++; v -= p_mul(); }
		else return v;
	}
}

static unsigned long p_rel(void)
{
	unsigned long v = p_add();
	for( ;; )
	{
		skip();
		if( pp[0]=='<' && pp[1]=='=' ) { pp+=2; v = v <= p_add(); }
		else if( pp[0]=='>' && pp[1]=='=' ) { pp+=2; v = v >= p_add(); }
		else if( pp[0]=='<' ) { pp++; v = v < p_add(); }
		else if( pp[0]=='>' ) { pp++; v = v > p_add(); }
		else return v;
	}
}

static unsigned long p_eq(void)
{
	unsigned long v = p_rel();
	for( ;; )
	{
		skip();
		if( pp[0]=='=' && pp[1]=='=' ) { pp+=2; v = v == p_rel(); }
		else if( pp[0]=='!' && pp[1]=='=' ) { pp+=2; v = v != p_rel(); }
		else return v;
	}
}

static unsigned long p_and(void)
{
	unsigned long v = p_eq();
	for( ;; )
	{
		skip();
		if( pp[0]=='&' && pp[1]=='&' ) { unsigned long r; pp+=2; r = p_eq(); v = v && r; }
		else return v;
	}
}

static unsigned long p_or(void)
{
	unsigned long v = p_and();
	for( ;; )
	{
		skip();
		if( pp[0]=='|' && pp[1]=='|' ) { unsigned long r; pp+=2; r = p_and(); v = v || r; }
		else return v;
	}
}

static unsigned long p_cond(void)
{
	unsigned long c = p_or(), a, b;
	skip();
	if( *pp != '?' )
		return c;
	pp++;
	a = p_cond();
	skip();
	if( *pp == ':' ) pp++;
	b = p_cond();
	return c ? a : b;
}

static unsigned long plural_index(struct catalog *c, unsigned long n)
{
	unsigned long i;
	if( !c->plural )
		return n != 1;
	pp = c->plural;
	pn = n;
	i = p_cond();
	return i < c->nplurals ? i : 0;
}

/* ---- public API ---- */

static struct catalog *ready_catalog(const char *domain)
{
	struct catalog *c = find_catalog(domain ? domain : cur_domain, 0);
	if( !c )
		return NULL;
	if( !c->loaded )
		load_catalog(c);
	return c->data ? c : NULL;
}

char *dcngettext(const char *domain, const char *msgid, const char *msgid_plural, unsigned long n, int category)
{
	struct catalog *c = ready_catalog(domain);
	(void)category;
	if( c && msgid )
	{
		uint32_t len;
		const char *t;
		if( msgid_plural )
		{
			/* plural entries are keyed by "msgid\0msgid_plural"; the msgid prefix is unique */
			uint32_t lo = 0, hi = c->count;
			while( lo < hi )
			{
				uint32_t mid = (lo+hi)/2;
				const char *orig = (const char *)c->data + rd32(c, c->orig_off + mid*8 + 4);
				int r = strcmp(msgid, orig);
				if( r == 0 )
				{
					unsigned long idx = plural_index(c, n);
					uint32_t tlen = rd32(c, c->trans_off + mid*8);
					t = (const char *)c->data + rd32(c, c->trans_off + mid*8 + 4);
					while( idx-- && (uint32_t)strlen(t)+1 < tlen )
					{
						tlen -= strlen(t)+1;
						t += strlen(t)+1;
					}
					if( t[0] )
						return (char *)t;
					break;
				}
				if( r < 0 ) hi = mid; else lo = mid+1;
			}
		}
		else if( (t = lookup(c, msgid, &len)) )
			return (char *)t;
	}
	if( msgid_plural && n != 1 )
		return (char *)msgid_plural;
	return (char *)msgid;
}

char *dcgettext(const char *domain, const char *msgid, int category)
{
	return dcngettext(domain, msgid, NULL, 1, category);
}

char *dgettext(const char *domain, const char *msgid)
{
	return dcngettext(domain, msgid, NULL, 1, 0);
}

char *gettext(const char *msgid)
{
	return dcngettext(NULL, msgid, NULL, 1, 0);
}

char *dngettext(const char *domain, const char *msgid, const char *msgid_plural, unsigned long n)
{
	return dcngettext(domain, msgid, msgid_plural, n, 0);
}

char *ngettext(const char *msgid, const char *msgid_plural, unsigned long n)
{
	return dcngettext(NULL, msgid, msgid_plural, n, 0);
}

char *textdomain(const char *domain)
{
	if( domain && domain[0] )
	{
		strncpy(cur_domain, domain, sizeof(cur_domain)-1);
		find_catalog(cur_domain, 1);
	}
	return cur_domain;
}

char *bindtextdomain(const char *domain, const char *dirname)
{
	struct catalog *c = find_catalog(domain, 1);
	if( !c )
		return NULL;
	if( dirname )
	{
		strncpy(c->dir, dirname, sizeof(c->dir)-1);
		free(c->data);
		c->data = NULL;
		c->loaded = 0;
	}
	return c->dir;
}

char *bind_textdomain_codeset(const char *domain, const char *codeset)
{
	(void)domain;
	(void)codeset;
	return (char *)"UTF-8";
}

char *libintl_setlocale(int category, const char *locale)
{
#ifdef MINI_LIBINTL_LC_MESSAGES
	static char messages_locale[64] = "C";
	if( category == LC_MESSAGES )
	{
		if( locale )
		{
			strncpy(messages_locale, locale[0] ? locale : "C", sizeof(messages_locale)-1);
			messages_locale[sizeof(messages_locale)-1] = 0;
		}
		return messages_locale;
	}
#endif
	return setlocale(category, locale);
}
