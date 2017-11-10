#ifndef CS_CLIPBOARD_H_
#define CS_CLIPBOARD_H_

#include <freerdp/client/cliprdr.h>

#include "DevolutionsRdp.h"

int cs_cliprdr_send_client_format_list(CliprdrClientContext* cliprdr);

int cs_cliprdr_init(csContext* ctx, CliprdrClientContext* cliprdr);
int cs_cliprdr_uninit(csContext* ctx, CliprdrClientContext* cliprdr);

#endif /* CS_CLIPBOARD_H_ */
