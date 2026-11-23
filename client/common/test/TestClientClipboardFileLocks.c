#include <freerdp/client/client_cliprdr_file.h>
#include <freerdp/channels/channels.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
static UINT16 response_flags;
static BYTE response_data[8];
static UINT response(CliprdrClientContext* c, const CLIPRDR_FILE_CONTENTS_RESPONSE* r) {
 (void)c; response_flags=r->common.msgFlags;
 memset(response_data,0,sizeof(response_data));
 if(r->requestedData && r->cbRequested<=sizeof(response_data)) memcpy(response_data,r->requestedData,r->cbRequested);
 return 0;
}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void read_data(CliprdrClientContext* c, BOOL locked, UINT32 id, const char* expected) {
 CLIPRDR_FILE_CONTENTS_REQUEST r={0};r.haveClipDataId=locked;r.clipDataId=id;r.listIndex=0;r.streamId=1;r.dwFlags=FILECONTENTS_RANGE;r.nPositionLow=20*1024*1024;r.cbRequested=4;
 response_flags=0;CHECK(c->ServerFileContentsRequest(c,&r)==0);
 CHECK(response_flags==(expected?CB_RESPONSE_OK:CB_RESPONSE_FAIL));
 if(expected)CHECK(memcmp(response_data,expected,4)==0);
}
static void lock_data(CliprdrClientContext* c,UINT32 id) {CLIPRDR_LOCK_CLIPBOARD_DATA r={0};r.clipDataId=id;CHECK(c->ServerLockClipboardData(c,&r)==0);}
static void unlock_data(CliprdrClientContext* c,UINT32 id) {CLIPRDR_UNLOCK_CLIPBOARD_DATA r={0};r.clipDataId=id;CHECK(c->ServerUnlockClipboardData(c,&r)==0);}
int main(void) {
 char a[100],b[100],ua[128],ub[128];snprintf(a,sizeof(a),"/tmp/cliprdr-test-%d-a",getpid());snprintf(b,sizeof(b),"/tmp/cliprdr-test-%d-b",getpid());
 const char* paths[]={a,b};const char* values[]={"AAAA","BBBB"};
 for(int i=0;i<2;i++){int fd=open(paths[i],O_CREAT|O_EXCL|O_RDWR,0600);CHECK(fd>=0);CHECK(ftruncate(fd,32*1024*1024)==0);CHECK(pwrite(fd,values[i],4,20*1024*1024)==4);close(fd);}
 snprintf(ua,sizeof(ua),"file://%s\n",a);snprintf(ub,sizeof(ub),"file://%s\n",b);
 CliprdrClientContext c={0};c.ClientFileContentsResponse=response;
 CliprdrFileContext* f=cliprdr_file_context_new(NULL);CHECK(f);CHECK(cliprdr_file_context_init(f,&c));
 CHECK(cliprdr_file_context_remote_set_flags(f,CB_STREAM_FILECLIP_ENABLED|CB_CAN_LOCK_CLIPDATA));CHECK(cliprdr_file_context_set_locally_available(f,TRUE));CHECK(cliprdr_file_context_current_flags(f)&CB_CAN_LOCK_CLIPDATA);
 CHECK(cliprdr_file_context_notify_new_client_format_list(f)==0);
 lock_data(&c,0); /* Valid lock ID zero, before the descriptor is fetched. */
 CHECK(cliprdr_file_context_update_client_data(f,ua,strlen(ua)));
 read_data(&c,TRUE,0,"AAAA");read_data(&c,FALSE,0,"AAAA");
 lock_data(&c,7); /* A second lock after the file list was already fetched. */
 CHECK(cliprdr_file_context_clear(f));
 read_data(&c,TRUE,0,"AAAA");read_data(&c,TRUE,7,"AAAA");read_data(&c,FALSE,0,NULL);
 CHECK(cliprdr_file_context_notify_new_client_format_list(f)==0);
 lock_data(&c,8);CHECK(cliprdr_file_context_update_client_data(f,ub,strlen(ub)));
 read_data(&c,FALSE,0,"BBBB");read_data(&c,TRUE,8,"BBBB");read_data(&c,TRUE,0,"AAAA");read_data(&c,TRUE,7,"AAAA");
 CHECK(cliprdr_file_context_clear(f));read_data(&c,TRUE,8,"BBBB");
 unlock_data(&c,7);read_data(&c,TRUE,7,NULL);read_data(&c,TRUE,0,"AAAA");
 unlock_data(&c,0);unlock_data(&c,8);read_data(&c,TRUE,0,NULL);read_data(&c,TRUE,8,NULL);
 cliprdr_file_context_free(f);unlink(a);unlink(b);puts("PASS: lock before/after data, ID zero, clipboard clear, concurrent generations, unlock cleanup; reads beyond 20 MiB");return 0;
}
