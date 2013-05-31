/*
 RDP Keyboard helper 
 
 Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz
 
 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
 If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#import "RDPKeyboard.h"
#include <freerdp/locale/keyboard.h>

@interface RDPKeyboard (Private)
- (void)sendVirtualKey:(int)vKey up:(BOOL)up;
- (void)handleSpecialKey:(int)character;
- (void)handleAlphaNumChar:(int)character;
- (void)notifyDelegateModifiersChanged;
@end

@implementation RDPKeyboard

@synthesize delegate = _delegate, ctrlPressed = _ctrl_pressed, altPressed = _alt_pressed, shiftPressed = _shift_pressed, winPressed = _win_pressed;

- (id)init
{
	if((self = [super init]) != nil)
	{
        [self initWithSession:nil delegate:nil];
        
		memset(_virtual_key_map, 0, sizeof(_virtual_key_map));
        memset(_unicode_map, 0, sizeof(_unicode_map));
		
		// init vkey map - used for alpha-num characters
		_virtual_key_map['0'] = VK_KEY_0;
		_virtual_key_map['1'] = VK_KEY_1;
		_virtual_key_map['2'] = VK_KEY_2;
		_virtual_key_map['3'] = VK_KEY_3;
		_virtual_key_map['4'] = VK_KEY_4;
		_virtual_key_map['5'] = VK_KEY_5;
		_virtual_key_map['6'] = VK_KEY_6;
		_virtual_key_map['7'] = VK_KEY_7;
		_virtual_key_map['8'] = VK_KEY_8;
		_virtual_key_map['9'] = VK_KEY_9;
		
		_virtual_key_map['a'] = VK_KEY_A;
		_virtual_key_map['b'] = VK_KEY_B;
		_virtual_key_map['c'] = VK_KEY_C;
		_virtual_key_map['d'] = VK_KEY_D;
		_virtual_key_map['e'] = VK_KEY_E;
		_virtual_key_map['f'] = VK_KEY_F;
		_virtual_key_map['g'] = VK_KEY_G;
		_virtual_key_map['h'] = VK_KEY_H;
		_virtual_key_map['i'] = VK_KEY_I;
		_virtual_key_map['j'] = VK_KEY_J;
		_virtual_key_map['k'] = VK_KEY_K;
		_virtual_key_map['l'] = VK_KEY_L;
		_virtual_key_map['m'] = VK_KEY_M;
		_virtual_key_map['n'] = VK_KEY_N;
		_virtual_key_map['o'] = VK_KEY_O;
		_virtual_key_map['p'] = VK_KEY_P;
		_virtual_key_map['q'] = VK_KEY_Q;
		_virtual_key_map['r'] = VK_KEY_R;
		_virtual_key_map['s'] = VK_KEY_S;
		_virtual_key_map['t'] = VK_KEY_T;
		_virtual_key_map['u'] = VK_KEY_U;
		_virtual_key_map['v'] = VK_KEY_V;
		_virtual_key_map['w'] = VK_KEY_W;
		_virtual_key_map['x'] = VK_KEY_X;
		_virtual_key_map['y'] = VK_KEY_Y;
		_virtual_key_map['z'] = VK_KEY_Z;
        
        // init scancode map - used for special characters
        _unicode_map['-'] = 45;
        _unicode_map['/'] = 47;
        _unicode_map[':'] = 58;
        _unicode_map[';'] = 59;
        _unicode_map['('] = 40;
        _unicode_map[')'] = 41;
        _unicode_map['&'] = 38;
        _unicode_map['@'] = 64;        
        _unicode_map['.'] = 46;
        _unicode_map[','] = 44;
        _unicode_map['?'] = 63;
        _unicode_map['!'] = 33;
        _unicode_map['\''] = 39;
        _unicode_map['\"'] = 34;
		
        _unicode_map['['] = 91;
        _unicode_map[']'] = 93;        
        _unicode_map['{'] = 123;
        _unicode_map['}'] = 125;
        _unicode_map['#'] = 35;
        _unicode_map['%'] = 37;
        _unicode_map['^'] = 94;
        _unicode_map['*'] = 42;
        _unicode_map['+'] = 43;
        _unicode_map['='] = 61;

        _unicode_map['_'] = 95;
        _unicode_map['\\'] = 92;
        _unicode_map['|'] = 124;
        _unicode_map['~'] = 126;
        _unicode_map['<'] = 60;
        _unicode_map['>'] = 62;
        _unicode_map['$'] = 36;
    }
	return self;
}

- (void)dealloc
{
	[super dealloc];
}

#pragma mark -
#pragma mark class methods

// return a keyboard instance
+ (RDPKeyboard*)getSharedRDPKeyboard
{
	static RDPKeyboard* _shared_keyboard = nil;
	
	if (_shared_keyboard == nil)
	{
		@synchronized(self)
		{
			if (_shared_keyboard == nil)
				_shared_keyboard = [[RDPKeyboard alloc] init];		
		}
	}
	
	return _shared_keyboard;	
}

// reset the keyboard instance and assign the given rdp instance
- (void)initWithSession:(RDPSession *)session delegate:(NSObject<RDPKeyboardDelegate> *)delegate
{
    _alt_pressed = NO;
    _ctrl_pressed = NO;
    _shift_pressed = NO;
    _win_pressed = NO;
    
    _session = session;
    _delegate = delegate;
}

- (void)reset
{
    // reset pressed ctrl, alt, shift or win key
    if(_shift_pressed)
        [self toggleShiftKey];
    if(_alt_pressed)
        [self toggleAltKey];
    if(_ctrl_pressed)
        [self toggleCtrlKey];
    if(_win_pressed)
        [self toggleWinKey];        
}

// handles button pressed input event from the iOS keyboard
// performs all conversions etc.
- (void)sendUnicode:(int)character
{   
    if(isalnum(character))
        [self handleAlphaNumChar:character];
    else
        [self handleSpecialKey:character];

    [self reset];
}

// send a backspace key press
- (void)sendVirtualKeyCode:(int)keyCode
{
    [self sendVirtualKey:keyCode up:NO];    
    [self sendVirtualKey:keyCode up:YES];        
}

#pragma mark modifier key handling
// toggle ctrl key, returns true if pressed, otherwise false
- (void)toggleCtrlKey
{    
    [self sendVirtualKey:VK_LCONTROL up:_ctrl_pressed];
    _ctrl_pressed = !_ctrl_pressed;
    [self notifyDelegateModifiersChanged];    
}

// toggle alt key, returns true if pressed, otherwise false
- (void)toggleAltKey
{
    [self sendVirtualKey:VK_LMENU up:_alt_pressed];
    _alt_pressed = !_alt_pressed;
    [self notifyDelegateModifiersChanged];
}

// toggle shift key, returns true if pressed, otherwise false
- (void)toggleShiftKey
{
    [self sendVirtualKey:VK_LSHIFT up:_shift_pressed];
    _shift_pressed = !_shift_pressed;
    [self notifyDelegateModifiersChanged];
}

// toggle windows key, returns true if pressed, otherwise false
- (void)toggleWinKey
{
    [self sendVirtualKey:(VK_LWIN | KBDEXT) up:_win_pressed];
    _win_pressed = !_win_pressed;
    [self notifyDelegateModifiersChanged];
}

#pragma mark Sending special key strokes

- (void)sendEnterKeyStroke
{
    [self sendVirtualKeyCode:(VK_RETURN | KBDEXT)];
}

- (void)sendEscapeKeyStroke
{
    [self sendVirtualKeyCode:VK_ESCAPE];
}

- (void)sendBackspaceKeyStroke
{
    [self sendVirtualKeyCode:VK_BACK];
}

@end

#pragma mark -
@implementation RDPKeyboard (Private)

- (void)handleAlphaNumChar:(int)character
{
    // if we recive an uppercase letter - make it lower and send an shift down event to server
    BOOL shift_was_sent = NO;
    if(isupper(character) && _shift_pressed == NO)
    {
        character = tolower(character);
        [self sendVirtualKey:VK_LSHIFT up:NO];
        shift_was_sent = YES;
    }
    
    // convert the character to a VK
    int vk = _virtual_key_map[character];
    if(vk != 0)
    {
        // send key pressed
        [self sendVirtualKey:vk up:NO];    
        [self sendVirtualKey:vk up:YES];        
    }
    
    // send the missing shift up if we had a shift down
    if(shift_was_sent)
        [self sendVirtualKey:VK_LSHIFT up:YES];
}

- (void)handleSpecialKey:(int)character
{
    NSDictionary* eventDescriptor = nil;
    if(character < 256)
    {
        // convert the character to a unicode character
        int code = _unicode_map[character];
        if(code != 0)
            eventDescriptor = [NSDictionary dictionaryWithObjectsAndKeys:	
                               @"keyboard", @"type",
                               @"unicode", @"subtype",
                               [NSNumber numberWithUnsignedShort:0], @"flags",
                               [NSNumber numberWithUnsignedShort:code], @"unicode_char",
                               nil];
    }

    if (eventDescriptor == nil)
        eventDescriptor = [NSDictionary dictionaryWithObjectsAndKeys:	
                           @"keyboard", @"type",
                           @"unicode", @"subtype",
                           [NSNumber numberWithUnsignedShort:0], @"flags",
                           [NSNumber numberWithUnsignedShort:character], @"unicode_char",
                           nil];        

    [_session sendInputEvent:eventDescriptor];        
}

// sends the vk code to the session
- (void)sendVirtualKey:(int)vKey up:(BOOL)up
{
    DWORD scancode = GetVirtualScanCodeFromVirtualKeyCode(vKey, 4);
    int flags = (up ? KBD_FLAGS_RELEASE : KBD_FLAGS_DOWN);
    flags |= ((scancode & KBDEXT) ? KBD_FLAGS_EXTENDED : 0);
    [_session sendInputEvent:[NSDictionary dictionaryWithObjectsAndKeys:	
                                @"keyboard", @"type",
                                @"scancode", @"subtype",
                                [NSNumber numberWithUnsignedShort:flags], @"flags",
                                [NSNumber numberWithUnsignedShort:(scancode & 0xFF)], @"scancode",
                                nil]];        
}

- (void)notifyDelegateModifiersChanged
{
    if ([[self delegate] respondsToSelector:@selector(modifiersChangedForKeyboard:)])
        [[self delegate] modifiersChangedForKeyboard:self];
}

@end
