//
//  ViewController.m
//  ulink
//
//  Created by matrix on 14/12/20.
//  Copyright (c) 2014年 matrix. All rights reserved.
//

#import "ViewController.h"
#import "UlinkSender.h"

@interface ViewController ()

@end

@implementation ViewController
{
    UlinkSender *s;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    
    s = [[UlinkSender alloc] init];
    [s setDelegate:self];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
    
    
}

- (IBAction)onTest:(id)sender
{
    if ([s isRunning])
    {
        [s stop];
    }
    else
    {
        [s send: [[[self txtToSend] text] dataUsingEncoding:NSUTF8StringEncoding]];
    }
}

- (void)sendStart:(id)sender
{
    [[self btnTest] setTitle:@"Stop" forState:UIControlStateNormal];
}

- (void)sendStop:(id)sender
{
    [[self btnTest] setTitle:@"Send" forState:UIControlStateNormal];
}

@end
