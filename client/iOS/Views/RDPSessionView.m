/*
 RDP Session View

 Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

 This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 If a copy of the MPL was not distributed with this file, You can obtain one at
 http://mozilla.org/MPL/2.0/.
 */

#import "RDPSessionView.h"
#import "RDPCursor.h"
#import "RDPKeyboard.h"

#include <winpr/wtypes.h>
#include <freerdp/locale/keyboard.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

@class RDPSessionView;

@interface RDPSessionDisplayLinkTarget : NSObject
{
	RDPSessionView *_view;
}
- (id)initWithView:(RDPSessionView *)view;
- (void)displayLinkDidFire:(CADisplayLink *)displayLink;
- (void)clearView;
@end

@interface RDPSessionView ()
{
	id<MTLDevice> _metalDevice;
	id<MTLCommandQueue> _commandQueue;
	id<MTLRenderPipelineState> _pipelineState;
	id<MTLTexture> _desktopTexture;
	CADisplayLink *_displayLink;
	RDPSessionDisplayLinkTarget *_displayLinkTarget;
	CGRect _pendingDirtyRect;
	BOOL _hasPendingDirtyRect;
	CGSize _drawableSize;
	UIImageView *_cursorImageView;
	CGPoint _cursorPosition;
	CGPoint _cursorHotspot;
}

- (void)configureMetal;
- (void)configureDesktopTexture;
- (void)scheduleRender;
- (void)renderDisplayLink:(CADisplayLink *)displayLink;
@end

@implementation RDPSessionDisplayLinkTarget

- (id)initWithView:(RDPSessionView *)view
{
	self = [super init];
	if (self)
		_view = view;
	return self;
}

- (void)displayLinkDidFire:(CADisplayLink *)displayLink
{
	[_view renderDisplayLink:displayLink];
}

- (void)clearView
{
	_view = nil;
}

@end

@implementation RDPSessionView

+ (Class)layerClass
{
	return [CAMetalLayer class];
}

- (void)setSession:(RDPSession *)session
{
	_session = session;
	[self configureDesktopTexture];
	[self setNeedsDisplayInRemoteRect:[self bounds]];
}

- (void)awakeFromNib
{
	[super awakeFromNib];
	// The view is kept at the remote desktop's native size. UIScrollView applies
	// the final device-size transform without creating a Retina-sized drawable.
	[self setContentScaleFactor:1.0f];
	[self setOpaque:YES];
	[self setBackgroundColor:[UIColor blackColor]];
	[self setClipsToBounds:YES];
	_session = nil;
	_pendingDirtyRect = CGRectNull;
	_hasPendingDirtyRect = NO;
	_drawableSize = CGSizeZero;
	_cursorPosition = CGPointZero;
	_cursorHotspot = CGPointZero;
	_cursorImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
	[_cursorImageView setUserInteractionEnabled:NO];
	[_cursorImageView setContentMode:UIViewContentModeTopLeft];
	[[_cursorImageView layer] setMagnificationFilter:kCAFilterNearest];
	[[_cursorImageView layer] setMinificationFilter:kCAFilterNearest];
	[_cursorImageView setHidden:YES];
	[self addSubview:_cursorImageView];
	[self configureMetal];
}

- (void)updateRemoteCursorFrame
{
	UIImage *image = [_cursorImageView image];
	if (!image)
		return;

	[_cursorImageView setFrame:CGRectMake(_cursorPosition.x - _cursorHotspot.x,
	                                      _cursorPosition.y - _cursorHotspot.y, [image size].width,
	                                      [image size].height)];
	[self bringSubviewToFront:_cursorImageView];
}

- (void)setRemoteCursor:(RDPCursor *)cursor
{
	if (!cursor || ![cursor image])
	{
		[self hideRemoteCursor];
		return;
	}

	_cursorHotspot = [cursor hotspot];
	[_cursorImageView setImage:[cursor image]];
	[_cursorImageView setHidden:NO];
	[self updateRemoteCursorFrame];
}

- (void)setRemoteCursorPosition:(CGPoint)position
{
	_cursorPosition = position;
	[self updateRemoteCursorFrame];
}

- (void)showRemoteCursor
{
	if ([_cursorImageView image])
		[_cursorImageView setHidden:NO];
}

- (void)hideRemoteCursor
{
	[_cursorImageView setHidden:YES];
}

- (void)setDefaultRemoteCursor
{
	[self setRemoteCursor:[RDPCursor defaultCursor]];
}

- (void)configureMetal
{
	_metalDevice = [MTLCreateSystemDefaultDevice() retain];
	if (!_metalDevice)
		return;

	CAMetalLayer *metalLayer = (CAMetalLayer *)[self layer];
	[metalLayer setDevice:_metalDevice];
	[metalLayer setPixelFormat:MTLPixelFormatBGRA8Unorm];
	[metalLayer setFramebufferOnly:YES];
	[metalLayer setOpaque:YES];
	[metalLayer setContentsScale:1.0f];

	_commandQueue = [_metalDevice newCommandQueue];
	NSString *shaderSource =
	    @"#include <metal_stdlib>\n"
	     "using namespace metal;\n"
	     "struct VertexOut { float4 position [[position]]; float2 texCoord; };\n"
	     "vertex VertexOut desktopVertex(uint vertexID [[vertex_id]]) {\n"
	     "  const float2 positions[4] = { float2(-1.0, 1.0), float2(-1.0, -1.0), "
	     "float2(1.0, 1.0), float2(1.0, -1.0) };\n"
	     "  const float2 texCoords[4] = { float2(0.0, 0.0), float2(0.0, 1.0), "
	     "float2(1.0, 0.0), float2(1.0, 1.0) };\n"
	     "  VertexOut out; out.position = float4(positions[vertexID], 0.0, 1.0); "
	     "out.texCoord = texCoords[vertexID]; return out;\n"
	     "}\n"
	     "fragment float4 desktopFragment(VertexOut in [[stage_in]], "
	     "texture2d<float> desktop [[texture(0)]]) {\n"
	     "  constexpr sampler textureSampler(address::clamp_to_edge, filter::linear);\n"
	     "  float4 color = desktop.sample(textureSampler, in.texCoord);\n"
	     "  return float4(color.rgb, 1.0);\n"
	     "}\n";

	NSError *error = nil;
	id<MTLLibrary> library = [_metalDevice newLibraryWithSource:shaderSource
	                                                    options:nil
	                                                      error:&error];
	if (!library)
	{
		NSLog(@"Failed to create iFreeRDP Metal shader library: %@", error);
		return;
	}

	id<MTLFunction> vertexFunction = [library newFunctionWithName:@"desktopVertex"];
	id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"desktopFragment"];
	MTLRenderPipelineDescriptor *descriptor =
	    [[[MTLRenderPipelineDescriptor alloc] init] autorelease];
	[descriptor setVertexFunction:vertexFunction];
	[descriptor setFragmentFunction:fragmentFunction];
	[[descriptor colorAttachments][0] setPixelFormat:MTLPixelFormatBGRA8Unorm];
	_pipelineState = [_metalDevice newRenderPipelineStateWithDescriptor:descriptor error:&error];
	[vertexFunction release];
	[fragmentFunction release];
	[library release];

	if (!_pipelineState)
	{
		NSLog(@"Failed to create iFreeRDP Metal pipeline: %@", error);
		return;
	}

	_displayLinkTarget = [[RDPSessionDisplayLinkTarget alloc] initWithView:self];
	_displayLink = [[CADisplayLink displayLinkWithTarget:_displayLinkTarget
	                                            selector:@selector(displayLinkDidFire:)] retain];
	[_displayLink setPreferredFramesPerSecond:60];
	[_displayLink setPaused:YES];
	[_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
}

- (void)configureDesktopTexture
{
	[_desktopTexture release];
	_desktopTexture = nil;

	CGContextRef bitmapContext = (_session != nil) ? [_session bitmapContext] : nil;
	if (!_metalDevice || !bitmapContext)
		return;

	NSUInteger width = CGBitmapContextGetWidth(bitmapContext);
	NSUInteger height = CGBitmapContextGetHeight(bitmapContext);
	if ((width == 0) || (height == 0))
		return;

	MTLTextureDescriptor *descriptor =
	    [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
	                                                       width:width
	                                                      height:height
	                                                   mipmapped:NO];
	[descriptor setUsage:MTLTextureUsageShaderRead];
	[descriptor setStorageMode:MTLStorageModeShared];
	_desktopTexture = [_metalDevice newTextureWithDescriptor:descriptor];
}

- (void)prepareForBitmapContextChange
{
	[_displayLink setPaused:YES];
	_hasPendingDirtyRect = NO;
	_pendingDirtyRect = CGRectNull;
	[_desktopTexture release];
	_desktopTexture = nil;
}

- (void)setNeedsDisplayInRemoteRect:(CGRect)rect
{
	if (!_desktopTexture || CGRectIsNull(rect) || CGRectIsEmpty(rect))
		return;

	CGRect textureBounds = CGRectMake(0.0, 0.0, [_desktopTexture width], [_desktopTexture height]);
	rect = CGRectIntersection(CGRectIntegral(rect), textureBounds);
	if (CGRectIsNull(rect) || CGRectIsEmpty(rect))
		return;

	_pendingDirtyRect = _hasPendingDirtyRect ? CGRectUnion(_pendingDirtyRect, rect) : rect;
	_hasPendingDirtyRect = YES;
	[self scheduleRender];
}

- (void)scheduleRender
{
	if ([self window] && _displayLink && _hasPendingDirtyRect)
		[_displayLink setPaused:NO];
}

- (void)didMoveToWindow
{
	[super didMoveToWindow];
	[self scheduleRender];
}

- (void)layoutSubviews
{
	[super layoutSubviews];
	CAMetalLayer *metalLayer = (CAMetalLayer *)[self layer];
	CGSize size = [self bounds].size;
	CGSize drawableSize = CGSizeMake(MAX(1.0, size.width), MAX(1.0, size.height));
	if (CGSizeEqualToSize(drawableSize, _drawableSize))
		return;

	_drawableSize = drawableSize;
	[metalLayer setDrawableSize:drawableSize];

	if (_desktopTexture)
		[self setNeedsDisplayInRemoteRect:CGRectMake(0, 0, [_desktopTexture width],
		                                             [_desktopTexture height])];
}

- (void)renderDisplayLink:(CADisplayLink *)displayLink
{
	(void)displayLink;
	if (!_hasPendingDirtyRect || !_desktopTexture || !_pipelineState)
	{
		[_displayLink setPaused:YES];
		return;
	}

	CGContextRef bitmapContext = (_session != nil) ? [_session bitmapContext] : nil;
	BYTE *source = bitmapContext ? CGBitmapContextGetData(bitmapContext) : nullptr;
	if (!source)
	{
		[_displayLink setPaused:YES];
		return;
	}

	CGRect dirtyRect = _pendingDirtyRect;
	_pendingDirtyRect = CGRectNull;
	_hasPendingDirtyRect = NO;
	NSUInteger x = (NSUInteger)CGRectGetMinX(dirtyRect);
	NSUInteger y = (NSUInteger)CGRectGetMinY(dirtyRect);
	NSUInteger width = (NSUInteger)CGRectGetWidth(dirtyRect);
	NSUInteger height = (NSUInteger)CGRectGetHeight(dirtyRect);
	NSUInteger bytesPerRow = CGBitmapContextGetBytesPerRow(bitmapContext);
	const NSUInteger bytesPerPixel = 4;
	MTLRegion region = MTLRegionMake2D(x, y, width, height);
	[_desktopTexture replaceRegion:region
	                   mipmapLevel:0
	                     withBytes:source + (y * bytesPerRow) + (x * bytesPerPixel)
	                   bytesPerRow:bytesPerRow];

	id<CAMetalDrawable> drawable = [(CAMetalLayer *)[self layer] nextDrawable];
	if (!drawable)
	{
		_pendingDirtyRect = dirtyRect;
		_hasPendingDirtyRect = YES;
		[_displayLink setPaused:YES];
		return;
	}

	MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
	[[pass colorAttachments][0] setTexture:[drawable texture]];
	[[pass colorAttachments][0] setLoadAction:MTLLoadActionClear];
	[[pass colorAttachments][0] setStoreAction:MTLStoreActionStore];
	[[pass colorAttachments][0] setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 1.0)];

	id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
	id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
	[encoder setRenderPipelineState:_pipelineState];
	[encoder setFragmentTexture:_desktopTexture atIndex:0];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
	[encoder endEncoding];
	[commandBuffer presentDrawable:drawable];
	[commandBuffer commit];

	if (!_hasPendingDirtyRect)
		[_displayLink setPaused:YES];
}

static int VKFromHIDUsage(UIKeyboardHIDUsage usage)
{
	if (usage >= UIKeyboardHIDUsageKeyboardA && usage <= UIKeyboardHIDUsageKeyboardZ)
		return VK_KEY_A + (usage - UIKeyboardHIDUsageKeyboardA);
	if (usage >= UIKeyboardHIDUsageKeyboard1 && usage <= UIKeyboardHIDUsageKeyboard9)
		return VK_KEY_1 + (usage - UIKeyboardHIDUsageKeyboard1);
	if (usage >= UIKeyboardHIDUsageKeyboardF1 && usage <= UIKeyboardHIDUsageKeyboardF12)
		return VK_F1 + (usage - UIKeyboardHIDUsageKeyboardF1);

	switch (usage)
	{
		case UIKeyboardHIDUsageKeyboard0:
			return VK_KEY_0;
		case UIKeyboardHIDUsageKeyboardReturnOrEnter:
			return VK_RETURN;
		case UIKeyboardHIDUsageKeyboardEscape:
			return VK_ESCAPE;
		case UIKeyboardHIDUsageKeyboardDeleteOrBackspace:
			return VK_BACK;
		case UIKeyboardHIDUsageKeyboardTab:
			return VK_TAB;
		case UIKeyboardHIDUsageKeyboardSpacebar:
			return VK_SPACE;
		case UIKeyboardHIDUsageKeyboardHyphen:
			return VK_OEM_MINUS;
		case UIKeyboardHIDUsageKeyboardEqualSign:
			return VK_OEM_PLUS;
		case UIKeyboardHIDUsageKeyboardOpenBracket:
			return VK_OEM_4;
		case UIKeyboardHIDUsageKeyboardCloseBracket:
			return VK_OEM_6;
		case UIKeyboardHIDUsageKeyboardBackslash:
			return VK_OEM_5;
		case UIKeyboardHIDUsageKeyboardSemicolon:
			return VK_OEM_1;
		case UIKeyboardHIDUsageKeyboardQuote:
			return VK_OEM_7;
		case UIKeyboardHIDUsageKeyboardGraveAccentAndTilde:
			return VK_OEM_3;
		case UIKeyboardHIDUsageKeyboardComma:
			return VK_OEM_COMMA;
		case UIKeyboardHIDUsageKeyboardPeriod:
			return VK_OEM_PERIOD;
		case UIKeyboardHIDUsageKeyboardSlash:
			return VK_OEM_2;
		case UIKeyboardHIDUsageKeyboardCapsLock:
			return VK_CAPITAL;
		case UIKeyboardHIDUsageKeyboardRightArrow:
			return VK_RIGHT | KBDEXT;
		case UIKeyboardHIDUsageKeyboardLeftArrow:
			return VK_LEFT | KBDEXT;
		case UIKeyboardHIDUsageKeyboardDownArrow:
			return VK_DOWN | KBDEXT;
		case UIKeyboardHIDUsageKeyboardUpArrow:
			return VK_UP | KBDEXT;
		case UIKeyboardHIDUsageKeyboardInsert:
			return VK_INSERT | KBDEXT;
		case UIKeyboardHIDUsageKeyboardHome:
			return VK_HOME | KBDEXT;
		case UIKeyboardHIDUsageKeyboardPageUp:
			return VK_PRIOR | KBDEXT;
		case UIKeyboardHIDUsageKeyboardDeleteForward:
			return VK_DELETE | KBDEXT;
		case UIKeyboardHIDUsageKeyboardEnd:
			return VK_END | KBDEXT;
		case UIKeyboardHIDUsageKeyboardPageDown:
			return VK_NEXT | KBDEXT;
		case UIKeyboardHIDUsageKeyboardLeftControl:
			return VK_LCONTROL;
		case UIKeyboardHIDUsageKeyboardLeftShift:
			return VK_LSHIFT;
		case UIKeyboardHIDUsageKeyboardLeftAlt:
			return VK_LMENU;
		case UIKeyboardHIDUsageKeyboardLeftGUI:
			return VK_LWIN | KBDEXT;
		case UIKeyboardHIDUsageKeyboardRightControl:
			return VK_RCONTROL | KBDEXT;
		case UIKeyboardHIDUsageKeyboardRightShift:
			return VK_RSHIFT;
		case UIKeyboardHIDUsageKeyboardRightAlt:
			return VK_RMENU | KBDEXT;
		case UIKeyboardHIDUsageKeyboardRightGUI:
			return VK_RWIN | KBDEXT;
		default:
			return 0;
	}
}

- (BOOL)canBecomeFirstResponder
{
	return _hardwareKeyboardActive;
}

- (void)handlePresses:(NSSet<UIPress *> *)presses up:(BOOL)up
{
	for (UIPress *press in presses)
	{
		UIKey *key = press.key;
		if (key == nil)
			continue;

		int vk = VKFromHIDUsage(key.keyCode);
		if (vk != 0)
			[[RDPKeyboard getSharedRDPKeyboard] sendVirtualKey:vk up:up];
	}
}

- (void)pressesBegan:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event
{
	if (_hardwareKeyboardActive)
		[self handlePresses:presses up:NO];
	else
		[super pressesBegan:presses withEvent:event];
}

- (void)pressesEnded:(NSSet<UIPress *> *)presses withEvent:(UIPressesEvent *)event
{
	if (_hardwareKeyboardActive)
		[self handlePresses:presses up:YES];
	else
		[super pressesEnded:presses withEvent:event];
}

- (void)dealloc
{
	[_displayLinkTarget clearView];
	[_displayLink invalidate];
	[_displayLink release];
	[_displayLinkTarget release];
	[_desktopTexture release];
	[_pipelineState release];
	[_commandQueue release];
	[_metalDevice release];
	[_cursorImageView release];
	[super dealloc];
}

@end
