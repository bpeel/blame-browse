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

gboolean
_git_boolean_continue_accumulator (GSignalInvocationHint *ihint,
				   GValue *return_accu,
				   const GValue *handler_return,
				   gpointer data)
{
  gboolean continue_emission;

  continue_emission = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, continue_emission);
  
  return continue_emission;
}

