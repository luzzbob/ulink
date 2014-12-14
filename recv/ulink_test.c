#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "ulink.h"

int main(int argc, char *argv[])
{
    int ret;
    unsigned char *data = NULL;
    size_t len;
    char *dev = "wlan0";

    if (argc > 1)
    {
        dev = argv[1];
    }

    ret = ulink_recv(dev, 60, &data, &len);
    if (len > 0)
    {
        printf("%s\n", data);
        free(data);
    }

    return 0;
}
