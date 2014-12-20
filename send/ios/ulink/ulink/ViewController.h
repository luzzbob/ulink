//
//  ViewController.h
//  ulink
//
//  Created by matrix on 14/12/20.
//  Copyright (c) 2014年 matrix. All rights reserved.
//

#import "UlinkSender.h"
#import <UIKit/UIKit.h>

@interface ViewController : UIViewController<UlinkSenderDelegate>

@property (weak, nonatomic) IBOutlet UITextField *txtToSend;
- (IBAction)onTest:(id)sender;
@property (weak, nonatomic) IBOutlet UIButton *btnTest;

@end

