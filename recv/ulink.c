#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pcap/pcap.h>

#include <linux/wireless.h>

#include "ulink.h"

#define CHANNEL_SCAN_TIME (300)
#define CHANNEL_READ_TIME (3000)
#define CHANNEL_SCAN_PACKET_COUNT (20)

#define LOGL_(l,fmt...) do { \
    if (l > 0) { \
        char mess_[1024]; \
        time_t t_; \
        struct tm tmp_; \
        t_ = time(NULL); \
        localtime_r(&t_, &tmp_); \
        char ts_[64]; \
        strftime(ts_, sizeof(ts_), "%y-%m-%d %H:%M:%S", &tmp_); \
        int n_ = sprintf(mess_, "[%s] [%s:%d] ", ts_, __FILE__, __LINE__); \
        n_ += sprintf(&mess_[n_], ##fmt); \
        if (n_ < (int)sizeof(mess_) - 2) { \
            if (mess_[n_-1] != '\n') {\
                mess_[n_++] = '\n'; \
                mess_[n_++] = '\0'; \
            } \
            printf(mess_); \
        } \
    } \
} while (0)

static int debug=0;
#define LOG_(fmt...) LOGL_(1,##fmt)
#define LOGD_(fmt...) LOGL_(debug,##fmt)

typedef struct ulink_recv_
{
    pcap_t *pcap_handle;
    struct bpf_program bpf;
} ulink_recv_t;

static ulink_recv_t *ulink_recv_open(const char *netdev)
{
	int err;
    ulink_recv_t *t = NULL;

    if (getenv("debug") != NULL)
    {
        debug = 1;
    }
    
    t = (ulink_recv_t *)malloc(sizeof(ulink_recv_t));
    memset(t, 0, sizeof(ulink_recv_t));

    /* init the pcap */
    {
        char errbuf[PCAP_ERRBUF_SIZE];
        t->pcap_handle = pcap_create(netdev, errbuf);
        if (NULL == t->pcap_handle)
        {
            LOG_("pcap_open_live failed. %s", errbuf);
            free(t);
            return NULL;
        }

        err = pcap_set_snaplen(t->pcap_handle, 128);
        if (err)
        {
            LOG_("pcap_set_snaplen failed: %s", pcap_statustostr(err));
            pcap_close(t->pcap_handle);
            free(t);
            return NULL;
        }

        err = pcap_set_rfmon(t->pcap_handle, 1);
        if (err)
        {
            LOG_("pcap_set_rfmon failed: %s", pcap_statustostr(err));
            pcap_close(t->pcap_handle);
            free(t);
            return NULL;
        }


        err = pcap_set_timeout(t->pcap_handle, CHANNEL_SCAN_TIME);
        if (err)
        {
            LOG_("pcap_set_timeout failed: %s", pcap_statustostr(err));
            pcap_close(t->pcap_handle);
            free(t);
            return NULL;
        }

        err = pcap_activate(t->pcap_handle);
        if (err)
        {
            LOG_("pcap_activate failed: %s", pcap_statustostr(err));
            pcap_close(t->pcap_handle);
            free(t);
            return NULL;
        }

        /* filter */
        err = pcap_compile(t->pcap_handle, &t->bpf, "ether multicast and type data subtype qos-data or subtype data", 1, 0);
        if (err)
        {
            LOG_("pcap_compile failed: %s", pcap_geterr(t->pcap_handle));
            pcap_close(t->pcap_handle);
            free(t);
            return NULL;
        }


        err = pcap_setfilter(t->pcap_handle, &t->bpf);
        if (err)
        {
            LOG_("pcap_setfilter failed: %s", pcap_statustostr(err));
            pcap_freecode(&t->bpf);
            pcap_close(t->pcap_handle);
            free(t);
            return NULL;
        }

    }
    
	return t;
}


static void ulink_recv_close(ulink_recv_t *t)
{
    pcap_freecode(&t->bpf);
    pcap_close(t->pcap_handle);
    LOG_("ulink_recv_close: %p", t);
    free(t);
    return;
}

static void switch_channel(const char *dev, int channel)
{
    struct iwreq wrq;
    
    int sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        LOG_("socket failed. %d, %s", errno, strerror(errno));
        return;
    }

    memset(&wrq, 0, sizeof(struct iwreq));
    strncpy(wrq.ifr_name, dev, IFNAMSIZ);
    wrq.u.freq.m = (double) channel;
    wrq.u.freq.e = (double) 0;

    if(ioctl(sock, SIOCSIWFREQ, &wrq) < 0)
    {
        usleep(10000); /* madwifi needs a second chance */
        if(ioctl(sock, SIOCSIWFREQ, &wrq) < 0)
        {
            LOG_("change channel to %d failed. %d, %s", channel, errno, strerror(errno));
        }
        else
        {
            LOG_("retry to change channel to %d ok", channel);
        }
    }
    else
    {
        LOG_("change channel to %d ok", channel);
    }

    close(sock);
}


static inline unsigned long long get_timestamp()
{
    struct timeval ts;
    gettimeofday(&ts, NULL);
    return (ts.tv_sec * 1000) + (ts.tv_usec / 1000);
}


#define MAC_BUFF_COUNT (64)
typedef struct internal_data_
{
    pcap_t *handle;
    pthread_mutex_t mutex_;
    pthread_cond_t cond_;
    unsigned char smac[MAC_BUFF_COUNT][6];
    unsigned int  smac_count[MAC_BUFF_COUNT];
    unsigned int  smac_offset;
    unsigned char xmac[6];
    int flags;
    unsigned char data[256];
    unsigned char data_crc;
    int data_length;
    unsigned char xdata_flags[128];
    int  xdata_length;
    int xdata_read;
} internal_data_t;


static void ulink_recv_callback(unsigned char* data, const struct pcap_pkthdr *hdr, const unsigned char *pkt)
{
    internal_data_t *ud = (internal_data_t *) data;
    int radio_head_length = 0;
    const unsigned char *src_mac = NULL;
    const unsigned char *dst_mac = NULL;
    int subtype = 0;
    int i = 0;
    
    radio_head_length = pkt[2] & 0xff;

    // skip too short and too long.
    if ((hdr->len < radio_head_length + 22) || (hdr->len > radio_head_length + 300))
    {
        return;
    }

    subtype = (pkt[radio_head_length] & 0xf0) >> 4;
    if (subtype == 0)
    {
        // data
        src_mac = &pkt[radio_head_length+16];
        dst_mac = &pkt[radio_head_length+4];
    }
    else if (subtype == 0x08)
    {
        // qos-data
        src_mac = &pkt[radio_head_length+10];
        dst_mac = &pkt[radio_head_length+16];
    }
    else
    {
        return;
    }


    // check dest mac address if a multicast
    if (dst_mac[0] == 0x01 && 
        dst_mac[1] == 0x00 && 
        dst_mac[2] == 0x5e && 
        (dst_mac[3] & 0x80) == 0x00)
    {

        if (ud->flags == 0)
        {
            for (i=0; i<ud->smac_offset; i++)
            {
                /* source mac compare 10-15 */
                if (memcmp(src_mac, ud->smac[i], 6) == 0)
                {
                    ud->smac_count[i]++;
                    break;
                }
            }

            if (i == ud->smac_offset && ud->smac_offset < MAC_BUFF_COUNT)
            {
                // new mac
                memcpy(ud->smac[i], src_mac, 6);
                ud->smac_count[i] = 1;
                ud->smac_offset++;
            }


            /* check for flags*/
            int smax = 0;
            for(i=0; i<ud->smac_offset; i++)
            {
                if (ud->smac_count[i] > smax)
                {
                    smax = ud->smac_count[i];
                    memcpy(ud->xmac, ud->smac[i], 6);
                }
            }

            if (smax > CHANNEL_SCAN_PACKET_COUNT)
            {
                ud->flags = 1;
                LOG_("find ulink sender: %02x-%02x-%02x-%02x-%02x-%02x\n", 
                    ud->xmac[0] & 0xff, 
                    ud->xmac[1] & 0xff, 
                    ud->xmac[2] & 0xff, 
                    ud->xmac[3] & 0xff, 
                    ud->xmac[4] & 0xff, 
                    ud->xmac[5] & 0xff);
            }
        }
        else if(ud->flags == 1)
        {
            if (memcmp(src_mac, ud->xmac, 6) == 0)
            {
                unsigned char seq = dst_mac[3] & 0x7f; 
                unsigned char d1 =  dst_mac[4];
                unsigned char d2 =  dst_mac[5];
                LOGD_("recv: %02x %02x %02x", seq, d1, d2);

                if (ud->xdata_flags[seq] == 0)
                {
                    ud->xdata_read += 1;
                    ud->xdata_flags[seq] = 1;

                    if (seq == 0)
                    {
                        ud->data_crc = d2;
                        ud->xdata_length = (d1 + 1)/2 + 1;
                        ud->data_length = d1;
                    }
                    else
                    {
                        ud->data[seq*2-2] = d1;
                        ud->data[seq*2-1] = d2;
                    }
                    
                    if (ud->xdata_length > 0 && ud->xdata_length == ud->xdata_read)
                    {
                        int i = 0;
                        for (i=0; i<ud->data_length; i++)
                        {
                            ud->data_crc ^= ud->data[i];
                        }
                        if (ud->data_crc == 0)
                        {
                            ud->flags = 2;
                        }
                        else
                        {
                            LOG_("crc mismatch! %02x", ud->data_crc);
                            ud->flags = 1;
                        }
                        // notify break thread
                        pthread_mutex_lock(&ud->mutex_);
                        pthread_cond_signal(&ud->cond_);       
                        pthread_mutex_unlock(&ud->mutex_);
                    }
                }
            }
        }
    }
}

void *pcap_break_proc_(void *args)
{
    struct timeval now;
    struct timespec wait1, wait2;
    internal_data_t *ud = (internal_data_t *)args;

    gettimeofday(&now, NULL);
    
    wait1.tv_sec = now.tv_sec + CHANNEL_SCAN_TIME / 1000;
    wait1.tv_nsec = now.tv_usec * 1000 + (CHANNEL_SCAN_TIME % 1000) * 1000000;

    wait1.tv_sec += wait1.tv_nsec / 1000000000;
    wait1.tv_nsec %= 1000000000;

    wait2.tv_sec = wait1.tv_sec + CHANNEL_READ_TIME / 1000;
    wait2.tv_nsec = wait1.tv_nsec;

    // wait first timeout.
    pthread_mutex_lock(&ud->mutex_);
    pthread_cond_timedwait(&ud->cond_, &ud->mutex_, &wait1);
    pthread_mutex_unlock(&ud->mutex_);
    if (ud->flags != 1)
    {
        pcap_breakloop(ud->handle);
        return NULL;
    }

    // second wait.
    pthread_mutex_lock(&ud->mutex_);
    pthread_cond_timedwait(&ud->cond_, &ud->mutex_, &wait2);
    pthread_mutex_unlock(&ud->mutex_);
    
    pcap_breakloop(ud->handle);

    return NULL;
}


int ulink_recv(const char *dev, int timeout, unsigned char **data, size_t *size)
{
    int ret = 0;
    ulink_recv_t *t;
    int channel = 1;
    unsigned long long start, now;
    internal_data_t *udata = (internal_data_t *)malloc(sizeof(internal_data_t));

    t = ulink_recv_open(dev);
    if (t)
    {
        LOG_("ulink_recv_open: %p", t);
        
        start = get_timestamp();
        switch_channel(dev, channel);
        
        while(1)
        {
            pthread_t break_thread;
            memset(udata, 0, sizeof(internal_data_t));

            /* init data */
            udata->handle = t->pcap_handle;
            pthread_mutex_init(&udata->mutex_, NULL); 
            pthread_cond_init(&udata->cond_, NULL);
            

            (void)pthread_create(&break_thread, NULL, pcap_break_proc_, udata);
            pcap_loop(t->pcap_handle, -1, ulink_recv_callback, (unsigned char *)udata);
            pthread_join(break_thread, NULL);
            
            // some cleanup
            pthread_mutex_destroy(&udata->mutex_);
            pthread_cond_destroy(&udata->cond_);

            /* check flags */
            if (udata->flags == 2)
            {
                *data = (unsigned char *)(char *)malloc((size_t)udata->data_length);
                *size = udata->data_length;
                memcpy(*data, udata->data, udata->data_length);
                ret = 1;
                break;
            }
            
            channel %= 14;
            channel++;

            switch_channel(dev, channel);

            now = get_timestamp();
            /* timeout */
            if (now - start > timeout * 1000)
            {
                break;
            }
        }

        ulink_recv_close(t);
    }

    free(udata);

    return ret;
}



