//
//  WCWiflyControlWrapper.m
//
//  Created by Bastian Kres on 16.04.13.
//  Copyright (c) 2013 Bastian Kres. All rights reserved.
//

#import "WCWiflyControlWrapper.h"
#include "WiflyControlNoThrow.h"
#include <time.h>

@interface WCWiflyControlWrapper ()

@property (nonatomic) WiflyControlNoThrow *mControl;

@end

@implementation WCWiflyControlWrapper

#pragma mark - Object

- (id)init
{
    @throw ([NSException exceptionWithName:@"Wrong init-method" reason:@"Use -initWithIP:withPort:" userInfo:nil]);
}

- (id)initWithIP:(uint32_t)ip port:(uint16_t)port
{
    self = [super init];
    if (self)
    {
        self.mControl = new WiflyControlNoThrow(ip,port);
    }
    return self;
}

-(void)dealloc
{
    delete self.mControl;
    
    self.mControl = NULL;
}

#pragma mark - Configuration WLAN-Module

- (uint32_t)setWlanSSID:(NSString *)ssid password:(NSString *)password
{
    const std::string ssidCString([ssid cStringUsingEncoding:NSASCIIStringEncoding]);
    const std::string passwordCString([password cStringUsingEncoding:NSASCIIStringEncoding]);
    
    return (*self.mControl).ConfSetWlan(passwordCString, ssidCString);
}

- (uint32_t)setDefaultConfiguration
{
    return (*self.mControl).ConfSetDefaults();
}

- (uint32_t)setWlanDeviceName:(NSString *)name
{
	const std::string nameCString([name cStringUsingEncoding:NSASCIIStringEncoding]);
	
	return (*self.mControl).ConfSetDeviceId(nameCString);
}

- (uint32_t)rebootWlanModul
{
	return (*self.mControl).ConfRebootWlanModule();
}

#pragma mark - Firmware methods

- (uint32_t)setColorDirect:(UIColor *)newColor
{
    float redPart;
    float greenPart;
    float bluePart;
    [newColor getRed:&redPart green:&greenPart blue:&bluePart alpha:nil];
    
    int sizeColorArray = 32 * 3;
    
    uint8_t colorArray[sizeColorArray];
    uint8_t *pointer = colorArray;
    
    for (int i = 0; i < sizeColorArray; i++)
    {
        switch (i%3)
        {
            case 0:
                *pointer++ = (uint8_t)(bluePart * 255);
                break;
            case 1:
                *pointer++ = (uint8_t)(greenPart * 255);
                break;
            case 2:
                *pointer++ = (uint8_t)(redPart * 255);
                break;
        }
    }
    
    return [self setColorDirect:colorArray bufferLength:sizeColorArray];
}

- (uint32_t)setColorDirect:(const uint8_t*)pointerBuffer bufferLength:(size_t)length
{
    return (*self.mControl).FwSetColorDirect(pointerBuffer, length);
}

- (uint32_t)setWaitTimeInTenMilliSecondsIntervals:(uint16_t)time
{
    return (*self.mControl).FwSetWait(time);
}

- (uint32_t)setFade:(uint32_t)colorInARGB
{
    return (*self.mControl).FwSetFade(colorInARGB);
}

- (uint32_t)setFade:(uint32_t)colorInARGB time:(uint16_t)timeValue
{
    return (*self.mControl).FwSetFade(colorInARGB, timeValue);
}

- (uint32_t)setFade:(uint32_t)colorInARGB time:(uint16_t)timeValue address:(uint32_t)address
{
    return (*self.mControl).FwSetFade(colorInARGB, timeValue, address);
}

- (uint32_t)setFade:(uint32_t)colorInARGB time:(uint16_t)timeValue address:(uint32_t)address parallelFade:(BOOL)parallel
{
    return (*self.mControl).FwSetFade(colorInARGB, timeValue, address, parallel);
}

- (uint32_t)setGradientWithColor:(uint32_t)colorOneInARGB colorTwo:(uint32_t)colorTwoInARGB
{
    return (*self.mControl).FwSetGradient(colorOneInARGB, colorTwoInARGB);
}

- (uint32_t)setGradientWithColor:(uint32_t)colorOneInARGB colorTwo:(uint32_t)colorTwoInARGB time:(uint16_t)timeValue
{
    return (*self.mControl).FwSetGradient(colorOneInARGB, colorTwoInARGB, timeValue);
}

- (uint32_t)setGradientWithColor:(uint32_t)colorOneInARGB colorTwo:(uint32_t)colorTwoInARGB time:(uint16_t)timeValue parallelFade:(BOOL)parallel
{
    return (*self.mControl).FwSetGradient(colorOneInARGB, colorTwoInARGB, timeValue, parallel);
}

- (uint32_t)setGradientWithColor:(uint32_t)colorOneInARGB colorTwo:(uint32_t)colorTwoInARGB time:(uint16_t)timeValue parallelFade:(BOOL)parallel gradientLength:(uint8_t)length
{
    return (*self.mControl).FwSetGradient(colorOneInARGB, colorTwoInARGB, timeValue, parallel, length);
}

- (uint32_t)setGradientWithColor:(uint32_t)colorOneInARGB colorTwo:(uint32_t)colorTwoInARGB time:(uint16_t)timeValue parallelFade:(BOOL)parallel gradientLength:(uint8_t)length startPosition:(uint8_t)offset
{
    return (*self.mControl).FwSetGradient(colorOneInARGB, colorTwoInARGB, timeValue, parallel, length, offset);
}

- (uint32_t)loopOn
{
    return (*self.mControl).FwLoopOn();
}

- (uint32_t)loopOffAfterNumberOfRepeats:(uint8_t)repeats
{
    return (*self.mControl).FwLoopOff(repeats); // 0: Endlosschleife / 255: Maximale Anzahl
}

- (uint32_t)clearScript
{
    return (*self.mControl).FwClearScript();
}

- (uint32_t)readRtcTime:(NSDate **)date
{
    struct tm timeInfo;
    uint32_t returnValue = (*self.mControl).FwGetRtc(timeInfo);
    *date = [NSDate dateWithTimeIntervalSince1970:mktime(&timeInfo)];
    
    return returnValue;
}

- (uint32_t)writeRtcTime
{
    //NSDate *date = [NSDate date];
    //NSTimeZone = [NSTimeZone locald]
    
    NSTimeInterval timeInterval = [[NSDate date] timeIntervalSince1970];
    struct tm* timeInfo;
    time_t rawTime = (time_t)timeInterval;
    
    //time(&rawTime);
    timeInfo = localtime(&rawTime);
    
    return (*self.mControl).FwSetRtc(*timeInfo);
}

- (uint32_t)readCurrentFirmwareVersionFromFirmware:(NSString **)currentFirmwareVersionStringPlaceholder
{
    std::string firmwareVersionString;
    uint32_t returnValue = (*self.mControl).FwGetVersion(firmwareVersionString);
    *currentFirmwareVersionStringPlaceholder = [NSString stringWithCString:firmwareVersionString.c_str() encoding:NSASCIIStringEncoding];
    
    return returnValue;
}

- (uint32_t)enterBootloader
{
    return (*self.mControl).FwStartBl();
}

#pragma mark - Bootloader methods

- (uint32_t)readCurrentFirmwareVersionFromBootloder:(NSString **)currentFirmwareVersionStringPlaceholder
{
    std::string firmwareVersionString;
    uint32_t returnValue = (*self.mControl).BlReadFwVersion(firmwareVersionString);
    *currentFirmwareVersionStringPlaceholder = [NSString stringWithCString:firmwareVersionString.c_str() encoding:NSASCIIStringEncoding];
    
    return returnValue;
}

- (uint32_t)programFlash
{
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectoryPath = [paths objectAtIndex:0];
    NSString *filePath = [documentsDirectoryPath stringByAppendingPathComponent:@"main.hex"];
    
    return (*self.mControl).BlProgramFlash(std::string([filePath cStringUsingEncoding:NSASCIIStringEncoding]));
}

- (uint32_t)leaveBootloader
{
    return (*self.mControl).BlRunApp();
}

@end
