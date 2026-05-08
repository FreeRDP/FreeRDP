import { image } from '@kit.ImageKit';
import { inputMethod } from '@kit.IMEKit';
import { SessionSnapshot, SessionStage } from '../model/SessionModels';
import { KeyboardMapper } from '../utils/KeyboardMapper';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

const PTR_FLAGS_BUTTON1: number = 0x1000;
const PTR_FLAGS_BUTTON2: number = 0x2000;
const PTR_FLAGS_BUTTON3: number = 0x4000;

interface RemotePoint {
  x: number;
  y: number;
}

interface RenderMetrics {
  width: number;
  height: number;
  offsetX: number;
  offsetY: number;
}

interface TextDelta {
  deletedCount: number;
  insertedText: string;
}

@Component
export struct RemoteDesktopSurface {
  @Prop sessionId: number;
  @Prop stage: SessionStage;
  @Prop snapshot: SessionSnapshot;
  @Prop keyboardRequestToken: number = 0;
  @Prop isFullscreen: boolean = false;
  @Prop @Watch('onThemeChanged') isDark: boolean = false;
  @State colors: ThemeColors = LightColors;
  getFrameBuffer: () => ArrayBuffer = (): ArrayBuffer => new ArrayBuffer(0);
  setSurfaceId: (surfaceId: string) => Promise<void> = async (_surfaceId: string): Promise<void> => {};
  onMouseMove: (x: number, y: number) => void = (_x: number, _y: number): void => {};
  onMouseButton: (x: number, y: number, buttonFlags: number, down: boolean) => void =
    (_x: number, _y: number, _buttonFlags: number, _down: boolean): void => {};
  onMouseWheel: (x: number, y: number, delta: number) => void =
    (_x: number, _y: number, _delta: number): void => {};
  onKeyScancode: (scancode: number, down: boolean) => void =
    (_scancode: number, _down: boolean): void => {};
  onUnicodeKey: (codepoint: number, down: boolean) => void =
    (_codepoint: number, _down: boolean): void => {};

  @State private surfaceWidth: number = 0;
  @State private surfaceHeight: number = 0;
  @State private framePixelMap: image.PixelMap | null = null;
  @State private softInputDraft: string = '';
  private refreshTimer: number = -1;
  private renderedFrameSequence: number = 0;
  private handledKeyboardRequestToken: number = 0;

  private surfaceController: XComponentController = new XComponentController();

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  private getInputLayerId(): string {
    return 'rdp_input_layer_' + this.sessionId;
  }

  private getSoftInputId(): string {
    return 'rdp_soft_input_' + this.sessionId;
  }

  private getRenderMetrics(): RenderMetrics {
    const viewportWidth: number = this.surfaceWidth > 0 ? this.surfaceWidth : this.snapshot.width;
    const viewportHeight: number = this.surfaceHeight > 0 ? this.surfaceHeight : this.snapshot.height;
    if (viewportWidth <= 0 || viewportHeight <= 0 || this.snapshot.width <= 0 || this.snapshot.height <= 0) {
      return {
        width: Math.max(1, viewportWidth),
        height: Math.max(1, viewportHeight),
        offsetX: 0,
        offsetY: 0
      };
    }

    const viewportRatio: number = viewportWidth / viewportHeight;
    const snapshotRatio: number = this.snapshot.width / this.snapshot.height;

    if (viewportRatio > snapshotRatio) {
      const renderHeight: number = viewportHeight;
      const renderWidth: number = renderHeight * snapshotRatio;
      return {
        width: Math.max(1, renderWidth),
        height: Math.max(1, renderHeight),
        offsetX: (viewportWidth - renderWidth) / 2,
        offsetY: 0
      };
    }

    const renderWidth: number = viewportWidth;
    const renderHeight: number = renderWidth / snapshotRatio;
    return {
      width: Math.max(1, renderWidth),
      height: Math.max(1, renderHeight),
      offsetX: 0,
      offsetY: (viewportHeight - renderHeight) / 2
    };
  }

  private toRemotePoint(localX: number, localY: number): RemotePoint {
    const metrics: RenderMetrics = this.getRenderMetrics();
    const localFrameX: number = localX - metrics.offsetX;
    const localFrameY: number = localY - metrics.offsetY;
    const limitedX: number = Math.max(0, Math.min(Math.max(1, metrics.width) - 1, localFrameX));
    const limitedY: number = Math.max(0, Math.min(Math.max(1, metrics.height) - 1, localFrameY));

    const scaledX: number = Math.max(0, Math.min(this.snapshot.width - 1,
      Math.round(limitedX * this.snapshot.width / Math.max(1, metrics.width))));
    const scaledY: number = Math.max(0, Math.min(this.snapshot.height - 1,
      Math.round(limitedY * this.snapshot.height / Math.max(1, metrics.height))));

    return { x: scaledX, y: scaledY };
  }

  private mapMouseButton(button: number): number {
    if (button === MouseButton.Right) {
      return PTR_FLAGS_BUTTON2;
    }
    if (button === MouseButton.Middle) {
      return PTR_FLAGS_BUTTON3;
    }
    return PTR_FLAGS_BUTTON1;
  }

  private requestInputFocus(): void {
    try {
      const fc = this.getUIContext()?.getFocusController?.();
      fc?.requestFocus?.(this.getSoftInputId());
    } catch (_error) {
    }
    try {
      const controller = inputMethod.getController();
      controller.showTextInput();
    } catch (e) {
      console.warn(`Failed to show soft keyboard: ${e}`);
    }
  }

  private showSoftKeyboard(): void {
    if (this.stage !== SessionStage.Connected) {
      return;
    }
    setTimeout(() => {
      this.requestInputFocus();
    }, 0);
  }

  private buildTextDelta(previous: string, next: string): TextDelta {
    const prevChars: string[] = Array.from(previous ?? '');
    const nextChars: string[] = Array.from(next ?? '');

    let prefix: number = 0;
    while (prefix < prevChars.length && prefix < nextChars.length && prevChars[prefix] === nextChars[prefix]) {
      prefix += 1;
    }

    let suffix: number = 0;
    while (suffix < prevChars.length - prefix &&
      suffix < nextChars.length - prefix &&
      prevChars[prevChars.length - 1 - suffix] === nextChars[nextChars.length - 1 - suffix]) {
      suffix += 1;
    }

    return {
      deletedCount: prevChars.length - prefix - suffix,
      insertedText: nextChars.slice(prefix, nextChars.length - suffix).join('')
    };
  }

  private sendRemoteBackspace(count: number): void {
    const total: number = Math.max(0, Math.floor(count));
    const backspace: number = KeyboardMapper.mapKeyScancode('Backspace', 0);
    if (total <= 0 || backspace <= 0) {
      return;
    }
    let index: number = 0;
    while (index < total) {
      this.onKeyScancode(backspace, true);
      this.onKeyScancode(backspace, false);
      index += 1;
    }
  }

  private sendRemoteCommittedText(text: string): void {
    if (text.length === 0) {
      return;
    }

    for (const ch of text) {
      const codepoint: number = ch.codePointAt(0) ?? 0;
      if (codepoint === 10 || codepoint === 13) {
        const enter: number = KeyboardMapper.mapKeyScancode('Enter', 0);
        if (enter > 0) {
          this.onKeyScancode(enter, true);
          this.onKeyScancode(enter, false);
        }
        continue;
      }
      this.onUnicodeKey(codepoint, true);
      this.onUnicodeKey(codepoint, false);
    }
  }

  private handleSoftInputChange(next: string): void {
    const previous: string = this.softInputDraft ?? '';
    this.softInputDraft = next;
    if (this.stage !== SessionStage.Connected) {
      return;
    }

    const delta: TextDelta = this.buildTextDelta(previous, next);
    if (delta.deletedCount > 0) {
      this.sendRemoteBackspace(delta.deletedCount);
    }
    if (delta.insertedText.length > 0) {
      this.sendRemoteCommittedText(delta.insertedText);
    }
  }

  private handleKey(event: KeyEvent): void {
    if (this.stage !== SessionStage.Connected) {
      return;
    }

    const isDown: boolean = event.type === KeyType.Down;
    const isUp: boolean = event.type === KeyType.Up;
    if (!isDown && !isUp) {
      return;
    }

    const scancode: number = KeyboardMapper.mapKeyScancode(event.keyText, event.keyCode);
    if (scancode > 0) {
      this.onKeyScancode(scancode, isDown);
      return;
    }

    if (event.keyText.length === 1) {
      this.onUnicodeKey(event.keyText.charCodeAt(0), isDown);
    }
  }

  private handleTouch(event: TouchEvent): void {
    if (this.stage !== SessionStage.Connected || event.changedTouches.length < 1) {
      return;
    }

    const touch = event.changedTouches[0];
    const point: RemotePoint = this.toRemotePoint(touch.x, touch.y);
    this.onMouseMove(point.x, point.y);

    if (touch.type === TouchType.Down) {
      this.requestInputFocus();
      this.onMouseButton(point.x, point.y, PTR_FLAGS_BUTTON1, true);
    } else if (touch.type === TouchType.Up || touch.type === TouchType.Cancel) {
      this.onMouseButton(point.x, point.y, PTR_FLAGS_BUTTON1, false);
    }
  }

  private handleMouse(event?: MouseEvent): void {
    if (this.stage !== SessionStage.Connected || event === undefined) {
      return;
    }

    const point: RemotePoint = this.toRemotePoint(event.x, event.y);
    if (event.action === MouseAction.Move) {
      this.onMouseMove(point.x, point.y);
      return;
    }

    if (event.action === MouseAction.Press) {
      this.requestInputFocus();
      this.onMouseButton(point.x, point.y, this.mapMouseButton(event.button), true);
      return;
    }

    if (event.action === MouseAction.Release) {
      this.onMouseButton(point.x, point.y, this.mapMouseButton(event.button), false);
    }
  }

  private releasePixelMap(): void {
    if (this.framePixelMap !== null) {
      try {
        this.framePixelMap.release();
      } catch (_error) {
      }
      this.framePixelMap = null;
    }
  }

  private normalizeFrameBuffer(frame: ArrayBuffer): ArrayBuffer {
    const expectedStride: number = this.snapshot.width * 4;
    if (this.snapshot.width <= 0 || this.snapshot.height <= 0 || expectedStride <= 0) {
      return new ArrayBuffer(0);
    }
    if (this.snapshot.frameStride <= 0 || this.snapshot.frameStride === expectedStride) {
      return frame;
    }

    const src: Uint8Array = new Uint8Array(frame);
    const tight: Uint8Array = new Uint8Array(expectedStride * this.snapshot.height);
    const rowBytes: number = Math.min(expectedStride, this.snapshot.frameStride);
    let y: number = 0;
    while (y < this.snapshot.height) {
      const srcOffset: number = y * this.snapshot.frameStride;
      const dstOffset: number = y * expectedStride;
      let x: number = 0;
      while (x < rowBytes && srcOffset + x < src.length && dstOffset + x < tight.length) {
        tight[dstOffset + x] = src[srcOffset + x];
        x += 1;
      }
      y += 1;
    }
    return tight.buffer;
  }

  private refreshPixelMap(): void {
    if (this.keyboardRequestToken > this.handledKeyboardRequestToken) {
      this.handledKeyboardRequestToken = this.keyboardRequestToken;
      this.showSoftKeyboard();
    }

    if (this.stage !== SessionStage.Connected) {
      return;
    }
    if (this.snapshot.frameSequence <= this.renderedFrameSequence ||
      this.snapshot.width <= 0 ||
      this.snapshot.height <= 0) {
      return;
    }

    const rawFrame: ArrayBuffer = this.getFrameBuffer();
    if (rawFrame.byteLength === 0) {
      return;
    }

    const frame: ArrayBuffer = this.normalizeFrameBuffer(rawFrame);
    const expectedSize: number = this.snapshot.width * this.snapshot.height * 4;
    if (frame.byteLength < expectedSize) {
      return;
    }

    try {
      const nextPixelMap: image.PixelMap = image.createPixelMapSync(frame, {
        size: { width: this.snapshot.width, height: this.snapshot.height },
        srcPixelFormat: image.PixelMapFormat.BGRA_8888,
        pixelFormat: image.PixelMapFormat.BGRA_8888,
        editable: false
      });
      this.releasePixelMap();
      this.framePixelMap = nextPixelMap;
      this.renderedFrameSequence = this.snapshot.frameSequence;
    } catch (error) {
      console.error(`RemoteDesktopSurface refreshPixelMap failed: ${JSON.stringify(error)}`);
    }
  }

  aboutToAppear(): void {
    this.setSurfaceId('');
    setTimeout(() => {
      this.requestInputFocus();
    }, 0);
    this.refreshTimer = setInterval(() => {
      this.refreshPixelMap();
    }, 120);
  }

  aboutToDisappear(): void {
    this.setSurfaceId('');
    if (this.refreshTimer >= 0) {
      clearInterval(this.refreshTimer);
      this.refreshTimer = -1;
    }
    this.releasePixelMap();
  }

  build() {
    Stack() {
      if (this.framePixelMap !== null) {
        Image(this.framePixelMap)
          .width('100%')
          .height('100%')
          .objectFit(ImageFit.Contain)
      } else if (this.stage === SessionStage.Connecting) {
        Column({ space: 12 }) {
          LoadingProgress()
            .color(this.colors.primary)
          Text('正在建立会话并等待首帧...')
            .fontColor(this.colors.mutedForeground)
            .fontSize(14)
        }
      } else if (this.stage === SessionStage.Connected) {
        Text('已连接，正在等待首帧...')
          .fontColor(this.colors.mutedForeground)
          .fontSize(14)
      } else {
        Text('连接后将在这里展示远程桌面画面')
          .fontColor(this.colors.mutedForeground)
          .fontSize(14)
      }

      Column() {
      }
      .id(this.getInputLayerId())
      .width('100%')
      .height('100%')
      .backgroundColor('rgba(0,0,0,0.001)')
      .focusable(true)
      .focusOnTouch(true)
      .defaultFocus(true)
      .onAppear(() => {
        if (this.stage === SessionStage.Connected) {
          this.requestInputFocus();
        }
      })
      .onTouch((event: TouchEvent) => {
        this.handleTouch(event);
      })
      .onMouse((event?: MouseEvent) => {
        this.handleMouse(event);
      })
      .onKeyEvent((event: KeyEvent) => {
        this.handleKey(event);
      })

      if (this.stage === SessionStage.Connected) {
        TextInput({ text: this.softInputDraft, placeholder: '' })
          .id(this.getSoftInputId())
          .width(1)
          .height(1)
          .opacity(0.01)
          .caretColor(Color.Transparent)
          .fontSize(1)
          .fontColor(Color.Transparent)
          .backgroundColor(Color.Transparent)
          .position({ x: -100, y: -100 })
          .focusable(true)
          .focusOnTouch(true)
          .type(InputType.Normal)
          .onChange((value: string) => {
            this.handleSoftInputChange(value);
          })
          .onSubmit((_: EnterKeyType) => {
            const enter: number = KeyboardMapper.mapKeyScancode('Enter', 0);
            if (enter > 0) {
              this.onKeyScancode(enter, true);
              setTimeout(() => {
                this.onKeyScancode(enter, false);
              }, 10);
            }
          })
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor(this.colors.background)
    .borderRadius(this.isFullscreen ? 0 : 18)
    .clip(true)
    .onAreaChange((_, newValue) => {
      this.surfaceWidth = Number(newValue.width);
      this.surfaceHeight = Number(newValue.height);
    })
  }
}
