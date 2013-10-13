//
//  NWColorEditView.h
//  WyLight
//
//  Created by Nils Weiß on 13.10.13.
//  Copyright (c) 2013 Nils Weiß. All rights reserved.
//

#import <UIKit/UIKit.h>

@class NWColorEditView, NWScriptCommandObject;

@protocol NWColorEditViewDelegate <NSObject>

- (void)NWColorEditView:(NWColorEditView *)view changedColor:(UIColor *)color;

@end

@interface NWColorEditView : UIView

@property (nonatomic, weak) id<NWColorEditViewDelegate> delegate;
@property (nonatomic) CGFloat cornerRadius;
@property (nonatomic, strong) NSString *title;

@end