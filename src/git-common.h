#ifndef __GIT_COMMON_H__
#define __GIT_COMMON_H__

#include <glib.h>

#define GIT_ERROR (git_error_quark ())

typedef enum {
  GIT_ERROR_EXIT_STATUS
} GitError;

GQuark git_error_quark (void);

#endif /* __GIT_COMMON_H__ */
