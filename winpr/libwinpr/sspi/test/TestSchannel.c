
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/file.h>
#include <winpr/pipe.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/crypto.h>
#include <winpr/wlog.h>
#include <winpr/schannel.h>

static BOOL g_ClientWait = FALSE;
static BOOL g_ServerWait = FALSE;

static HANDLE g_ClientReadPipe = NULL;
static HANDLE g_ClientWritePipe = NULL;
static HANDLE g_ServerReadPipe = NULL;
static HANDLE g_ServerWritePipe = NULL;

static const BYTE test_localhost_crt[1029] =
{
	0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x43, 0x45, 0x52, 0x54,
	0x49, 0x46, 0x49, 0x43, 0x41, 0x54, 0x45, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A, 0x4D, 0x49,
	0x49, 0x43, 0x79, 0x6A, 0x43, 0x43, 0x41, 0x62, 0x4B, 0x67, 0x41, 0x77, 0x49, 0x42, 0x41,
	0x67, 0x49, 0x45, 0x63, 0x61, 0x64, 0x63, 0x72, 0x7A, 0x41, 0x4E, 0x42, 0x67, 0x6B, 0x71,
	0x68, 0x6B, 0x69, 0x47, 0x39, 0x77, 0x30, 0x42, 0x41, 0x51, 0x55, 0x46, 0x41, 0x44, 0x41,
	0x55, 0x4D, 0x52, 0x49, 0x77, 0x45, 0x41, 0x59, 0x44, 0x56, 0x51, 0x51, 0x44, 0x45, 0x77,
	0x6C, 0x73, 0x0A, 0x62, 0x32, 0x4E, 0x68, 0x62, 0x47, 0x68, 0x76, 0x63, 0x33, 0x51, 0x77,
	0x48, 0x68, 0x63, 0x4E, 0x4D, 0x54, 0x4D, 0x78, 0x4D, 0x44, 0x45, 0x78, 0x4D, 0x44, 0x59,
	0x78, 0x4E, 0x7A, 0x55, 0x31, 0x57, 0x68, 0x63, 0x4E, 0x4D, 0x54, 0x51, 0x78, 0x4D, 0x44,
	0x45, 0x78, 0x4D, 0x44, 0x59, 0x78, 0x4E, 0x7A, 0x55, 0x31, 0x57, 0x6A, 0x41, 0x55, 0x4D,
	0x52, 0x49, 0x77, 0x45, 0x41, 0x59, 0x44, 0x0A, 0x56, 0x51, 0x51, 0x44, 0x45, 0x77, 0x6C,
	0x73, 0x62, 0x32, 0x4E, 0x68, 0x62, 0x47, 0x68, 0x76, 0x63, 0x33, 0x51, 0x77, 0x67, 0x67,
	0x45, 0x69, 0x4D, 0x41, 0x30, 0x47, 0x43, 0x53, 0x71, 0x47, 0x53, 0x49, 0x62, 0x33, 0x44,
	0x51, 0x45, 0x42, 0x41, 0x51, 0x55, 0x41, 0x41, 0x34, 0x49, 0x42, 0x44, 0x77, 0x41, 0x77,
	0x67, 0x67, 0x45, 0x4B, 0x41, 0x6F, 0x49, 0x42, 0x41, 0x51, 0x43, 0x33, 0x0A, 0x65, 0x6E,
	0x33, 0x68, 0x5A, 0x4F, 0x53, 0x33, 0x6B, 0x51, 0x2F, 0x55, 0x54, 0x30, 0x53, 0x45, 0x6C,
	0x30, 0x48, 0x6E, 0x50, 0x79, 0x64, 0x48, 0x75, 0x35, 0x39, 0x61, 0x69, 0x71, 0x64, 0x73,
	0x64, 0x53, 0x55, 0x74, 0x6E, 0x43, 0x41, 0x37, 0x46, 0x66, 0x74, 0x30, 0x4F, 0x36, 0x51,
	0x79, 0x68, 0x49, 0x71, 0x58, 0x7A, 0x30, 0x47, 0x32, 0x53, 0x76, 0x77, 0x4C, 0x54, 0x62,
	0x79, 0x68, 0x0A, 0x59, 0x54, 0x68, 0x31, 0x36, 0x78, 0x31, 0x72, 0x45, 0x48, 0x68, 0x31,
	0x57, 0x47, 0x5A, 0x6D, 0x36, 0x77, 0x64, 0x2B, 0x4B, 0x76, 0x38, 0x6B, 0x31, 0x6B, 0x2F,
	0x36, 0x6F, 0x41, 0x2F, 0x4F, 0x51, 0x76, 0x65, 0x61, 0x38, 0x6B, 0x63, 0x45, 0x64, 0x53,
	0x72, 0x54, 0x64, 0x75, 0x71, 0x4A, 0x33, 0x65, 0x66, 0x74, 0x48, 0x4A, 0x4A, 0x6E, 0x43,
	0x4B, 0x30, 0x41, 0x62, 0x68, 0x34, 0x39, 0x0A, 0x41, 0x47, 0x41, 0x50, 0x39, 0x79, 0x58,
	0x77, 0x77, 0x59, 0x41, 0x6A, 0x51, 0x49, 0x52, 0x6E, 0x38, 0x2B, 0x4F, 0x63, 0x63, 0x48,
	0x74, 0x6F, 0x4E, 0x75, 0x75, 0x79, 0x52, 0x63, 0x6B, 0x49, 0x50, 0x71, 0x75, 0x70, 0x78,
	0x79, 0x31, 0x4A, 0x5A, 0x4B, 0x39, 0x64, 0x76, 0x76, 0x62, 0x34, 0x79, 0x53, 0x6B, 0x49,
	0x75, 0x7A, 0x62, 0x79, 0x50, 0x6F, 0x54, 0x41, 0x79, 0x61, 0x55, 0x2B, 0x0A, 0x51, 0x72,
	0x70, 0x34, 0x78, 0x67, 0x64, 0x4B, 0x46, 0x54, 0x70, 0x6B, 0x50, 0x46, 0x34, 0x33, 0x6A,
	0x32, 0x4D, 0x6D, 0x5A, 0x72, 0x46, 0x63, 0x42, 0x76, 0x79, 0x6A, 0x69, 0x35, 0x6A, 0x4F,
	0x37, 0x74, 0x66, 0x6F, 0x56, 0x61, 0x6B, 0x59, 0x47, 0x53, 0x2F, 0x4C, 0x63, 0x78, 0x77,
	0x47, 0x2B, 0x77, 0x51, 0x77, 0x63, 0x4F, 0x43, 0x54, 0x42, 0x45, 0x78, 0x2F, 0x7A, 0x31,
	0x53, 0x30, 0x0A, 0x37, 0x49, 0x2F, 0x6A, 0x62, 0x44, 0x79, 0x53, 0x4E, 0x68, 0x44, 0x35,
	0x63, 0x61, 0x63, 0x54, 0x75, 0x4E, 0x36, 0x50, 0x68, 0x33, 0x58, 0x30, 0x71, 0x70, 0x47,
	0x73, 0x37, 0x79, 0x50, 0x6B, 0x4E, 0x79, 0x69, 0x4A, 0x33, 0x57, 0x52, 0x69, 0x6C, 0x35,
	0x75, 0x57, 0x73, 0x4B, 0x65, 0x79, 0x63, 0x64, 0x71, 0x42, 0x4E, 0x72, 0x34, 0x75, 0x32,
	0x62, 0x49, 0x52, 0x6E, 0x63, 0x54, 0x51, 0x0A, 0x46, 0x72, 0x68, 0x73, 0x58, 0x39, 0x69,
	0x77, 0x37, 0x35, 0x76, 0x75, 0x53, 0x64, 0x35, 0x46, 0x39, 0x37, 0x56, 0x70, 0x41, 0x67,
	0x4D, 0x42, 0x41, 0x41, 0x47, 0x6A, 0x4A, 0x44, 0x41, 0x69, 0x4D, 0x42, 0x4D, 0x47, 0x41,
	0x31, 0x55, 0x64, 0x4A, 0x51, 0x51, 0x4D, 0x4D, 0x41, 0x6F, 0x47, 0x43, 0x43, 0x73, 0x47,
	0x41, 0x51, 0x55, 0x46, 0x42, 0x77, 0x4D, 0x42, 0x4D, 0x41, 0x73, 0x47, 0x0A, 0x41, 0x31,
	0x55, 0x64, 0x44, 0x77, 0x51, 0x45, 0x41, 0x77, 0x49, 0x45, 0x4D, 0x44, 0x41, 0x4E, 0x42,
	0x67, 0x6B, 0x71, 0x68, 0x6B, 0x69, 0x47, 0x39, 0x77, 0x30, 0x42, 0x41, 0x51, 0x55, 0x46,
	0x41, 0x41, 0x4F, 0x43, 0x41, 0x51, 0x45, 0x41, 0x49, 0x51, 0x66, 0x75, 0x2F, 0x77, 0x39,
	0x45, 0x34, 0x4C, 0x6F, 0x67, 0x30, 0x71, 0x35, 0x4B, 0x53, 0x38, 0x71, 0x46, 0x78, 0x62,
	0x36, 0x6F, 0x0A, 0x36, 0x31, 0x62, 0x35, 0x37, 0x6F, 0x6D, 0x6E, 0x46, 0x59, 0x52, 0x34,
	0x47, 0x43, 0x67, 0x33, 0x6F, 0x6A, 0x4F, 0x4C, 0x54, 0x66, 0x38, 0x7A, 0x6A, 0x4D, 0x43,
	0x52, 0x6D, 0x75, 0x59, 0x32, 0x76, 0x30, 0x4E, 0x34, 0x78, 0x66, 0x68, 0x69, 0x35, 0x4B,
	0x69, 0x59, 0x67, 0x64, 0x76, 0x4E, 0x4C, 0x4F, 0x33, 0x52, 0x42, 0x6D, 0x4E, 0x50, 0x76,
	0x59, 0x58, 0x50, 0x52, 0x46, 0x41, 0x76, 0x0A, 0x66, 0x61, 0x76, 0x66, 0x57, 0x75, 0x6C,
	0x44, 0x31, 0x64, 0x50, 0x36, 0x31, 0x69, 0x35, 0x62, 0x36, 0x59, 0x66, 0x56, 0x6C, 0x78,
	0x62, 0x31, 0x61, 0x57, 0x46, 0x37, 0x4C, 0x5A, 0x44, 0x32, 0x55, 0x6E, 0x63, 0x41, 0x6A,
	0x37, 0x4E, 0x38, 0x78, 0x38, 0x2B, 0x36, 0x58, 0x6B, 0x30, 0x6B, 0x63, 0x70, 0x58, 0x46,
	0x38, 0x6C, 0x77, 0x58, 0x48, 0x55, 0x57, 0x57, 0x55, 0x6D, 0x73, 0x2B, 0x0A, 0x4B, 0x56,
	0x44, 0x34, 0x34, 0x39, 0x68, 0x6F, 0x4D, 0x2B, 0x77, 0x4E, 0x4A, 0x49, 0x61, 0x4F, 0x52,
	0x39, 0x4C, 0x46, 0x2B, 0x6B, 0x6F, 0x32, 0x32, 0x37, 0x7A, 0x74, 0x37, 0x54, 0x41, 0x47,
	0x64, 0x56, 0x35, 0x4A, 0x75, 0x7A, 0x71, 0x38, 0x32, 0x2F, 0x6B, 0x75, 0x73, 0x6F, 0x65,
	0x32, 0x69, 0x75, 0x57, 0x77, 0x54, 0x65, 0x42, 0x6C, 0x53, 0x5A, 0x6E, 0x6B, 0x42, 0x38,
	0x63, 0x64, 0x0A, 0x77, 0x4D, 0x30, 0x5A, 0x42, 0x58, 0x6D, 0x34, 0x35, 0x48, 0x38, 0x6F,
	0x79, 0x75, 0x36, 0x4A, 0x71, 0x59, 0x71, 0x45, 0x6D, 0x75, 0x4A, 0x51, 0x64, 0x67, 0x79,
	0x52, 0x2B, 0x63, 0x53, 0x53, 0x41, 0x7A, 0x2B, 0x4F, 0x32, 0x6D, 0x61, 0x62, 0x68, 0x50,
	0x5A, 0x65, 0x49, 0x76, 0x78, 0x65, 0x67, 0x6A, 0x6A, 0x61, 0x5A, 0x61, 0x46, 0x4F, 0x71,
	0x74, 0x73, 0x2B, 0x64, 0x33, 0x72, 0x39, 0x0A, 0x79, 0x71, 0x4A, 0x78, 0x67, 0x75, 0x39,
	0x43, 0x38, 0x39, 0x5A, 0x69, 0x33, 0x39, 0x57, 0x34, 0x38, 0x46, 0x66, 0x46, 0x63, 0x49,
	0x58, 0x4A, 0x4F, 0x6B, 0x39, 0x43, 0x4E, 0x46, 0x41, 0x2F, 0x69, 0x70, 0x54, 0x57, 0x6A,
	0x74, 0x74, 0x4E, 0x2F, 0x6B, 0x4F, 0x6B, 0x5A, 0x42, 0x70, 0x6F, 0x6A, 0x2F, 0x32, 0x6A,
	0x4E, 0x45, 0x62, 0x4F, 0x59, 0x7A, 0x7A, 0x6E, 0x4B, 0x77, 0x3D, 0x3D, 0x0A, 0x2D, 0x2D,
	0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x43, 0x45, 0x52, 0x54, 0x49, 0x46, 0x49, 0x43,
	0x41, 0x54, 0x45, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A
};

static const BYTE test_localhost_key[1704] =
{
	0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x42, 0x45, 0x47, 0x49, 0x4E, 0x20, 0x50, 0x52, 0x49, 0x56,
	0x41, 0x54, 0x45, 0x20, 0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A, 0x4D, 0x49,
	0x49, 0x45, 0x76, 0x51, 0x49, 0x42, 0x41, 0x44, 0x41, 0x4E, 0x42, 0x67, 0x6B, 0x71, 0x68,
	0x6B, 0x69, 0x47, 0x39, 0x77, 0x30, 0x42, 0x41, 0x51, 0x45, 0x46, 0x41, 0x41, 0x53, 0x43,
	0x42, 0x4B, 0x63, 0x77, 0x67, 0x67, 0x53, 0x6A, 0x41, 0x67, 0x45, 0x41, 0x41, 0x6F, 0x49,
	0x42, 0x41, 0x51, 0x43, 0x33, 0x65, 0x6E, 0x33, 0x68, 0x5A, 0x4F, 0x53, 0x33, 0x6B, 0x51,
	0x2F, 0x55, 0x0A, 0x54, 0x30, 0x53, 0x45, 0x6C, 0x30, 0x48, 0x6E, 0x50, 0x79, 0x64, 0x48,
	0x75, 0x35, 0x39, 0x61, 0x69, 0x71, 0x64, 0x73, 0x64, 0x53, 0x55, 0x74, 0x6E, 0x43, 0x41,
	0x37, 0x46, 0x66, 0x74, 0x30, 0x4F, 0x36, 0x51, 0x79, 0x68, 0x49, 0x71, 0x58, 0x7A, 0x30,
	0x47, 0x32, 0x53, 0x76, 0x77, 0x4C, 0x54, 0x62, 0x79, 0x68, 0x59, 0x54, 0x68, 0x31, 0x36,
	0x78, 0x31, 0x72, 0x45, 0x48, 0x68, 0x31, 0x0A, 0x57, 0x47, 0x5A, 0x6D, 0x36, 0x77, 0x64,
	0x2B, 0x4B, 0x76, 0x38, 0x6B, 0x31, 0x6B, 0x2F, 0x36, 0x6F, 0x41, 0x2F, 0x4F, 0x51, 0x76,
	0x65, 0x61, 0x38, 0x6B, 0x63, 0x45, 0x64, 0x53, 0x72, 0x54, 0x64, 0x75, 0x71, 0x4A, 0x33,
	0x65, 0x66, 0x74, 0x48, 0x4A, 0x4A, 0x6E, 0x43, 0x4B, 0x30, 0x41, 0x62, 0x68, 0x34, 0x39,
	0x41, 0x47, 0x41, 0x50, 0x39, 0x79, 0x58, 0x77, 0x77, 0x59, 0x41, 0x6A, 0x0A, 0x51, 0x49,
	0x52, 0x6E, 0x38, 0x2B, 0x4F, 0x63, 0x63, 0x48, 0x74, 0x6F, 0x4E, 0x75, 0x75, 0x79, 0x52,
	0x63, 0x6B, 0x49, 0x50, 0x71, 0x75, 0x70, 0x78, 0x79, 0x31, 0x4A, 0x5A, 0x4B, 0x39, 0x64,
	0x76, 0x76, 0x62, 0x34, 0x79, 0x53, 0x6B, 0x49, 0x75, 0x7A, 0x62, 0x79, 0x50, 0x6F, 0x54,
	0x41, 0x79, 0x61, 0x55, 0x2B, 0x51, 0x72, 0x70, 0x34, 0x78, 0x67, 0x64, 0x4B, 0x46, 0x54,
	0x70, 0x6B, 0x0A, 0x50, 0x46, 0x34, 0x33, 0x6A, 0x32, 0x4D, 0x6D, 0x5A, 0x72, 0x46, 0x63,
	0x42, 0x76, 0x79, 0x6A, 0x69, 0x35, 0x6A, 0x4F, 0x37, 0x74, 0x66, 0x6F, 0x56, 0x61, 0x6B,
	0x59, 0x47, 0x53, 0x2F, 0x4C, 0x63, 0x78, 0x77, 0x47, 0x2B, 0x77, 0x51, 0x77, 0x63, 0x4F,
	0x43, 0x54, 0x42, 0x45, 0x78, 0x2F, 0x7A, 0x31, 0x53, 0x30, 0x37, 0x49, 0x2F, 0x6A, 0x62,
	0x44, 0x79, 0x53, 0x4E, 0x68, 0x44, 0x35, 0x0A, 0x63, 0x61, 0x63, 0x54, 0x75, 0x4E, 0x36,
	0x50, 0x68, 0x33, 0x58, 0x30, 0x71, 0x70, 0x47, 0x73, 0x37, 0x79, 0x50, 0x6B, 0x4E, 0x79,
	0x69, 0x4A, 0x33, 0x57, 0x52, 0x69, 0x6C, 0x35, 0x75, 0x57, 0x73, 0x4B, 0x65, 0x79, 0x63,
	0x64, 0x71, 0x42, 0x4E, 0x72, 0x34, 0x75, 0x32, 0x62, 0x49, 0x52, 0x6E, 0x63, 0x54, 0x51,
	0x46, 0x72, 0x68, 0x73, 0x58, 0x39, 0x69, 0x77, 0x37, 0x35, 0x76, 0x75, 0x0A, 0x53, 0x64,
	0x35, 0x46, 0x39, 0x37, 0x56, 0x70, 0x41, 0x67, 0x4D, 0x42, 0x41, 0x41, 0x45, 0x43, 0x67,
	0x67, 0x45, 0x41, 0x42, 0x36, 0x6A, 0x6C, 0x65, 0x48, 0x4E, 0x74, 0x32, 0x50, 0x77, 0x46,
	0x58, 0x53, 0x65, 0x79, 0x42, 0x4A, 0x63, 0x4C, 0x2B, 0x55, 0x74, 0x35, 0x71, 0x46, 0x54,
	0x38, 0x34, 0x68, 0x72, 0x48, 0x77, 0x6F, 0x39, 0x68, 0x62, 0x66, 0x59, 0x47, 0x6F, 0x6E,
	0x44, 0x59, 0x0A, 0x66, 0x70, 0x47, 0x2B, 0x32, 0x52, 0x30, 0x50, 0x62, 0x43, 0x63, 0x4B,
	0x35, 0x30, 0x46, 0x61, 0x4A, 0x46, 0x36, 0x71, 0x63, 0x56, 0x4A, 0x4E, 0x75, 0x52, 0x36,
	0x48, 0x71, 0x2B, 0x43, 0x55, 0x4A, 0x74, 0x48, 0x35, 0x39, 0x48, 0x48, 0x37, 0x62, 0x68,
	0x6A, 0x39, 0x62, 0x64, 0x78, 0x45, 0x6D, 0x6F, 0x48, 0x30, 0x4A, 0x76, 0x68, 0x45, 0x76,
	0x67, 0x4D, 0x2F, 0x55, 0x38, 0x42, 0x51, 0x0A, 0x65, 0x57, 0x4F, 0x4E, 0x68, 0x78, 0x50,
	0x73, 0x69, 0x73, 0x6D, 0x57, 0x6B, 0x78, 0x61, 0x5A, 0x6F, 0x6C, 0x72, 0x32, 0x69, 0x44,
	0x56, 0x72, 0x7A, 0x54, 0x37, 0x55, 0x4A, 0x71, 0x6A, 0x74, 0x59, 0x49, 0x74, 0x67, 0x2B,
	0x37, 0x59, 0x43, 0x32, 0x70, 0x55, 0x58, 0x6B, 0x64, 0x49, 0x35, 0x4A, 0x4D, 0x67, 0x6C,
	0x44, 0x47, 0x4D, 0x52, 0x5A, 0x35, 0x55, 0x5A, 0x48, 0x75, 0x63, 0x7A, 0x0A, 0x41, 0x56,
	0x2B, 0x71, 0x77, 0x77, 0x33, 0x65, 0x45, 0x52, 0x74, 0x78, 0x44, 0x50, 0x61, 0x61, 0x61,
	0x34, 0x54, 0x39, 0x50, 0x64, 0x33, 0x44, 0x31, 0x6D, 0x62, 0x71, 0x58, 0x66, 0x75, 0x45,
	0x68, 0x42, 0x6D, 0x33, 0x51, 0x6F, 0x2B, 0x75, 0x7A, 0x51, 0x32, 0x36, 0x76, 0x73, 0x66,
	0x48, 0x75, 0x56, 0x76, 0x61, 0x39, 0x38, 0x32, 0x4F, 0x6A, 0x41, 0x55, 0x6A, 0x6E, 0x64,
	0x30, 0x70, 0x0A, 0x77, 0x43, 0x53, 0x6E, 0x42, 0x49, 0x48, 0x67, 0x70, 0x73, 0x30, 0x79,
	0x61, 0x45, 0x50, 0x63, 0x37, 0x46, 0x78, 0x39, 0x71, 0x45, 0x63, 0x6D, 0x33, 0x70, 0x7A,
	0x41, 0x56, 0x31, 0x69, 0x72, 0x31, 0x4E, 0x4E, 0x63, 0x51, 0x47, 0x55, 0x45, 0x75, 0x45,
	0x6C, 0x4A, 0x78, 0x76, 0x2B, 0x69, 0x57, 0x34, 0x6D, 0x35, 0x70, 0x7A, 0x4C, 0x6A, 0x64,
	0x53, 0x63, 0x49, 0x30, 0x59, 0x45, 0x73, 0x0A, 0x4D, 0x61, 0x33, 0x78, 0x32, 0x79, 0x48,
	0x74, 0x6E, 0x77, 0x79, 0x65, 0x4C, 0x4D, 0x54, 0x4B, 0x6C, 0x72, 0x46, 0x4B, 0x70, 0x55,
	0x4E, 0x4A, 0x62, 0x78, 0x73, 0x35, 0x32, 0x62, 0x5A, 0x4B, 0x71, 0x49, 0x56, 0x33, 0x33,
	0x4A, 0x53, 0x34, 0x41, 0x51, 0x4B, 0x42, 0x67, 0x51, 0x44, 0x73, 0x4C, 0x54, 0x49, 0x68,
	0x35, 0x59, 0x38, 0x4C, 0x2F, 0x48, 0x33, 0x64, 0x74, 0x68, 0x63, 0x62, 0x0A, 0x53, 0x43,
	0x45, 0x77, 0x32, 0x64, 0x42, 0x49, 0x76, 0x49, 0x79, 0x54, 0x7A, 0x39, 0x53, 0x72, 0x62,
	0x33, 0x58, 0x37, 0x37, 0x41, 0x77, 0x57, 0x45, 0x4C, 0x53, 0x4D, 0x49, 0x57, 0x53, 0x50,
	0x55, 0x43, 0x4B, 0x54, 0x49, 0x70, 0x6A, 0x4D, 0x73, 0x6E, 0x7A, 0x6B, 0x46, 0x67, 0x32,
	0x32, 0x59, 0x32, 0x53, 0x75, 0x47, 0x38, 0x4C, 0x72, 0x50, 0x6D, 0x76, 0x73, 0x46, 0x4A,
	0x34, 0x30, 0x0A, 0x32, 0x67, 0x35, 0x44, 0x55, 0x6C, 0x59, 0x33, 0x59, 0x6D, 0x53, 0x4F,
	0x46, 0x61, 0x45, 0x4A, 0x54, 0x70, 0x55, 0x47, 0x44, 0x4D, 0x79, 0x65, 0x33, 0x74, 0x36,
	0x4F, 0x30, 0x6C, 0x63, 0x51, 0x41, 0x66, 0x79, 0x6D, 0x58, 0x66, 0x41, 0x38, 0x74, 0x50,
	0x42, 0x48, 0x6A, 0x5A, 0x78, 0x56, 0x61, 0x38, 0x78, 0x78, 0x52, 0x5A, 0x6E, 0x56, 0x43,
	0x31, 0x41, 0x62, 0x75, 0x49, 0x49, 0x52, 0x0A, 0x6E, 0x77, 0x72, 0x4E, 0x46, 0x2B, 0x42,
	0x6F, 0x53, 0x4B, 0x55, 0x41, 0x73, 0x78, 0x2B, 0x46, 0x75, 0x35, 0x5A, 0x4A, 0x4B, 0x4F,
	0x66, 0x79, 0x4D, 0x51, 0x4B, 0x42, 0x67, 0x51, 0x44, 0x47, 0x34, 0x50, 0x52, 0x39, 0x2F,
	0x58, 0x58, 0x6B, 0x51, 0x54, 0x36, 0x6B, 0x7A, 0x4B, 0x64, 0x34, 0x50, 0x6C, 0x50, 0x4D,
	0x63, 0x2B, 0x4B, 0x51, 0x79, 0x4C, 0x45, 0x6C, 0x4B, 0x39, 0x71, 0x47, 0x0A, 0x41, 0x6D,
	0x6E, 0x2F, 0x31, 0x68, 0x64, 0x69, 0x57, 0x57, 0x4F, 0x52, 0x57, 0x46, 0x62, 0x32, 0x38,
	0x30, 0x4D, 0x77, 0x76, 0x77, 0x41, 0x64, 0x78, 0x72, 0x66, 0x65, 0x4C, 0x57, 0x4D, 0x57,
	0x32, 0x66, 0x76, 0x4C, 0x59, 0x4B, 0x66, 0x6C, 0x4F, 0x35, 0x50, 0x51, 0x44, 0x59, 0x67,
	0x4B, 0x4A, 0x78, 0x35, 0x79, 0x50, 0x37, 0x52, 0x64, 0x38, 0x2F, 0x64, 0x50, 0x79, 0x5A,
	0x59, 0x36, 0x0A, 0x7A, 0x56, 0x37, 0x47, 0x47, 0x6B, 0x51, 0x5A, 0x42, 0x4B, 0x36, 0x79,
	0x74, 0x61, 0x66, 0x32, 0x35, 0x44, 0x50, 0x67, 0x50, 0x72, 0x32, 0x77, 0x73, 0x59, 0x4D,
	0x43, 0x6C, 0x53, 0x74, 0x6C, 0x56, 0x74, 0x72, 0x6D, 0x4F, 0x78, 0x59, 0x55, 0x56, 0x77,
	0x42, 0x59, 0x4F, 0x69, 0x36, 0x45, 0x62, 0x50, 0x69, 0x6B, 0x78, 0x47, 0x48, 0x5A, 0x70,
	0x59, 0x6F, 0x5A, 0x5A, 0x70, 0x68, 0x4A, 0x0A, 0x4E, 0x61, 0x38, 0x4F, 0x4C, 0x31, 0x69,
	0x77, 0x75, 0x51, 0x4B, 0x42, 0x67, 0x51, 0x44, 0x42, 0x55, 0x55, 0x31, 0x54, 0x79, 0x5A,
	0x2B, 0x4A, 0x5A, 0x43, 0x64, 0x79, 0x72, 0x33, 0x58, 0x43, 0x63, 0x77, 0x77, 0x58, 0x2F,
	0x48, 0x49, 0x73, 0x31, 0x34, 0x6B, 0x4B, 0x42, 0x48, 0x68, 0x44, 0x79, 0x33, 0x78, 0x37,
	0x74, 0x50, 0x38, 0x2F, 0x6F, 0x48, 0x54, 0x6F, 0x72, 0x76, 0x79, 0x74, 0x0A, 0x41, 0x68,
	0x38, 0x4B, 0x36, 0x4B, 0x72, 0x43, 0x41, 0x75, 0x65, 0x50, 0x6D, 0x79, 0x32, 0x6D, 0x4F,
	0x54, 0x31, 0x54, 0x39, 0x6F, 0x31, 0x61, 0x47, 0x55, 0x49, 0x6C, 0x66, 0x38, 0x72, 0x76,
	0x33, 0x2F, 0x30, 0x45, 0x78, 0x67, 0x53, 0x6B, 0x57, 0x50, 0x6D, 0x4F, 0x41, 0x38, 0x35,
	0x49, 0x32, 0x2F, 0x58, 0x48, 0x65, 0x66, 0x71, 0x54, 0x6F, 0x45, 0x48, 0x30, 0x44, 0x65,
	0x41, 0x4E, 0x0A, 0x7A, 0x6C, 0x4B, 0x4C, 0x71, 0x79, 0x44, 0x56, 0x30, 0x42, 0x56, 0x4E,
	0x76, 0x48, 0x42, 0x57, 0x79, 0x32, 0x49, 0x51, 0x35, 0x62, 0x50, 0x42, 0x57, 0x76, 0x30,
	0x37, 0x63, 0x34, 0x2B, 0x6A, 0x39, 0x4E, 0x62, 0x57, 0x67, 0x64, 0x44, 0x43, 0x43, 0x35,
	0x52, 0x6B, 0x4F, 0x6A, 0x70, 0x33, 0x4D, 0x4E, 0x45, 0x58, 0x47, 0x56, 0x43, 0x69, 0x51,
	0x51, 0x4B, 0x42, 0x67, 0x43, 0x7A, 0x4D, 0x0A, 0x77, 0x65, 0x61, 0x62, 0x73, 0x50, 0x48,
	0x68, 0x44, 0x4B, 0x5A, 0x38, 0x2F, 0x34, 0x43, 0x6A, 0x73, 0x61, 0x62, 0x4E, 0x75, 0x41,
	0x7A, 0x62, 0x57, 0x4B, 0x52, 0x42, 0x38, 0x37, 0x44, 0x61, 0x58, 0x46, 0x78, 0x6F, 0x4D,
	0x73, 0x35, 0x52, 0x79, 0x6F, 0x38, 0x55, 0x4D, 0x6B, 0x72, 0x67, 0x30, 0x35, 0x4C, 0x6F,
	0x67, 0x37, 0x4D, 0x78, 0x62, 0x33, 0x76, 0x61, 0x42, 0x34, 0x63, 0x2F, 0x0A, 0x52, 0x57,
	0x77, 0x7A, 0x38, 0x72, 0x34, 0x39, 0x70, 0x48, 0x64, 0x71, 0x68, 0x4F, 0x6D, 0x63, 0x6C,
	0x45, 0x77, 0x79, 0x4D, 0x34, 0x51, 0x79, 0x6A, 0x39, 0x52, 0x6D, 0x57, 0x62, 0x51, 0x58,
	0x54, 0x54, 0x45, 0x63, 0x2B, 0x35, 0x67, 0x54, 0x4B, 0x50, 0x4E, 0x53, 0x33, 0x6D, 0x70,
	0x4D, 0x54, 0x36, 0x39, 0x46, 0x45, 0x74, 0x2F, 0x35, 0x72, 0x4D, 0x52, 0x70, 0x4B, 0x2B,
	0x52, 0x68, 0x0A, 0x49, 0x32, 0x42, 0x58, 0x6B, 0x51, 0x71, 0x31, 0x36, 0x6E, 0x72, 0x31,
	0x61, 0x45, 0x4D, 0x6D, 0x64, 0x51, 0x42, 0x51, 0x79, 0x4B, 0x59, 0x4A, 0x6C, 0x30, 0x6C,
	0x50, 0x68, 0x69, 0x42, 0x2F, 0x75, 0x6C, 0x5A, 0x63, 0x72, 0x67, 0x4C, 0x70, 0x41, 0x6F,
	0x47, 0x41, 0x65, 0x30, 0x65, 0x74, 0x50, 0x4A, 0x77, 0x6D, 0x51, 0x46, 0x6B, 0x6A, 0x4D,
	0x70, 0x66, 0x4D, 0x44, 0x61, 0x4E, 0x34, 0x0A, 0x70, 0x7A, 0x71, 0x45, 0x51, 0x72, 0x52,
	0x35, 0x4B, 0x35, 0x4D, 0x6E, 0x54, 0x48, 0x76, 0x47, 0x67, 0x2F, 0x70, 0x6A, 0x57, 0x6A,
	0x43, 0x57, 0x58, 0x56, 0x48, 0x67, 0x35, 0x76, 0x36, 0x46, 0x6F, 0x5A, 0x48, 0x35, 0x6E,
	0x59, 0x2B, 0x56, 0x2F, 0x57, 0x75, 0x57, 0x38, 0x38, 0x6A, 0x6C, 0x4B, 0x53, 0x50, 0x6C,
	0x77, 0x6A, 0x50, 0x7A, 0x41, 0x67, 0x7A, 0x47, 0x33, 0x45, 0x41, 0x55, 0x0A, 0x71, 0x57,
	0x6B, 0x42, 0x67, 0x30, 0x71, 0x75, 0x50, 0x4D, 0x72, 0x54, 0x6B, 0x73, 0x69, 0x6E, 0x58,
	0x50, 0x2B, 0x58, 0x6B, 0x51, 0x65, 0x46, 0x66, 0x58, 0x61, 0x33, 0x38, 0x6A, 0x72, 0x70,
	0x62, 0x4B, 0x46, 0x4F, 0x72, 0x7A, 0x49, 0x6F, 0x6A, 0x69, 0x65, 0x6C, 0x4B, 0x55, 0x4D,
	0x50, 0x4D, 0x78, 0x2F, 0x78, 0x70, 0x53, 0x6A, 0x63, 0x55, 0x42, 0x68, 0x62, 0x4E, 0x34,
	0x45, 0x54, 0x0A, 0x4F, 0x30, 0x66, 0x63, 0x57, 0x47, 0x6F, 0x61, 0x56, 0x50, 0x72, 0x63,
	0x6E, 0x38, 0x62, 0x58, 0x4D, 0x54, 0x45, 0x4E, 0x53, 0x31, 0x41, 0x3D, 0x0A, 0x2D, 0x2D,
	0x2D, 0x2D, 0x2D, 0x45, 0x4E, 0x44, 0x20, 0x50, 0x52, 0x49, 0x56, 0x41, 0x54, 0x45, 0x20,
	0x4B, 0x45, 0x59, 0x2D, 0x2D, 0x2D, 0x2D, 0x2D, 0x0A
};

static const BYTE test_DummyMessage[64] =
{
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
	0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
	0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD
};

static const BYTE test_LastDummyMessage[64] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int schannel_send(PSecurityFunctionTable table, HANDLE hPipe, PCtxtHandle phContext,
                         BYTE* buffer, UINT32 length)
{
	BYTE* ioBuffer;
	UINT32 ioBufferLength;
	BYTE* pMessageBuffer;
	SecBuffer Buffers[4];
	SecBufferDesc Message;
	SECURITY_STATUS status;
	DWORD NumberOfBytesWritten;
	SecPkgContext_StreamSizes StreamSizes;
	ZeroMemory(&StreamSizes, sizeof(SecPkgContext_StreamSizes));
	status = table->QueryContextAttributes(phContext, SECPKG_ATTR_STREAM_SIZES, &StreamSizes);
	ioBufferLength = StreamSizes.cbHeader + StreamSizes.cbMaximumMessage + StreamSizes.cbTrailer;
	ioBuffer = (BYTE*) calloc(1, ioBufferLength);

	if (!ioBuffer)
		return -1;

	pMessageBuffer = ioBuffer + StreamSizes.cbHeader;
	CopyMemory(pMessageBuffer, buffer, length);
	Buffers[0].pvBuffer = ioBuffer;
	Buffers[0].cbBuffer = StreamSizes.cbHeader;
	Buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
	Buffers[1].pvBuffer = pMessageBuffer;
	Buffers[1].cbBuffer = length;
	Buffers[1].BufferType = SECBUFFER_DATA;
	Buffers[2].pvBuffer = pMessageBuffer + length;
	Buffers[2].cbBuffer = StreamSizes.cbTrailer;
	Buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
	Buffers[3].pvBuffer = NULL;
	Buffers[3].cbBuffer = 0;
	Buffers[3].BufferType = SECBUFFER_EMPTY;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.cBuffers = 4;
	Message.pBuffers = Buffers;
	ioBufferLength = Message.pBuffers[0].cbBuffer + Message.pBuffers[1].cbBuffer +
	                 Message.pBuffers[2].cbBuffer;
	status = table->EncryptMessage(phContext, 0, &Message, 0);
	printf("EncryptMessage status: 0x%08"PRIX32"\n", status);
	printf("EncryptMessage output: cBuffers: %"PRIu32" [0]: %"PRIu32" / %"PRIu32" [1]: %"PRIu32" / %"PRIu32" [2]: %"PRIu32" / %"PRIu32" [3]: %"PRIu32" / %"PRIu32"\n",
	       Message.cBuffers,
	       Message.pBuffers[0].cbBuffer, Message.pBuffers[0].BufferType,
	       Message.pBuffers[1].cbBuffer, Message.pBuffers[1].BufferType,
	       Message.pBuffers[2].cbBuffer, Message.pBuffers[2].BufferType,
	       Message.pBuffers[3].cbBuffer, Message.pBuffers[3].BufferType);

	if (status != SEC_E_OK)
		return -1;

	printf("Client > Server (%"PRIu32")\n", ioBufferLength);
	winpr_HexDump("sspi.test", WLOG_DEBUG, ioBuffer, ioBufferLength);

	if (!WriteFile(hPipe, ioBuffer, ioBufferLength, &NumberOfBytesWritten, NULL))
	{
		printf("schannel_send: failed to write to pipe\n");
		return -1;
	}

	return 0;
}

static int schannel_recv(PSecurityFunctionTable table, HANDLE hPipe, PCtxtHandle phContext)
{
	BYTE* ioBuffer;
	UINT32 ioBufferLength;
	//BYTE* pMessageBuffer;
	SecBuffer Buffers[4];
	SecBufferDesc Message;
	SECURITY_STATUS status;
	DWORD NumberOfBytesRead;
	SecPkgContext_StreamSizes StreamSizes;
	ZeroMemory(&StreamSizes, sizeof(SecPkgContext_StreamSizes));
	status = table->QueryContextAttributes(phContext, SECPKG_ATTR_STREAM_SIZES, &StreamSizes);
	ioBufferLength = StreamSizes.cbHeader + StreamSizes.cbMaximumMessage + StreamSizes.cbTrailer;
	ioBuffer = (BYTE*) calloc(1, ioBufferLength);

	if (!ioBuffer)
		return -1;

	if (!ReadFile(hPipe, ioBuffer, ioBufferLength, &NumberOfBytesRead, NULL))
	{
		printf("schannel_recv: failed to read from pipe\n");
		return -1;
	}

	Buffers[0].pvBuffer = ioBuffer;
	Buffers[0].cbBuffer = NumberOfBytesRead;
	Buffers[0].BufferType = SECBUFFER_DATA;
	Buffers[1].pvBuffer = NULL;
	Buffers[1].cbBuffer = 0;
	Buffers[1].BufferType = SECBUFFER_EMPTY;
	Buffers[2].pvBuffer = NULL;
	Buffers[2].cbBuffer = 0;
	Buffers[2].BufferType = SECBUFFER_EMPTY;
	Buffers[3].pvBuffer = NULL;
	Buffers[3].cbBuffer = 0;
	Buffers[3].BufferType = SECBUFFER_EMPTY;
	Message.ulVersion = SECBUFFER_VERSION;
	Message.cBuffers = 4;
	Message.pBuffers = Buffers;
	status = table->DecryptMessage(phContext, &Message, 0, NULL);
	printf("DecryptMessage status: 0x%08"PRIX32"\n", status);
	printf("DecryptMessage output: cBuffers: %"PRIu32" [0]: %"PRIu32" / %"PRIu32" [1]: %"PRIu32" / %"PRIu32" [2]: %"PRIu32" / %"PRIu32" [3]: %"PRIu32" / %"PRIu32"\n",
	       Message.cBuffers,
	       Message.pBuffers[0].cbBuffer, Message.pBuffers[0].BufferType,
	       Message.pBuffers[1].cbBuffer, Message.pBuffers[1].BufferType,
	       Message.pBuffers[2].cbBuffer, Message.pBuffers[2].BufferType,
	       Message.pBuffers[3].cbBuffer, Message.pBuffers[3].BufferType);

	if (status != SEC_E_OK)
		return -1;

	printf("Decrypted Message (%"PRIu32")\n", Message.pBuffers[1].cbBuffer);
	winpr_HexDump("sspi.test", WLOG_DEBUG, (BYTE*) Message.pBuffers[1].pvBuffer,
	              Message.pBuffers[1].cbBuffer);

	if (memcmp(Message.pBuffers[1].pvBuffer, test_LastDummyMessage, sizeof(test_LastDummyMessage)) == 0)
		return -1;

	return 0;
}

static DWORD WINAPI schannel_test_server_thread(LPVOID arg)
{
	BOOL extraData;
	BYTE* lpTokenIn;
	BYTE* lpTokenOut;
	TimeStamp expiry;
	UINT32 cbMaxToken;
	UINT32 fContextReq;
	ULONG fContextAttr;
	SCHANNEL_CRED cred;
	CtxtHandle context;
	CredHandle credentials;
	DWORD cchNameString;
	LPTSTR pszNameString;
	HCERTSTORE hCertStore;
	PCCERT_CONTEXT pCertContext;
	PSecBuffer pSecBuffer;
	SecBuffer SecBuffer_in[2];
	SecBuffer SecBuffer_out[2];
	SecBufferDesc SecBufferDesc_in;
	SecBufferDesc SecBufferDesc_out;
	DWORD NumberOfBytesRead;
	SECURITY_STATUS status;
	PSecPkgInfo pPackageInfo;
	PSecurityFunctionTable table;
	DWORD NumberOfBytesWritten;
	printf("Starting Server\n");
	SecInvalidateHandle(&context);
	SecInvalidateHandle(&credentials);
	table = InitSecurityInterface();
	status = QuerySecurityPackageInfo(SCHANNEL_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo failure: 0x%08"PRIX32"\n", status);
		return 0;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;
	hCertStore = CertOpenSystemStore(0, _T("MY"));

	if (!hCertStore)
	{
		printf("Error opening system store\n");
		//return NULL;
	}

#ifdef CERT_FIND_HAS_PRIVATE_KEY
	pCertContext = CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING, 0,
	               CERT_FIND_HAS_PRIVATE_KEY, NULL, NULL);
#else
	pCertContext = CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING, 0, CERT_FIND_ANY, NULL,
	               NULL);
#endif

	if (!pCertContext)
	{
		printf("Error finding certificate in store\n");
		//return NULL;
	}

	cchNameString = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0);
	pszNameString = (LPTSTR) malloc(cchNameString * sizeof(TCHAR));

	if (!pszNameString)
	{
		printf("Memory allocation failed\n");
		return 0;
	}

	cchNameString = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL,
	                                  pszNameString, cchNameString);
	_tprintf(_T("Certificate Name: %s\n"), pszNameString);
	ZeroMemory(&cred, sizeof(SCHANNEL_CRED));
	cred.dwVersion = SCHANNEL_CRED_VERSION;
	cred.cCreds = 1;
	cred.paCred = &pCertContext;
	cred.cSupportedAlgs = 0;
	cred.palgSupportedAlgs = NULL;
	cred.grbitEnabledProtocols = SP_PROT_TLS1_SERVER;
	cred.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER;
	status = table->AcquireCredentialsHandle(NULL, SCHANNEL_NAME,
	         SECPKG_CRED_INBOUND, NULL, &cred, NULL, NULL, &credentials, NULL);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle failure: 0x%08"PRIX32"\n", status);
		return 0;
	}

	extraData = FALSE;
	g_ServerWait = TRUE;

	if (!(lpTokenIn = (BYTE*) malloc(cbMaxToken)))
	{
		printf("Memory allocation failed\n");
		return 0;
	}

	if (!(lpTokenOut = (BYTE*) malloc(cbMaxToken)))
	{
		printf("Memory allocation failed\n");
		free(lpTokenIn);
		return 0;
	}

	fContextReq = ASC_REQ_STREAM |
	              ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT |
	              ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR;

	do
	{
		if (!extraData)
		{
			if (g_ServerWait)
			{
				if (!ReadFile(g_ServerReadPipe, lpTokenIn, cbMaxToken, &NumberOfBytesRead, NULL))
				{
					printf("Failed to read from server pipe\n");
					return NULL;
				}
			}
			else
			{
				NumberOfBytesRead = 0;
			}
		}

		extraData = FALSE;
		g_ServerWait = TRUE;
		SecBuffer_in[0].BufferType = SECBUFFER_TOKEN;
		SecBuffer_in[0].pvBuffer = lpTokenIn;
		SecBuffer_in[0].cbBuffer = NumberOfBytesRead;
		SecBuffer_in[1].BufferType = SECBUFFER_EMPTY;
		SecBuffer_in[1].pvBuffer = NULL;
		SecBuffer_in[1].cbBuffer = 0;
		SecBufferDesc_in.ulVersion = SECBUFFER_VERSION;
		SecBufferDesc_in.cBuffers = 2;
		SecBufferDesc_in.pBuffers = SecBuffer_in;
		SecBuffer_out[0].BufferType = SECBUFFER_TOKEN;
		SecBuffer_out[0].pvBuffer = lpTokenOut;
		SecBuffer_out[0].cbBuffer = cbMaxToken;
		SecBufferDesc_out.ulVersion = SECBUFFER_VERSION;
		SecBufferDesc_out.cBuffers = 1;
		SecBufferDesc_out.pBuffers = SecBuffer_out;
		status = table->AcceptSecurityContext(&credentials, SecIsValidHandle(&context) ? &context : NULL,
		                                      &SecBufferDesc_in, fContextReq, 0, &context, &SecBufferDesc_out, &fContextAttr, &expiry);

		if ((status != SEC_E_OK) && (status != SEC_I_CONTINUE_NEEDED) &&
		    (status != SEC_E_INCOMPLETE_MESSAGE))
		{
			printf("AcceptSecurityContext unexpected status: 0x%08"PRIX32"\n", status);
			return NULL;
		}

		NumberOfBytesWritten = 0;

		if (status == SEC_E_OK)
			printf("AcceptSecurityContext status: SEC_E_OK\n");
		else if (status == SEC_I_CONTINUE_NEEDED)
			printf("AcceptSecurityContext status: SEC_I_CONTINUE_NEEDED\n");
		else if (status == SEC_E_INCOMPLETE_MESSAGE)
			printf("AcceptSecurityContext status: SEC_E_INCOMPLETE_MESSAGE\n");

		printf("Server cBuffers: %"PRIu32" pBuffers[0]: %"PRIu32" type: %"PRIu32"\n",
		       SecBufferDesc_out.cBuffers, SecBufferDesc_out.pBuffers[0].cbBuffer,
		       SecBufferDesc_out.pBuffers[0].BufferType);
		printf("Server Input cBuffers: %"PRIu32" pBuffers[0]: %"PRIu32" type: %"PRIu32" pBuffers[1]: %"PRIu32" type: %"PRIu32"\n",
		       SecBufferDesc_in.cBuffers,
		       SecBufferDesc_in.pBuffers[0].cbBuffer, SecBufferDesc_in.pBuffers[0].BufferType,
		       SecBufferDesc_in.pBuffers[1].cbBuffer, SecBufferDesc_in.pBuffers[1].BufferType);

		if (SecBufferDesc_in.pBuffers[1].BufferType == SECBUFFER_EXTRA)
		{
			printf("AcceptSecurityContext SECBUFFER_EXTRA\n");
			pSecBuffer = &SecBufferDesc_in.pBuffers[1];
			CopyMemory(lpTokenIn, &lpTokenIn[NumberOfBytesRead - pSecBuffer->cbBuffer], pSecBuffer->cbBuffer);
			NumberOfBytesRead = pSecBuffer->cbBuffer;
			continue;
		}

		if (status != SEC_E_INCOMPLETE_MESSAGE)
		{
			pSecBuffer = &SecBufferDesc_out.pBuffers[0];

			if (pSecBuffer->cbBuffer > 0)
			{
				printf("Server > Client (%"PRIu32")\n", pSecBuffer->cbBuffer);
				winpr_HexDump("sspi.test", WLOG_DEBUG, (BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);

				if (!WriteFile(g_ClientWritePipe, pSecBuffer->pvBuffer, pSecBuffer->cbBuffer, &NumberOfBytesWritten,
				               NULL))
				{
					printf("failed to write to client pipe\n");
					return NULL;
				}
			}
		}

		if (status == SEC_E_OK)
		{
			printf("Server Handshake Complete\n");
			break;
		}
	}
	while (1);

	do
	{
		if (schannel_recv(table, g_ServerReadPipe, &context) < 0)
			break;
	}
	while (1);

	return 0;
}

static int dump_test_certificate_files(void)
{
	FILE* fp;
	char* fullpath = NULL;
	int ret = -1;
	/*
	 * Output Certificate File
	 */
	fullpath = GetCombinedPath("/tmp", "localhost.crt");

	if (!fullpath)
		return -1;

	fp = fopen(fullpath, "w+");

	if (fp)
	{
		if (fwrite((void*) test_localhost_crt, sizeof(test_localhost_crt), 1, fp) != 1)
			goto out_fail;

		fclose(fp);
		fp = NULL;
	}

	free(fullpath);
	/*
	 * Output Private Key File
	 */
	fullpath = GetCombinedPath("/tmp", "localhost.key");

	if (!fullpath)
		return -1;

	fp = fopen(fullpath, "w+");

	if (fp && fwrite((void*) test_localhost_key, sizeof(test_localhost_key), 1, fp) != 1)
		goto out_fail;

	ret = 1;
out_fail:
	free(fullpath);

	if (fp)
		fclose(fp);

	return ret;
}

int TestSchannel(int argc, char* argv[])
{
	int count;
	DWORD index;
	ALG_ID algId;
	HANDLE thread;
	BYTE* lpTokenIn;
	BYTE* lpTokenOut;
	TimeStamp expiry;
	UINT32 cbMaxToken;
	SCHANNEL_CRED cred;
	UINT32 fContextReq;
	ULONG fContextAttr;
	CtxtHandle context;
	CredHandle credentials;
	SECURITY_STATUS status;
	PSecPkgInfo pPackageInfo;
	PSecBuffer pSecBuffer;
	SecBuffer SecBuffer_in[2];
	SecBuffer SecBuffer_out[1];
	SecBufferDesc SecBufferDesc_in;
	SecBufferDesc SecBufferDesc_out;
	PSecurityFunctionTable table;
	DWORD NumberOfBytesRead;
	DWORD NumberOfBytesWritten;
	SecPkgCred_SupportedAlgs SupportedAlgs;
	SecPkgCred_CipherStrengths CipherStrengths;
	SecPkgCred_SupportedProtocols SupportedProtocols;
	return 0; /* disable by default - causes crash */
	sspi_GlobalInit();
	dump_test_certificate_files();
	SecInvalidateHandle(&context);
	SecInvalidateHandle(&credentials);

	if (!CreatePipe(&g_ClientReadPipe, &g_ClientWritePipe, NULL, 0))
	{
		printf("Failed to create client pipe\n");
		return -1;
	}

	if (!CreatePipe(&g_ServerReadPipe, &g_ServerWritePipe, NULL, 0))
	{
		printf("Failed to create server pipe\n");
		return -1;
	}

	if (!(thread = CreateThread(NULL, 0, schannel_test_server_thread, NULL, 0, NULL)))
	{
		printf("Failed to create server thread\n");
		return -1;
	}

	table = InitSecurityInterface();
	status = QuerySecurityPackageInfo(SCHANNEL_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo failure: 0x%08"PRIX32"\n", status);
		return -1;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;
	ZeroMemory(&cred, sizeof(SCHANNEL_CRED));
	cred.dwVersion = SCHANNEL_CRED_VERSION;
	cred.cCreds = 0;
	cred.paCred = NULL;
	cred.cSupportedAlgs = 0;
	cred.palgSupportedAlgs = NULL;
	cred.grbitEnabledProtocols = SP_PROT_SSL3TLS1_CLIENTS;
	cred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;
	cred.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
	cred.dwFlags |= SCH_CRED_NO_SERVERNAME_CHECK;
	status = table->AcquireCredentialsHandle(NULL, SCHANNEL_NAME,
	         SECPKG_CRED_OUTBOUND, NULL, &cred, NULL, NULL, &credentials, NULL);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle failure: 0x%08"PRIX32"\n", status);
		return -1;
	}

	ZeroMemory(&SupportedAlgs, sizeof(SecPkgCred_SupportedAlgs));
	status = table->QueryCredentialsAttributes(&credentials, SECPKG_ATTR_SUPPORTED_ALGS,
	         &SupportedAlgs);

	if (status != SEC_E_OK)
	{
		printf("QueryCredentialsAttributes SECPKG_ATTR_SUPPORTED_ALGS failure: 0x%08"PRIX32"\n", status);
		return -1;
	}

	/**
	 * SupportedAlgs: 15
	 * 0x660E 0x6610 0x6801 0x6603 0x6601 0x8003 0x8004
	 * 0x800C 0x800D 0x800E 0x2400 0xAA02 0xAE06 0x2200 0x2203
	 */
	printf("SupportedAlgs: %"PRIu32"\n", SupportedAlgs.cSupportedAlgs);

	for (index = 0; index < SupportedAlgs.cSupportedAlgs; index++)
	{
		algId = SupportedAlgs.palgSupportedAlgs[index];
		printf("\t0x%08"PRIX32" CLASS: %"PRIu32" TYPE: %"PRIu32" SID: %"PRIu32"\n", algId,
		       ((GET_ALG_CLASS(algId)) >> 13), ((GET_ALG_TYPE(algId)) >> 9), GET_ALG_SID(algId));
	}

	printf("\n");
	ZeroMemory(&CipherStrengths, sizeof(SecPkgCred_CipherStrengths));
	status = table->QueryCredentialsAttributes(&credentials, SECPKG_ATTR_CIPHER_STRENGTHS,
	         &CipherStrengths);

	if (status != SEC_E_OK)
	{
		printf("QueryCredentialsAttributes SECPKG_ATTR_CIPHER_STRENGTHS failure: 0x%08"PRIX32"\n", status);
		return -1;
	}

	/* CipherStrengths: Minimum: 40 Maximum: 256 */
	printf("CipherStrengths: Minimum: %"PRIu32" Maximum: %"PRIu32"\n",
	       CipherStrengths.dwMinimumCipherStrength, CipherStrengths.dwMaximumCipherStrength);
	ZeroMemory(&SupportedProtocols, sizeof(SecPkgCred_SupportedProtocols));
	status = table->QueryCredentialsAttributes(&credentials, SECPKG_ATTR_SUPPORTED_PROTOCOLS,
	         &SupportedProtocols);

	if (status != SEC_E_OK)
	{
		printf("QueryCredentialsAttributes SECPKG_ATTR_SUPPORTED_PROTOCOLS failure: 0x%08"PRIX32"\n",
		       status);
		return -1;
	}

	/* SupportedProtocols: 0x208A0 */
	printf("SupportedProtocols: 0x%08"PRIX32"\n", SupportedProtocols.grbitProtocol);
	fContextReq = ISC_REQ_STREAM |
	              ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
	              ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR |
	              ISC_REQ_MANUAL_CRED_VALIDATION | ISC_REQ_INTEGRITY;

	if (!(lpTokenIn = (BYTE*) malloc(cbMaxToken)))
	{
		printf("Memory allocation failed\n");
		return -1;
	}

	if (!(lpTokenOut = (BYTE*) malloc(cbMaxToken)))
	{
		printf("Memory allocation failed\n");
		return -1;
	}

	ZeroMemory(&SecBuffer_in, sizeof(SecBuffer_in));
	ZeroMemory(&SecBuffer_out, sizeof(SecBuffer_out));
	ZeroMemory(&SecBufferDesc_in, sizeof(SecBufferDesc));
	ZeroMemory(&SecBufferDesc_out, sizeof(SecBufferDesc));
	g_ClientWait = FALSE;

	do
	{
		if (g_ClientWait)
		{
			if (!ReadFile(g_ClientReadPipe, lpTokenIn, cbMaxToken, &NumberOfBytesRead, NULL))
			{
				printf("failed to read from server pipe\n");
				return -1;
			}
		}
		else
		{
			NumberOfBytesRead = 0;
		}

		g_ClientWait = TRUE;
		printf("NumberOfBytesRead: %"PRIu32"\n", NumberOfBytesRead);
		SecBuffer_in[0].BufferType = SECBUFFER_TOKEN;
		SecBuffer_in[0].pvBuffer = lpTokenIn;
		SecBuffer_in[0].cbBuffer = NumberOfBytesRead;
		SecBuffer_in[1].pvBuffer   = NULL;
		SecBuffer_in[1].cbBuffer   = 0;
		SecBuffer_in[1].BufferType = SECBUFFER_EMPTY;
		SecBufferDesc_in.ulVersion = SECBUFFER_VERSION;
		SecBufferDesc_in.cBuffers = 2;
		SecBufferDesc_in.pBuffers = SecBuffer_in;
		SecBuffer_out[0].BufferType = SECBUFFER_TOKEN;
		SecBuffer_out[0].pvBuffer = lpTokenOut;
		SecBuffer_out[0].cbBuffer = cbMaxToken;
		SecBufferDesc_out.ulVersion = SECBUFFER_VERSION;
		SecBufferDesc_out.cBuffers = 1;
		SecBufferDesc_out.pBuffers = SecBuffer_out;
		status = table->InitializeSecurityContext(&credentials,
		         SecIsValidHandle(&context) ? &context : NULL, _T("localhost"),
		         fContextReq, 0, 0, &SecBufferDesc_in, 0, &context, &SecBufferDesc_out, &fContextAttr, &expiry);

		if ((status != SEC_E_OK) && (status != SEC_I_CONTINUE_NEEDED) &&
		    (status != SEC_E_INCOMPLETE_MESSAGE))
		{
			printf("InitializeSecurityContext unexpected status: 0x%08"PRIX32"\n", status);
			return -1;
		}

		NumberOfBytesWritten = 0;

		if (status == SEC_E_OK)
			printf("InitializeSecurityContext status: SEC_E_OK\n");
		else if (status == SEC_I_CONTINUE_NEEDED)
			printf("InitializeSecurityContext status: SEC_I_CONTINUE_NEEDED\n");
		else if (status == SEC_E_INCOMPLETE_MESSAGE)
			printf("InitializeSecurityContext status: SEC_E_INCOMPLETE_MESSAGE\n");

		printf("Client Output cBuffers: %"PRIu32" pBuffers[0]: %"PRIu32" type: %"PRIu32"\n",
		       SecBufferDesc_out.cBuffers, SecBufferDesc_out.pBuffers[0].cbBuffer,
		       SecBufferDesc_out.pBuffers[0].BufferType);
		printf("Client Input cBuffers: %"PRIu32" pBuffers[0]: %"PRIu32" type: %"PRIu32" pBuffers[1]: %"PRIu32" type: %"PRIu32"\n",
		       SecBufferDesc_in.cBuffers,
		       SecBufferDesc_in.pBuffers[0].cbBuffer, SecBufferDesc_in.pBuffers[0].BufferType,
		       SecBufferDesc_in.pBuffers[1].cbBuffer, SecBufferDesc_in.pBuffers[1].BufferType);

		if (status != SEC_E_INCOMPLETE_MESSAGE)
		{
			pSecBuffer = &SecBufferDesc_out.pBuffers[0];

			if (pSecBuffer->cbBuffer > 0)
			{
				printf("Client > Server (%"PRIu32")\n", pSecBuffer->cbBuffer);
				winpr_HexDump("sspi.test", WLOG_DEBUG, (BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);

				if (!WriteFile(g_ServerWritePipe, pSecBuffer->pvBuffer, pSecBuffer->cbBuffer, &NumberOfBytesWritten,
				               NULL))
				{
					printf("failed to write to server pipe\n");
					return -1;
				}
			}
		}

		if (status == SEC_E_OK)
		{
			printf("Client Handshake Complete\n");
			break;
		}
	}
	while (1);

	count = 0;

	do
	{
		if (schannel_send(table, g_ServerWritePipe, &context, test_DummyMessage,
		                  sizeof(test_DummyMessage)) < 0)
			break;

		for (index = 0; index < sizeof(test_DummyMessage); index++)
		{
			BYTE b, ln, hn;
			b = test_DummyMessage[index];
			ln = (b & 0x0F);
			hn = ((b & 0xF0) >> 4);
			ln = (ln + 1) % 0xF;
			hn = (ln + 1) % 0xF;
			b = (ln | (hn << 4));
			test_DummyMessage[index] = b;
		}

		Sleep(100);
		count++;
	}
	while (count < 3);

	schannel_send(table, g_ServerWritePipe, &context, test_LastDummyMessage,
	              sizeof(test_LastDummyMessage));
	WaitForSingleObject(thread, INFINITE);
	sspi_GlobalFinish();
	return 0;
}

