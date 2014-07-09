#ifndef _SSL_TRACE_H
#define _SSL_TRACE_H
void SSL_trace(int write_p, int version, int content_type, 
		const void *buf, size_t msglen, SSL *ssl, void *arg);
#endif // _SSL_TRACE_H
