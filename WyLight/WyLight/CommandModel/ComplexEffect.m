//
//  ComplexEffect.m
//  WyLight
//
//  Created by Nils Weiß on 14/11/13.
//  Copyright (c) 2013 Nils Weiß. All rights reserved.
//

#import "ComplexEffect.h"
#import "Script.h"
#import "SimpelEffect.h"
#import "WCWiflyControlWrapper.h"
#import "wifly_cmd.h"
#import "Fade.h"
#import "Gradient.h"


@implementation ComplexEffect

@dynamic waitCommand;
@dynamic effects;
@dynamic script;

- (id)copy {
    ComplexEffect *newComplex = [ComplexEffect insertNewObjectIntoContext:self.managedObjectContext];
    newComplex.duration = self.duration;
    newComplex.waitCommand = self.waitCommand;
    newComplex.backgroundColor = self.backgroundColor;

    for (SimpelEffect *effect in self.effects) {
        SimpelEffect *newEffect = [effect copy];
        newEffect.complexEffect = newComplex;
    }
    return newComplex;
}

- (void)setWaitCommand:(NSNumber *)waitCommand {
    [self willChangeValueForKey:@"waitCommand"];
    [self setPrimitiveValue:waitCommand forKey:@"waitCommand"];
    [self didChangeValueForKey:@"waitCommand"];
    
    [self setColors:nil];
    
    if (self.script) {
        self.script.snapshot = nil;
    }
}

- (void)setScript:(Script *)script {
    [self willChangeValueForKey:@"script"];
    [self setPrimitiveValue:script forKey:@"script"];
    [self didChangeValueForKey:@"script"];
    
    script.snapshot = nil;
}

- (void)setDuration:(NSNumber *)duration {
    [self willChangeValueForKey:@"duration"];
    [self setPrimitiveValue:duration forKey:@"duration"];
    [self didChangeValueForKey:@"duration"];
    
    if (self.script) {
        self.script.snapshot = nil;
    }
}

+ (NSString *)entityName {
    return @"ComplexEffect";
}

+ (instancetype)insertNewObjectIntoContext:(NSManagedObjectContext *)context {
    return [NSEntityDescription insertNewObjectForEntityForName:[self entityName]
                                         inManagedObjectContext:context];
}

- (void)sendToWCWiflyControl:(WCWiflyControlWrapper *)control {
    if (self.waitCommand.boolValue) {
        [control setWaitTimeInTenMilliSecondsIntervals:self.duration.unsignedIntegerValue];
    } else {
        [self prepareForSendToWCWiflyControl];
        for (id command in self.effects) {
            if ([command respondsToSelector:@selector(sendToWCWiflyControl:)]) {
                [command sendToWCWiflyControl:control];
            }
        }
    }
}

- (void)prepareForSendToWCWiflyControl {
    if (self.hasChanges) {
        NSError *error;
        if (self.managedObjectContext && ![self.managedObjectContext save:&error]) {
            NSLog(@"Save failed: %@", error.helpAnchor);
        }
    }
	if(self.managedObjectContext) {
    	[self.managedObjectContext refreshObject:self mergeChanges:YES];
	}
    for (SimpelEffect *command in self.effects) {
        if ([command isKindOfClass:[SimpelEffect class]]) {
            [command setParallel:@(YES)];
            [command setDuration:self.duration];
        }
    }
    [[self.effects lastObject] setParallel:@(NO)];
}

- (void)setBackgroundColor:(UIColor *)backgroundColor {
    
    [self willChangeValueForKey:@"backgroundColor"];
    [self setPrimitiveValue:backgroundColor forKey:@"backgroundColor"];
    [self didChangeValueForKey:@"backgroundColor"];
    
    [self setColors:nil];
    
    for (SimpelEffect *command in self.effects) {
        command.backgroundColor = backgroundColor;
    }
}

- (ComplexEffect *)prev {
    NSUInteger currentIdx = [self.script.effects indexOfObject:self];
    if (currentIdx == 0) {
        return nil;
    }
    ComplexEffect *prev = [self.script.effects objectAtIndex:currentIdx - 1];
    return prev != self ? prev : nil;
}

- (ComplexEffect *)next {
    NSUInteger currentIdx = [self.script.effects indexOfObject:self];
    currentIdx++;
    if (currentIdx >= self.script.effects.count) {
        return nil;
    }
    ComplexEffect *next = [self.script.effects objectAtIndex:currentIdx];
    return next != self ? next : nil;
}

- (void)setColors:(NSArray *)colors {
    [self willAccessValueForKey:@"colors"];
    NSArray *oldColors = [self primitiveValueForKey:@"colors"];
    [self didAccessValueForKey:@"colors"];
    
    if ([oldColors isEqualToArray:colors]) {
        return;
    }
    
    [self willChangeValueForKey:@"colors"];
    [self setPrimitiveValue:colors forKey:@"colors"];
    [self didChangeValueForKey:@"colors"];
    
    [self setSnapshot:nil];
    if (self.next) {
        self.next.snapshot = nil;
    }
    if (self.script) {
        self.script.snapshot = nil;
    }
}

- (void)setSnapshot:(UIImage *)snapshot {
    [self willAccessValueForKey:@"snapshot"];
    UIImage* snapshotBeforSet = [self primitiveValueForKey:@"snapshot"];
    [self didAccessValueForKey:@"snapshot"];
    
    [self willChangeValueForKey:@"snapshot"];
    [self setPrimitiveValue:snapshot forKey:@"snapshot"];
    [self didChangeValueForKey:@"snapshot"];
    
    if (snapshotBeforSet) {
        [self setOutdatedSnapshot:snapshotBeforSet];
    }
}

- (NSArray *)computeColors {
	NSLog(@"Complex Effect calc Colors");
	if (self.waitCommand.boolValue) {
		if (self.prev.colors) {
			return self.prev.colors;
		} else {
			NSMutableArray *outPutColors = [[NSMutableArray alloc] init];
			for (NSUInteger i = 0; i < NUM_OF_LED; i++) {
				[outPutColors addObject:self.backgroundColor];
			}
            return outPutColors;
		}
	}
	else {
		NSMutableArray *outPutColors;
		if (self.prev) {
			outPutColors = [[NSMutableArray alloc]initWithArray:self.prev.colors copyItems:YES];
		} else if (self.script && self.script.repeatsWhenFinished.boolValue && self.script.effects.firstObject == self && self.script.effects.lastObject != self) {

			ComplexEffect* lastEffect = self.script.effects.lastObject;
			[lastEffect willAccessValueForKey:@"colors"];
			NSArray *colors = [lastEffect primitiveValueForKey:@"colors"];
			[lastEffect didAccessValueForKey:@"colors"];

			if (colors == nil) {
				outPutColors = [[NSMutableArray alloc]init];
			} else {
				outPutColors = [[NSMutableArray alloc]initWithArray:colors copyItems:YES];
			}
		} else {
			outPutColors = [[NSMutableArray alloc]init];
		}

		uint32_t compareMask = 0x00000001;
		for (unsigned int i = 0; i < NUM_OF_LED; i++) {  //i = 0 - 31
			NSUInteger j = self.effects.count;
			while (j--) {
				SimpelEffect *currentObj = [self.effects objectAtIndex:j];
				const uint32_t bitmask = (uint32_t)currentObj.address.unsignedIntegerValue;
				if (bitmask & compareMask) {
					if (outPutColors.count == NUM_OF_LED) {
						[outPutColors replaceObjectAtIndex:i withObject:currentObj.colors[i]];
					} else {
						[outPutColors addObject:currentObj.colors[i]];
					}
					break;
				}
			}
			if (i >= outPutColors.count) {
				[outPutColors addObject:self.backgroundColor];
			}
			compareMask = compareMask << 1;
		}
		return outPutColors;
	}
}

- (NSArray *)colors {
    [self willAccessValueForKey:@"colors"];
    NSArray *colors = [self primitiveValueForKey:@"colors"];
    [self didAccessValueForKey:@"colors"];

    if (colors == nil) {
		colors = [self computeColors];


        [self willChangeValueForKey:@"colors"];
        [self setPrimitiveValue:colors forKey:@"colors"];
        [self didChangeValueForKey:@"colors"];

        if ([self next]) {
            self.next.snapshot = nil;
        }
        if (self.script) {
            self.script.snapshot = nil;
        }
    }
    return colors;
}

@end
