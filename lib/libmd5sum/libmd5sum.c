#include "libmd5sum.h"
#include <sys/types.h>


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "md5.h"
#include "getline.h"
#include <errno.h>
#include "error.h"
#include <string.h>

#define STREQ(a, b) (strcmp ((a), (b)) == 0)

static int have_read_stdin;

#if O_BINARY
# define OPENOPTS(BINARY) ((BINARY) != 0 ? TEXT1TO1 : TEXTCNVT)
# define TEXT1TO1 "rb"
# define TEXTCNVT "r"
#else
# if defined VMS
#  define OPENOPTS(BINARY) ((BINARY) != 0 ? TEXT1TO1 : TEXTCNVT)
#  define TEXT1TO1 "rb", "ctx=stm"
#  define TEXTCNVT "r", "ctx=stm"
# else
#  if UNIX || __UNIX__ || unix || __unix__ || _POSIX_VERSION
#   define OPENOPTS(BINARY) "r"
#  else
    /* The following line is intended to evoke an error.
       Using #error is not portable enough.  */
    "Cannot determine system type."
#  endif
# endif
#endif

int md5_file (const char *filename, int binary, unsigned char *md5_result)
{
  FILE *fp;
  int err;

  (void)binary;

  if (STREQ (filename, "-"))
    {
      have_read_stdin = 1;
      fp = stdin;
#if O_BINARY
      /* If we need binary reads from a pipe or redirected stdin, we need
         to switch it to BINARY mode here, since stdin is already open.  */
      if (binary)
        SET_BINARY (fileno (stdin));
#endif
    }
  else
    {
      /* OPENOPTS is a macro.  It varies with the system.
         Some systems distinguish between internal and
         external text representations.  */

      fp = fopen (filename, OPENOPTS (binary));
      if (fp == NULL)
        {
          error (0, errno, "%s", filename);
          return 1;
        }
    }
 
 err = md5_stream (fp, md5_result);
  if (err)
    {
      error (0, errno, "%s", filename);
      if (fp != stdin)
        fclose (fp);
      return 1;
    }

  if (fp != stdin && fclose (fp) == EOF)
    {
      error (0, errno, "%s", filename);
      return 1;
    }

  return 0;
}
