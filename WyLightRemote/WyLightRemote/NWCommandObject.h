//
//  NWCommandObject.h
//  WyLightRemote
//
//  Created by Nils Weiß on 13.08.13.
//  Copyright (c) 2013 Nils Weiß. All rights reserved.
//

#import <Foundation/Foundation.h>

@class WCWiflyControlWrapper;

@interface NWCommandObject : NSObject

- (void)sendToWCWiflyControl:(WCWiflyControlWrapper *)control;

@end



