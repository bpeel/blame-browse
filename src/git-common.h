#ifndef __GIT_COMMON_H__
#define __GIT_COMMON_H__

#include <glib.h>
#include <glib-object.h>

#define GIT_ERROR (git_error_quark ())

typedef enum {
  GIT_ERROR_EXIT_STATUS
} GitError;

GQuark git_error_quark (void);

gboolean _git_boolean_continue_accumulator (GSignalInvocationHint *ihint,
					    GValue *return_accu,
					    const GValue *handler_return,
					    gpointer data);

#endif /* __GIT_COMMON_H__ */
