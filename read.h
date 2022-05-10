/* file: read.h */

#ifndef READ_H__
#define READ_H__

#ifdef HAVE_READ2CH_H
#include "read2ch.h"
#endif

#include <unistd.h>
#include "apr_date.h"
#include "apr_file_io.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_tables.h"
#include "apr_time.h"

#ifdef USE_MMAP
#include "apr_mmap.h"
#endif

#ifdef PUT_ETAG
#include "zlib.h"
#endif

#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#include "apr_want.h"

#include "httpd.h"
#include "http_core.h"
#include "http_protocol.h"
#include "http_request.h"

enum html_error_t {
	ERROR_TOO_HUGE,
	ERROR_NOT_FOUND,
	ERROR_NO_MEMORY,
	ERROR_MAINTENANCE,
	ERROR_LOGOUT,
#ifdef	Katjusha_DLL_REPLY
	ERROR_ABORNED
#endif
};

typedef struct global_vars global_vars_t;

extern int dat_read(global_vars_t *gv, request_rec *r,
		    char const *fname,
		    int st,
		    int to,
		    int ls);
extern int dat_out(global_vars_t *gv, request_rec *r, int level);
extern void html_error(global_vars_t *gv, request_rec *r,
		       enum html_error_t errorcode);

#endif /* READ_H__ */
