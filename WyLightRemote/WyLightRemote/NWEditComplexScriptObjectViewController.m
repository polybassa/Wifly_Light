//
//  NWEditComplexScriptObjectViewController.m
//  WyLightRemote
//
//  Created by Nils Weiß on 15.08.13.
//  Copyright (c) 2013 Nils Weiß. All rights reserved.
//

#import "NWEditComplexScriptObjectViewController.h"
#import "NWScriptObjectButton.h"
#import "NWSetFadeScriptCommandObject.h"
#import "NWSetGradientScriptCommandObject.h"
#import "NWEditFadeCommandViewController.h"
#import "NWEditGradientCommandViewController.h"
#import "WCWiflyControlWrapper.h"

@interface NWEditComplexScriptObjectViewController ()
@property (nonatomic, strong) NWScriptObjectButton *gradientPreviewView;
@property (nonatomic, strong) UISwitch *modeSwitch;
@property (nonatomic, strong) UIScrollView *scrollView;
@property (nonatomic, strong) NSMutableArray *commandObjectViews;
@property (nonatomic) NSUInteger indexOfObjectToAlter;
@property (nonatomic, strong) UIButton *sendButton;
@end

@implementation NWEditComplexScriptObjectViewController

- (void)setup {
	self.view.backgroundColor = [UIColor viewFlipsideBackgroundColor];
	
	self.modeSwitch = [[UISwitch alloc]initWithFrame:CGRectZero];
	[self.modeSwitch addTarget:self action:@selector(switchChanged) forControlEvents:UIControlEventValueChanged];
	[self.view addSubview:self.modeSwitch];
	
	self.gradientPreviewView = [[NWScriptObjectButton alloc]initWithFrame:CGRectZero];
	self.gradientPreviewView.backgroundColor = [UIColor blackColor];
	[self.view addSubview:self.gradientPreviewView];
	
	self.scrollView = [[UIScrollView alloc]initWithFrame:CGRectZero];
	self.scrollView.backgroundColor = [UIColor lightGrayColor];
	[self.view addSubview:self.scrollView];
	
	[self setupCommandObjectViews];
	for (NWScriptObjectButton *view in self.commandObjectViews) {
		[self.scrollView addSubview:view];
	}
	
	self.sendButton = [UIButton buttonWithType:UIButtonTypeRoundedRect];
	[self.sendButton addTarget:self action:@selector(sendPreview:) forControlEvents:UIControlEventTouchDown];
	[self.sendButton setTitle:@"Preview" forState:UIControlStateNormal];
	[self.view addSubview:self.sendButton];
}

- (IBAction)sendPreview:(id)sender
{
	if (self.controlHandle) {
		[self.controlHandle clearScript];
		[self.controlHandle setColorDirect:[UIColor blackColor]];
		[self.command sendToWCWiflyControl:self.controlHandle];
	}
}

- (void)setupCommandObjectViews {
	NSMutableArray *commandObjectViews = [[NSMutableArray alloc]init];
	
	NSUInteger i = 0;
	
	for (NWScriptCommandObject *command in self.command.itsScriptObjects) {
		NWScriptObjectButton *view = [[NWScriptObjectButton alloc] initWithFrame:CGRectMake(20 + i * 100, self.scrollView.frame.origin.y + 10, 80, self.command.colors.count * 3)];
		view.endColors = command.colors;
		view.backgroundColor = [UIColor blackColor];
		
		UITapGestureRecognizer *gesture = [[UITapGestureRecognizer alloc]initWithTarget:self action:@selector(doubleTapAtCommandObject:)];
		gesture.numberOfTapsRequired = 2;
		[view addGestureRecognizer:gesture];
		
		[commandObjectViews addObject:view];
		i++;
	}
	self.commandObjectViews = commandObjectViews;
}

- (NSMutableArray *)commandObjectViews {
	if (_commandObjectViews == nil) {
		_commandObjectViews = [[NSMutableArray alloc]init];
	}
	return _commandObjectViews;
}



- (IBAction)doubleTapAtCommandObject:(UITapGestureRecognizer *)recognizer {
	self.indexOfObjectToAlter = [self.commandObjectViews indexOfObject:recognizer.view];
	NWScriptCommandObject *obj = [self.command.itsScriptObjects objectAtIndex:self.indexOfObjectToAlter];
	
	if ([obj isKindOfClass:[NWSetFadeScriptCommandObject class]]) {
		[self performSegueWithIdentifier:@"editFade:" sender:self];
	} else if ([obj isKindOfClass:[NWSetGradientScriptCommandObject class]])
	{
		[self performSegueWithIdentifier:@"editGradient:" sender:self];
	}
}

- (void)switchChanged {
	self.command.waitCommand = !self.modeSwitch.on;
	[self setColorsOfGradientView];
}

- (void)viewWillLayoutSubviews {
	[super viewWillLayoutSubviews];
	[self fixLocations];
}

- (void)viewWillAppear:(BOOL)animated {
	[super viewWillAppear:animated];
	self.modeSwitch.on = !self.command.isWaitCommand;
	[self setColorsOfGradientView];
}

- (void)setColorsOfGradientView {
	if (self.command.prev) {
		self.gradientPreviewView.startColors = self.command.prev.colors;
	} else {
		self.gradientPreviewView.startColors = nil;
	}

	if (self.command.isWaitCommand) {
		self.gradientPreviewView.endColors = self.gradientPreviewView.startColors;
	} else {
		self.gradientPreviewView.endColors = self.command.colors;
	}
}

- (void)fixLocations {
	//horizontal
	CGFloat totalWidth = self.view.bounds.size.width - 40.0;
	CGFloat totalHeight = self.view.bounds.size.height - 40.0;
	CGFloat xOffset = 20;
	
	self.gradientPreviewView.frame = CGRectMake(xOffset, 80, totalWidth, self.command.colors.count * 4);
	
	self.modeSwitch.frame = CGRectMake(self.view.bounds.size.width / 2 - 20, 20, 40, 20);
	
	self.scrollView.contentSize = CGSizeMake(100 * self.command.itsScriptObjects.count + 20, self.command.colors.count * 4);
	self.scrollView.frame = CGRectMake(xOffset, totalHeight / 2 + 40 , totalWidth, self.command.colors.count * 4);
	
	self.sendButton.frame = CGRectMake(20, 370, 100, 40);
}

- (void)viewDidLoad {
    [super viewDidLoad];
	
	//TESTCODE
	self.command = [[NWComplexScriptCommandObject alloc] init];
	self.command.backgroundColor = [UIColor blackColor];
	
	{
		NWSetFadeScriptCommandObject *obj = [[NWSetFadeScriptCommandObject alloc] init];
		obj.address = 0xff000000;
		obj.color = [UIColor redColor];
	
		[self.command.itsScriptObjects addObject:obj];
	}
	{
		NWSetFadeScriptCommandObject *obj = [[NWSetFadeScriptCommandObject alloc] init];
		obj.address = 0x000000ff;
		obj.color = [UIColor yellowColor];
		
		[self.command.itsScriptObjects addObject:obj];
	}
	{
		NWSetGradientScriptCommandObject *obj = [[NWSetGradientScriptCommandObject alloc] init];
		obj.color1 = [UIColor blueColor];
		obj.color2 = [UIColor greenColor];
	
		obj.offset = 10;
		obj.numberOfLeds = 10;
		[self.command.itsScriptObjects addObject: obj];
	}
	{
		NWSetGradientScriptCommandObject *obj = [[NWSetGradientScriptCommandObject alloc] init];
		obj.color1 = [UIColor blueColor];
		obj.color2 = [UIColor greenColor];
		
		obj.offset = 20;
		obj.numberOfLeds = 5;
		[self.command.itsScriptObjects addObject: obj];
	}
	self.command.duration = 2;
	[self setup];
}

- (IBAction)unwindEditScriptObject:(UIStoryboardSegue *)segue {
	if ([segue.sourceViewController respondsToSelector:@selector(command)]) {
		NWScriptCommandObject *cmdObj = (NWScriptCommandObject*)[segue.sourceViewController command];
		[self.command.itsScriptObjects replaceObjectAtIndex:self.indexOfObjectToAlter withObject:cmdObj];
		NWScriptObjectButton *view = [self.commandObjectViews objectAtIndex:self.indexOfObjectToAlter];
		view.endColors = cmdObj.colors;
	}
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
	if ([segue.identifier isEqualToString:@"editFade:"]) {
		if ([segue.destinationViewController isKindOfClass:[NWEditFadeCommandViewController class]]) {
			NWEditFadeCommandViewController *dest = (NWEditFadeCommandViewController*)segue.destinationViewController;
			dest.command = [[self.command.itsScriptObjects objectAtIndex:self.indexOfObjectToAlter] copy];
		}
	} else if ([segue.identifier isEqualToString:@"editGradient:"]) {
		if ([segue.destinationViewController isKindOfClass:[NWEditGradientCommandViewController class]]) {
			NWEditGradientCommandViewController *dest = (NWEditGradientCommandViewController*)segue.destinationViewController;
			dest.command = [[self.command.itsScriptObjects objectAtIndex:self.indexOfObjectToAlter] copy];
		}
	}
}



@end