/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types_int.h"
#include "memory.h"
#include "filenames.h"
#include "logging.h"
#include "dictstat.h"

extern const int comptime;

int sort_by_dictstat (const void *s1, const void *s2)
{
  dictstat_t *d1 = (dictstat_t *) s1;
  dictstat_t *d2 = (dictstat_t *) s2;

  #if defined (__linux__)
  d2->stat.st_atim = d1->stat.st_atim;
  #else
  d2->stat.st_atime = d1->stat.st_atime;
  #endif

  return memcmp (&d1->stat, &d2->stat, sizeof (struct stat));
}

void dictstat_init (dictstat_ctx_t *dictstat_ctx, char *profile_dir)
{
  dictstat_ctx->filename = (char *)       mymalloc (HCBUFSIZ_TINY);
  dictstat_ctx->base     = (dictstat_t *) mycalloc (MAX_DICTSTAT, sizeof (dictstat_t));
  dictstat_ctx->cnt      = 0;

  generate_dictstat_filename (profile_dir, dictstat_ctx->filename);
}

void dictstat_destroy (dictstat_ctx_t *dictstat_ctx)
{
  myfree (dictstat_ctx->filename);
  myfree (dictstat_ctx->base);
}

void dictstat_read (dictstat_ctx_t *dictstat_ctx)
{
  FILE *fp = fopen (dictstat_ctx->filename, "rb");

  if (fp == NULL)
  {
    // first run, file does not exist, do not error out

    return;
  }

  #if defined (_POSIX)
  struct stat tmpstat;

  fstat (fileno (fp), &tmpstat);
  #endif

  #if defined (_WIN)
  struct stat64 tmpstat;

  _fstat64 (fileno (fp), &tmpstat);
  #endif

  if (tmpstat.st_mtime < comptime)
  {
    /* with v0.15 the format changed so we have to ensure user is using a good version
       since there is no version-header in the dictstat file */

    fclose (fp);

    unlink (dictstat_ctx->filename);

    return;
  }

  while (!feof (fp))
  {
    dictstat_t d;

    const int nread = fread (&d, sizeof (dictstat_t), 1, fp);

    if (nread == 0) continue;

    lsearch (&d, dictstat_ctx->base, &dictstat_ctx->cnt, sizeof (dictstat_t), sort_by_dictstat);

    if (dictstat_ctx->cnt == MAX_DICTSTAT)
    {
      log_error ("ERROR: There are too many entries in the %s database. You have to remove/rename it.", dictstat_ctx->filename);

      break;
    }
  }

  fclose (fp);
}

int dictstat_write (dictstat_ctx_t *dictstat_ctx)
{
  FILE *fp = fopen (dictstat_ctx->filename, "wb");

  if (fp == NULL)
  {
    log_error ("ERROR: %s: %s", dictstat_ctx->filename, strerror (errno));

    return -1;
  }

  fwrite (dictstat_ctx->base, sizeof (dictstat_t), dictstat_ctx->cnt, fp);

  fclose (fp);

  return 0;
}

u64 dictstat_find (dictstat_ctx_t *dictstat_ctx, dictstat_t *d)
{
  dictstat_t *d_cache = (dictstat_t *) lfind (d, dictstat_ctx->base, &dictstat_ctx->cnt, sizeof (dictstat_t), sort_by_dictstat);

  if (d_cache == NULL) return 0;

  return d_cache->cnt;
}

void dictstat_append (dictstat_ctx_t *dictstat_ctx, dictstat_t *d)
{
  if (dictstat_ctx->cnt == MAX_DICTSTAT)
  {
    log_error ("ERROR: There are too many entries in the %s database. You have to remove/rename it.", dictstat_ctx->filename);

    return;
  }

  lsearch (d, dictstat_ctx->base, &dictstat_ctx->cnt, sizeof (dictstat_t), sort_by_dictstat);
}
