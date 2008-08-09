#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#include "git-common.h"

GQuark
git_error_quark (void)
{
  return g_quark_from_static_string ("git-error-quark");
}
