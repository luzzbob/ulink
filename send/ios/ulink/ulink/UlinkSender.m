//
//  UlinkSender.m
//  ulink
//
//  Created by matrix on 14/12/20.
//  Copyright (c) 2014 matrix. All rights reserved.
//

#import <CFNetwork/CFNetwork.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import "UlinkSender.h"

@implementation UlinkSender
{
    NSData *d_;
    Byte *data_;
    NSInteger dataLen_;
    Byte dataCrc_;
    BOOL canceling_;
}

@synthesize isRunning;
@synthesize delegate;


- (void)start_
{
    if ([delegate respondsToSelector:@selector(sendStart:)])
    {
        [delegate performSelector:@selector(sendStart:) withObject:self];
    }
}

- (void)stop_
{
    if ([delegate respondsToSelector:@selector(sendStop:)])
    {
        [delegate performSelector:@selector(sendStop:) withObject:self];
    }
}

- (void) run_:(id)obj
{
    if (dataLen_ > 0)
    {
        if (delegate)
        {
            [self performSelectorOnMainThread:@selector(start_) withObject:Nil waitUntilDone:YES];
        }
        
        CFSocketRef sock = CFSocketCreate(kCFAllocatorDefault, PF_INET, SOCK_DGRAM, IPPROTO_UDP, 0, Nil, Nil);
        struct sockaddr_in addr;
        char saddr[16];
    
        if (sock != Nil)
        {
            memset(&addr, 0, sizeof(addr));
            addr.sin_len = sizeof(addr);
            addr.sin_family = AF_INET;
            addr.sin_port = 1080;
        
            isRunning = YES;
            while (!canceling_)
            {
                int s = CFSocketGetNative(sock);
                
                // send first packet
                snprintf(saddr, 16, "239.0.%d.%d", dataLen_, dataCrc_);
                addr.sin_addr.s_addr = inet_addr(saddr);
                sendto(s, "x", 1, 0, (const struct sockaddr *)&addr, sizeof(addr));
                
                usleep(3000);
                
                for (int i=0; i<dataLen_; i+=2) {
                    if (i + 1 < dataLen_)
                    {
                        snprintf(saddr, 16, "239.%d.%d.%d", (i / 2) + 1, data_[i], data_[i+1]);
                    }
                    else
                    {
                        snprintf(saddr, 16, "239.%d.%d.0", (1 / 2) + 1, data_[i]);
                    }
                    
                    addr.sin_addr.s_addr = inet_addr(saddr);
                    sendto(s, "x", 1, 0, (const struct sockaddr *)&addr, sizeof(addr));
                }
            }
            
            CFRelease(sock);
        }
    }
    
    if (delegate)
    {
        [self performSelectorOnMainThread:@selector(stop_) withObject:Nil waitUntilDone:YES];
    }
    
    isRunning = NO;
}

- (UlinkSender *) init
{
    if ([super init])
    {
        d_ = Nil;
        data_ = Nil;
        dataLen_ = 0;
        dataCrc_ = 0;
        canceling_ = NO;
        isRunning = NO;
    }
    return self;
}

- (void)dealloc
{
    [self stop];
    while (isRunning)
    {
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow: 10]];
    }
    
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow: 10]];
}


- (void) send:(NSData *)data atIndex:(NSInteger) index ofLength:(NSInteger)length
{
    if (! isRunning)
    {
        d_ = [data subdataWithRange:NSMakeRange(index, length)];
        data_ = (Byte *)[d_ bytes];
        dataLen_ = length;
        
        for (int i=0; i<length; i++)
        {
            dataCrc_ ^= data_[i];
        }
        
        isRunning = YES;
        canceling_ = NO;
        [self performSelectorInBackground:@selector(run_:) withObject:nil];
    }
}

- (void) send:(NSData *)data
{
    return [self send:data atIndex:0 ofLength:[data length]];
}


- (void) stop
{
    canceling_ = YES;
}

@end
