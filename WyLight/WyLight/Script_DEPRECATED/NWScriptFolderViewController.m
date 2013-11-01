//
//  NWScriptFolderViewController.m
//  WyLight
//
//  Created by Nils Weiß on 10/18/13.
//  Copyright (c) 2013 Nils Weiß. All rights reserved.
//

#import "NWScriptFolderViewController.h"
#import "NWRenderableScript.h"
#import "NWScriptCommandObject.h"
#import "iCarousel.h"
#import "NWScriptObjectControl.h"
#import "NWComplexScriptCommandObject.h"
#import "UIView+blurredSnapshot.h"
#import "UIView+Quivering.h"
#import "WCWiflyControlWrapper.h"
#import "NWScriptViewController.h"
#import "NWScript+defaultScripts.h"
#import "NWRenderableScriptImageView.h"

@interface NWScriptFolderViewController () <iCarouselDataSource, iCarouselDelegate, NWRenderableScriptDelegate>

@property (nonatomic, assign) BOOL isDeletionModeActive;
@property (nonatomic, strong) UIBarButtonItem *addButton;
@property (nonatomic, strong) NSMutableArray *scriptObjects;
@property (nonatomic, strong) iCarousel *carousel;
@property (nonatomic, strong) UIImageView *background;
@property (nonatomic, strong) UIButton *sendButton;
@property (nonatomic, strong) UILabel *scriptTitleLabel;

@end

@implementation NWScriptFolderViewController

- (void)viewWillLayoutSubviews {
    [super viewWillLayoutSubviews];
    [self fixLocations];
}

- (void)viewDidLoad {
    [super viewDidLoad];
    [self setup];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    dispatch_async(dispatch_get_main_queue(), ^{
        self.tabBarController.navigationItem.rightBarButtonItem = self.addButton;
    });
    [self.carousel reloadData];
}

- (void)viewDidAppear:(BOOL)animated {
    [self updateView];
    [super viewDidAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated {
    [self saveUserData];
    dispatch_async(dispatch_get_main_queue(), ^{
        self.tabBarController.navigationItem.rightBarButtonItem = nil;
    });
    [super viewWillDisappear:animated];
}

- (void)updateView {
    if ([self.carousel.currentItemView isKindOfClass:[UIImageView class]]) {
        [UIView animateWithDuration:0.4 animations:^{
            self.background.image = ((UIImageView *)[self.carousel currentItemView]).image;
        }];
    }
    self.scriptTitleLabel.text = ((NWScript *)[self.scriptObjects objectAtIndex:self.carousel.currentItemIndex]).title;
}

- (void)fixLocations {
    if (self.view.bounds.size.height > self.view.bounds.size.width) {   //horizontal
        [self.tabBarController.tabBar setHidden:NO];
        self.sendButton.frame = CGRectMake(0, self.view.frame.size.height - 100, self.view.frame.size.width, 44);
        self.scriptTitleLabel.frame = CGRectMake(20, 60, self.view.bounds.size.width - 40, 40);
        self.carousel.frame = CGRectMake(0, 80, self.view.bounds.size.width, self.view.bounds.size.height - 160);
    }
	else {
        [self.tabBarController.tabBar setHidden:YES];
        self.sendButton.frame = CGRectMake(0, self.view.frame.size.height - 44, self.view.frame.size.width, 44);
        self.scriptTitleLabel.frame = CGRectMake(20, 45, self.view.bounds.size.width - 40, 40);
        self.carousel.frame = CGRectMake(0, 100, self.view.bounds.size.width, self.view.bounds.size.height - 160);
    }
    self.background.frame = self.view.bounds;
}

#define SCRIPTARRAY_KEY @"WyLightRemote.NWFolderScriptViewController.ScriptArray"

- (void)setup {
    self.view.superview.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.view.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    
    NSData *data = [[NSUserDefaults standardUserDefaults] objectForKey:SCRIPTARRAY_KEY];
     if (data) {
         self.scriptObjects = [NSKeyedUnarchiver unarchiveObjectWithData:data];
     } else {
         self.scriptObjects = [[NSMutableArray alloc] init];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptFastColorChange]];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptSlowColorChange]];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptConzentrationLight]];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptMovingColors]];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptRandomColors]];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptRunLightWithColor:[UIColor greenColor] timeInterval:100]];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptRunLightWithColor:[UIColor redColor] timeInterval:100]];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptRunLightWithColor:[UIColor blueColor] timeInterval:100]];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptColorCrashWithTimeInterval:50]];
         [self.scriptObjects addObject:[NWRenderableScript defaultScriptColorCrashWithTimeInterval:200]];
    }
    
    for (NWRenderableScript *script in self.scriptObjects) {
        script.delegate = self;
    }
    
    self.background = [[UIImageView alloc] initWithFrame:self.view.frame];
    self.background.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    self.background.contentMode = UIViewContentModeScaleToFill;
    self.background.userInteractionEnabled = NO;
    self.background.opaque = YES;
    [self.view addSubview:self.background];
    
   
    UIView* milkglassView = [[UIView alloc] initWithFrame:self.view.frame];
    milkglassView.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    milkglassView.opaque = NO;
    milkglassView.backgroundColor = [UIColor colorWithRed:1 green:1 blue:1 alpha:0.5];
    [self.view addSubview:milkglassView];
    
    self.sendButton = [UIButton buttonWithType:UIButtonTypeRoundedRect];
	[self.sendButton setTitle:@"SEND" forState:UIControlStateNormal];
	[self.sendButton addTarget:self action:@selector(sendScript) forControlEvents:UIControlEventTouchUpInside];
	[self.view addSubview:self.sendButton];
    
    self.carousel = [[iCarousel alloc] initWithFrame:CGRectZero];
    self.carousel.dataSource = self;
    self.carousel.delegate = self;
    self.carousel.autoresizingMask = UIViewAutoresizingFlexibleHeight | UIViewAutoresizingFlexibleWidth;
    self.carousel.type = iCarouselTypeCoverFlow;
    self.carousel.bounces = NO;
    self.carousel.pagingEnabled = YES;
    self.carousel.backgroundColor = [UIColor clearColor];
    self.carousel.opaque = NO;
    [self.view addSubview:self.carousel];
    
    self.scriptTitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.scriptTitleLabel.textAlignment = NSTextAlignmentCenter;
    [self.view addSubview:self.scriptTitleLabel];
    
    self.addButton = [[UIBarButtonItem alloc]initWithBarButtonSystemItem:UIBarButtonSystemItemAdd target:self action:@selector(addBarButtonPressed:)];
}

- (void)saveUserData {
	NSData *data = [NSKeyedArchiver archivedDataWithRootObject:self.scriptObjects];
	if (data) {
		[[NSUserDefaults standardUserDefaults] setObject:data forKey:SCRIPTARRAY_KEY];
	}
}

#pragma mark - GETTER & SETTER
- (NSMutableArray *)scriptObjects {
    if (!_scriptObjects) {
        _scriptObjects = [[NSMutableArray alloc] init];
        [_scriptObjects addObject:[[NWScript alloc] init]];
    }
    return _scriptObjects;
}

- (void)setIsDeletionModeActive:(BOOL)isDeletionModeActive {
    if (_isDeletionModeActive != isDeletionModeActive) {
        _isDeletionModeActive = isDeletionModeActive;
        if (_isDeletionModeActive) {
            [self.carousel.currentItemView startQuivering];
            self.carousel.scrollEnabled = NO;
        } else {
            [self.carousel.currentItemView stopQuivering];
            self.carousel.scrollEnabled = YES;
        }
    }
}

- (void)sendScript {
	dispatch_queue_t sendScriptQueue = dispatch_queue_create("sendScriptQueue", NULL);
	dispatch_async(sendScriptQueue, ^{
		[self.controlHandle setColorDirect:[UIColor blackColor]];
        NWScript *currentScript = self.scriptObjects[self.carousel.currentItemIndex];
		[currentScript sendToWCWiflyControl:self.controlHandle];
	});
}

#pragma mark - iCarouselDatasource
- (NSUInteger)numberOfItemsInCarousel:(iCarousel *)carousel {
    return self.scriptObjects.count;
}

- (UIView *)carousel:(iCarousel *)carousel viewForItemAtIndex:(NSUInteger)index reusingView:(UIView *)view {
    if (view == nil || ![view isKindOfClass:[UIImageView class]])
    {
        view = [[NWRenderableScriptImageView alloc] initWithFrame:CGRectMake(0, 0, 200, 200)];
        view.contentMode = UIViewContentModeCenter;
        view.backgroundColor = [UIColor grayColor];
        view.tag = index;
    }
    
    if ([view isKindOfClass:[UIImageView class]]) {
        if ([((NWRenderableScript *)self.scriptObjects[index]) needsRendering]) {
            
            if ([((NWRenderableScript *)self.scriptObjects[index]) snapshot]) {
                ((UIImageView *)view).image = [((NWRenderableScript *)self.scriptObjects[index]) snapshot];
            }
            dispatch_async(
                           dispatch_queue_create([[NSString stringWithFormat:@"renderImageForView #%lu", (unsigned long)index] cStringUsingEncoding:NSASCIIStringEncoding], NULL),
                           ^{
                               double delayInSeconds = 0.2;
                               dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
                               dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
                                   ((NWRenderableScriptImageView *)view).showActivityIndicator = [((NWRenderableScript *)self.scriptObjects[index]) isRendering];
                               });
                               [((NWRenderableScript *)self.scriptObjects[index]) startRenderingWithRect:view.frame];
                            });
        } else {
            if ([((NWRenderableScript *)self.scriptObjects[index]) snapshot]) {
                ((UIImageView *)view).image = [((NWRenderableScript *)self.scriptObjects[index]) snapshot];
            }
        }
        ((NWRenderableScriptImageView *)view).showActivityIndicator = [((NWRenderableScript *)self.scriptObjects[index]) isRendering];
        
        view.layer.cornerRadius = 5.0;
        view.clipsToBounds = YES;
        
        UILongPressGestureRecognizer *longPress = [[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(longPressOnScriptObjectControl:)];
        [view addGestureRecognizer:longPress];
        
        UITapGestureRecognizer *tap = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(tapOnScriptObjectControl:)];
        [view addGestureRecognizer:tap];
        
        UISwipeGestureRecognizer *swipe = [[UISwipeGestureRecognizer alloc] initWithTarget:self action:@selector(swipeUpOnScriptObjectControl:)];
        swipe.direction = UISwipeGestureRecognizerDirectionUp;
        [view addGestureRecognizer:swipe];
        
        return view;
        
    }
    return nil;

}

#pragma mark - iCarouselDelegate

- (CGFloat)carousel:(iCarousel *)carousel valueForOption:(iCarouselOption)option withDefault:(CGFloat)value {
	switch (option)
	{
		case iCarouselOptionWrap:
		{
			//normally you would hard-code this to YES or NO
			return NO;
		}
		case iCarouselOptionSpacing:
		{
			//add a bit of spacing between the item views
			return value * 1.05f;
		}
		default:
		{
			return value;
		}
	}
}

- (void)carouselCurrentItemIndexDidChange:(iCarousel *)carousel {
    [self updateView];
}

- (void)tapOnScriptObjectControl:(UITapGestureRecognizer *)gesture {
    if (self.isDeletionModeActive) {
        self.isDeletionModeActive = NO;
    } else {
        [self showFullScreenAlertView];
        double delayInSeconds = 0.1;
        dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
        dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
            [self performSegueWithIdentifier:@"edit:" sender:self];
        });
    }
}

- (void)longPressOnScriptObjectControl:(UILongPressGestureRecognizer *)gesture {
    self.isDeletionModeActive = YES;
}

- (void)swipeUpOnScriptObjectControl:(UISwipeGestureRecognizer *)gesture {
    if (self.isDeletionModeActive && self.scriptObjects.count > 1) {
        [self.scriptObjects removeObjectAtIndex:self.carousel.currentItemIndex];
        [self.carousel removeItemAtIndex:self.carousel.currentItemIndex animated:YES];
    }
    self.isDeletionModeActive = NO;
}

- (IBAction)addBarButtonPressed:(UIBarButtonItem *)sender {
    NWRenderableScript *script = [NWRenderableScript emptyScript];
    script.delegate = self;
    [self.scriptObjects addObject:script];
    [self.carousel insertItemAtIndex:self.scriptObjects.count - 1 animated:YES];
    [self.carousel scrollToItemAtIndex:self.scriptObjects.count - 1 animated:YES];
}

- (void)showFullScreenAlertView {
    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@"Loading data..." message:@"Please Wait!" delegate:nil cancelButtonTitle:nil otherButtonTitles:nil, nil];
    [alert show];
    
    double delayInSeconds = 2.0;
    dispatch_time_t popTime = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(delayInSeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^(void){
        [alert dismissWithClickedButtonIndex:0 animated:YES];
    });
}

#pragma mark - RenderableScriptDelegate

- (void)NWRenderableScriptFinishedRendering:(NWRenderableScript *)script {
    NSUInteger index = [self.scriptObjects indexOfObject:script];
    [self.carousel reloadItemAtIndex:index animated:YES];
    [self updateView];
}

#pragma mark - SEGUE
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
	if ([segue.identifier isEqualToString:@"edit:"])
	{
		if ([segue.destinationViewController respondsToSelector:@selector(setControlHandle:)])
		{
			[segue.destinationViewController performSelector:@selector(setControlHandle:) withObject:self.controlHandle];
		}
		
		if ([segue.destinationViewController isKindOfClass:[NWScriptViewController class]]) {
			NWScriptViewController *ctrl = (NWScriptViewController *)segue.destinationViewController;
			ctrl.script = [self.scriptObjects objectAtIndex:self.carousel.currentItemIndex];
		}
	}
    NSLog(@"prepare");
}



@end
