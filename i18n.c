/*
 * i18n.c
 *
 * Copyright (C) Johan Malm 2018
 */

#include <string.h>
#include <sys/stat.h>

#include "i18n.h"
#include "util.h"
#include "hashmap.h"

static struct hashmap map;

struct map_entry {
	struct hashmap_entry ent;
	char key[FLEX_ARRAY];
};

static int cmp(const struct map_entry *e1, const struct map_entry *e2,
	       const char *key)
{
	return strcasecmp(e1->key, key ? key : e2->key);
}

static struct map_entry *alloc_entry(int hash, char *key, int klen,
				     char *value, int vlen)
{
	struct map_entry *entry = malloc(sizeof(struct map_entry) + klen
			+ vlen + 2);
	hashmap_entry_init(entry, hash);
	memcpy(entry->key, key, klen + 1);
	memcpy(entry->key + klen + 1, value, vlen + 1);
	return entry;
}

static void hashput(char *id, char *str)
{
	int hash = 0;
	struct map_entry *entry;

	hash = strihash(id);
	entry = alloc_entry(hash, id, strlen(id), str, strlen(str));
	entry = hashmap_put(&map, entry);
}

static const char *get_value(const struct map_entry *e)
{
	return e->key + strlen(e->key) + 1;
}

static char *stripquotes(char *s)
{
	char *start, *end;

	start = strchr(s, '"');
	if (start)
		++start;
	else
		start = s;
	end = strrchr(start, '"');
	if (end)
		*(end) = '\0';
	return start;
}

static void process_line(char *line)
{
	char *p;
	static char *msgid;

	BUG_ON(!line);
	p = strrchr(line, '\n');
	if (p)
		*p = '\0';
	if (!strncmp(line, "msgid", 5)) {
		if (msgid)
			warn("last msgid had no corresponding msgstr");
		msgid = strdup(stripquotes(line + 5));
	} else if (!strncmp(line, "msgstr", 6)) {
		if (!msgid)
			warn("msgstr without msgid");
		hashput(msgid, stripquotes(line + 6));
		xfree(msgid);
	}
}

static void read_file(FILE *fp)
{
	char line[1024];

	while (fgets(line, sizeof(line), fp))
		process_line(line);
}

static void parse_file(char *filename)
{
	FILE *fp;

	fp = fopen(filename, "r");
	if (!fp)
		return;
	read_file(fp);
	fclose(fp);
}

void i18n_open(const char *filename)
{
	struct stat sb;
	struct sbuf s;
	static int has_run;

	if (!has_run) {
		hashmap_init(&map, (hashmap_cmp_fn) cmp, 0);
		has_run = 1;
	}
	if (!filename)
		return;
	if (filename[0] == '\0')
		return;
	sbuf_init(&s);
	sbuf_cpy(&s, filename);
	sbuf_expand_tilde(&s);
	if (stat(s.buf, &sb) < 0)
		die("%s: stat()", __func__);
	parse_file(s.buf);
	xfree(s.buf);
}

char *i18n_translate(struct sbuf *s)
{
	char *remainder;
	struct map_entry *entry;
	int hash = 0;

	remainder = strrchr(s->buf, ',');
	if (remainder) {
		*remainder = '\0';
		++remainder;
	}

	/* s->buf now contains first field, which we want to translate */
	hash = strihash(s->buf);
	entry = hashmap_get_from_hash(&map, hash, s->buf);
	if (entry)
		sbuf_cpy(s, get_value(entry));
	return remainder;
}