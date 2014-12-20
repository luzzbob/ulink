//
//  UlinkSender.h
//  ulink
//
//  Created by matrix on 14/12/20.
//  Copyright (c) 2014 matrix. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol UlinkSenderDelegate<NSObject>

- (void) sendStart:(id)sender;

- (void) sendStop:(id)sender;

@end

@interface UlinkSender: NSObject

@property(readonly) BOOL isRunning;

@property(nonatomic) id<UlinkSenderDelegate> delegate;

- (UlinkSender *) init;

- (void)dealloc;

- (void) send:(NSData *)data atIndex:(NSInteger) index ofLength:(NSInteger)length;

- (void) send:(NSData *)data;

- (void) stop;


@end


