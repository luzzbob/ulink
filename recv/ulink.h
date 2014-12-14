#ifndef __ulink_h__
#define __ulink_h__


#define RECV_TIMEOUT_MIN 3

/*
 * try recv data using ulink.
 *
 * dev: device name for wireless device, eg: wlan0
 * timeout: recv timeout in seconds.
 * data: data pointer [out]
 * size: out data length
 *
 * return: 1 if got new data. 0 for timeout. -1 for error.
 *
 * if got new data. you should free(*data) by yourself.
 *
 */
int ulink_recv(const char *dev, int timeout, unsigned char **data, size_t *size);

#if 0
/* async recv callback
 *
 * context: context by ulink_recv_async.
 * events: 1 for new data, 0 for timeout, -1 for error.
 * data: new incoming data.
 * size: new data length.
 *
 */
typedef void(*ulink_recv_async_callback)(void *context, int events, unsigned char *data, size_t size);

/*
 * async recv
 * 
 * dev: device name for wireless device, eg: wlan0
 * timeout: recv timeout in seconds.
 *
 * return: context, NULL if failed.
 */
void *ulink_recv_async(const char *dev, int timeout);

/*
 * cleanup recv context.
 */
void ulink_recv_cleanup(void *context);

#endif

#endif /* __ulink_h__ */

