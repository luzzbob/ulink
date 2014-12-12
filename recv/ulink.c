#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <pcap/pcap.h>

#include <linux/wireless.h>

#include "ulink.h"

#define CHANNEL_SCAN_TIME (200)
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

#define LOG_(fmt...) LOGL_(1,##fmt)

typedef struct ulink_recv_
{
    pcap_t *pcap_handle;
} ulink_recv_t;

static ulink_recv_t *ulink_recv_open(const char *netdev)
{
	int err;
    ulink_recv_t *t = NULL;
    
    t = (ulink_recv_t *)malloc(sizeof(ulink_recv_t));

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

    }
    
	return t;
}


static void ulink_recv_close(ulink_recv_t *t)
{
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
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC,&tp);
    return (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);
}


#define MAC_BUFF_COUNT (64)
typedef struct internal_data_
{
    pcap_t *handle;
    unsigned long long first_time;
    unsigned char smac[MAC_BUFF_COUNT][6];
    unsigned int  smac_count[MAC_BUFF_COUNT];
    unsigned int  smac_offset;
    unsigned char xmac[6];
    int flags;
    unsigned char data[256];
    int  data_length;
    int read_offset;
} internal_data_t;


static void ulink_recv_callback(unsigned char* data, const struct pcap_pkthdr *hdr, const unsigned char *pkt)
{
    internal_data_t *ud = (internal_data_t *) data;
    int radio_head_length = 0;
    const unsigned char *pkt_mac = NULL;
    unsigned long long current_time = hdr->ts.tv_sec * 1000 + hdr->ts.tv_usec / 1000;
    int i = 0;

    if (!ud->first_time)
    {
        ud->first_time = hdr->ts.tv_sec * 1000 + hdr->ts.tv_usec / 1000;
    }
    else
    {
        if (current_time - ud->first_time > 2000 + CHANNEL_SCAN_TIME)
        {
            pcap_breakloop(ud->handle);
        }

        /* after CHANNEL_SCAN_TIME ms */
        if (current_time - ud->first_time > CHANNEL_SCAN_TIME)
        {
            if (ud->flags == 0)
            {
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
                else
                {
                    pcap_breakloop(ud->handle);
                }
            }
        }
    }


    radio_head_length = pkt[2] & 0xff;
    pkt_mac = &pkt[radio_head_length];

    // skip too short and too long.
    if ((hdr->len < radio_head_length + 22) || (hdr->len > radio_head_length + 300))
    {
        return;
    }

    // check dest mac address if a multicast
    if (pkt_mac[4] == 0x01 && 
        pkt_mac[5] == 0x00 && 
        pkt_mac[6] == 0x5e && 
        (pkt_mac[7] & 0x80) == 0x00)
    {

        if (ud->flags == 0)
        {
            for (i=0; i<ud->smac_offset; i++)
            {
                /* source mac compare 16-21 */
                if (pkt_mac[21] == ud->smac[i][5] &&
                    pkt_mac[20] == ud->smac[i][4] &&
                    pkt_mac[19] == ud->smac[i][3] &&
                    pkt_mac[18] == ud->smac[i][2] &&
                    pkt_mac[17] == ud->smac[i][1] &&
                    pkt_mac[16] == ud->smac[i][0])
                {
                    ud->smac_count[i]++;
                    break;
                }
            }

            if (i == ud->smac_offset && ud->smac_offset < MAC_BUFF_COUNT)
            {
                // new mac
                memcpy(ud->smac[i], &pkt_mac[16], 6);
                ud->smac_count[i] = 1;

                ud->smac_offset++;
            }
        }
        else if(ud->flags == 1)
        {
            
        }
    }
}


int ulink_recv(const char *dev, int timeout, unsigned char **data, size_t *size)
{
    int ret = 0;
    ulink_recv_t *t;
    int channel = 1;
    unsigned long long start, now;
    internal_data_t udata;

    t = ulink_recv_open(dev);
    if (t)
    {
        LOG_("ulink_recv_open: %p", t);
        
        start = get_timestamp();
        switch_channel(dev, channel);
        
        while(1)
        {
            now = get_timestamp();
            memset(&udata, 0, sizeof(udata));
            udata.handle = t->pcap_handle;

            pcap_loop(t->pcap_handle, -1, ulink_recv_callback, (unsigned char *)&udata);
            
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

    return ret;
}



