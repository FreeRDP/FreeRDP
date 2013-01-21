
#include <winpr/crt.h>
#include <winpr/pool.h>

/**
 * Improve Scalability With New Thread Pool APIs:
 * http://msdn.microsoft.com/en-us/magazine/cc16332.aspx
 *
 * Developing with Thread Pool Enhancements:
 * http://msdn.microsoft.com/en-us/library/cc308561.aspx
 *
 * Introduction to the Windows Threadpool:
 * http://blogs.msdn.com/b/harip/archive/2010/10/11/introduction-to-the-windows-threadpool-part-1.aspx
 * http://blogs.msdn.com/b/harip/archive/2010/10/12/introduction-to-the-windows-threadpool-part-2.aspx
 */

int TestPoolThread(int argc, char* argv[])
{
	TP_POOL* pool;

	pool = CreateThreadpool(NULL);

	SetThreadpoolThreadMinimum(pool, 8); /* default is 0 */
	SetThreadpoolThreadMaximum(pool, 64); /* default is 500 */

	CloseThreadpool(pool);

	return 0;
}
