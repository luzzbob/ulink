#include <stdio.h>
#include <sys/types.h>

#include "ulink.h"

int main(int argc, char *argv[])
{
    int ret;
    unsigned char *data = NULL;
    size_t len;

    ret = ulink_recv("wlan0", 20, &data, &len);

    return 0;
}
