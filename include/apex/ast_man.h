#ifndef APEX_AST_MAN_H
#define APEX_AST_MAN_H

#include "apex/apex.h"
#include "cmark-gfm.h"

#ifdef __cplusplus
extern "C" {
#endif

char *apex_cmark_to_man_roff(cmark_node *document, const apex_options *options);
char *apex_cmark_to_man_html(cmark_node *document, const apex_options *options);

#ifdef __cplusplus
}
#endif

#endif
