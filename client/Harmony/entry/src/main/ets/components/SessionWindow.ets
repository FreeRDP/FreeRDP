import { SessionSnapshot, SessionStage } from '../model/SessionModels';
import { RemoteDesktopSurface } from './RemoteDesktopSurface';
import { SessionToolbar } from './SessionToolbar';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

@Component
export struct SessionWindow {
  @Prop sessionId: number;
  @Prop title: string;
  @Prop subtitle: string;
  @Prop stage: SessionStage;
  @Prop snapshot: SessionSnapshot;
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
  onSendCtrlAltDel: () => void = (): void => {};
  onSendWindowsKey: () => void = (): void => {};
  onSendEscape: () => void = (): void => {};
  onDisconnect: () => void = (): void => {};
  @State private isFullscreen: boolean = false;
  @State private keyboardRequestToken: number = 0;
  @State private chromeVisible: boolean = true;
  private chromeHideTimer: number = -1;

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  private scheduleChromeAutoHide(): void {
    if (this.chromeHideTimer !== -1) {
      clearTimeout(this.chromeHideTimer);
    }
    this.chromeHideTimer = setTimeout(() => {
      this.chromeVisible = false;
      this.chromeHideTimer = -1;
    }, 3200);
  }

  private revealChrome(): void {
    this.chromeVisible = true;
    this.scheduleChromeAutoHide();
  }

  private extendChromeInteractionIfVisible(): void {
    if (this.chromeVisible) {
      this.scheduleChromeAutoHide();
    }
  }

  aboutToAppear(): void {
    this.scheduleChromeAutoHide();
  }

  aboutToDisappear(): void {
    if (this.chromeHideTimer !== -1) {
      clearTimeout(this.chromeHideTimer);
      this.chromeHideTimer = -1;
    }
  }

  build() {
    Stack() {
      RemoteDesktopSurface({
        sessionId: this.sessionId,
        stage: this.stage,
        snapshot: this.snapshot,
        keyboardRequestToken: this.keyboardRequestToken,
        isFullscreen: this.isFullscreen,
        isDark: this.isDark,
        getFrameBuffer: this.getFrameBuffer,
        setSurfaceId: this.setSurfaceId,
        onMouseMove: this.onMouseMove,
        onMouseButton: (x, y, btn, down) => {
          this.extendChromeInteractionIfVisible();
          this.onMouseButton(x, y, btn, down);
        },
        onMouseWheel: this.onMouseWheel,
        onKeyScancode: (code, down) => {
          this.extendChromeInteractionIfVisible();
          this.onKeyScancode(code, down);
        },
        onUnicodeKey: (code, down) => {
          this.extendChromeInteractionIfVisible();
          this.onUnicodeKey(code, down);
        }
      })

      if (this.chromeVisible) {
        Column() {
          Row() {
            Column({ space: 4 }) {
              Text(this.title)
                .fontSize(this.isFullscreen ? 18 : 20)
                .fontWeight(FontWeight.Medium)
                .fontColor(this.colors.chromeOverlayText)
                .maxLines(1)
                .textOverflow({ overflow: TextOverflow.Ellipsis })

              Text(this.subtitle)
                .fontSize(12)
                .fontColor(this.colors.mutedForeground)
                .maxLines(1)
                .textOverflow({ overflow: TextOverflow.Ellipsis })
            }
            .alignItems(HorizontalAlign.Start)
            .layoutWeight(1)

            Text(this.stage)
              .fontSize(12)
              .fontColor(this.colors.tagText)
              .padding({ left: 10, right: 10, top: 6, bottom: 6 })
              .backgroundColor(this.colors.tagBackground)
              .borderRadius(999)
          }
          .width('100%')
          .alignItems(VerticalAlign.Center)
          .padding({ left: 14, right: 14, top: 12, bottom: 12 })
          .backgroundColor(this.colors.chromeOverlay)
          .border({ width: 1, color: this.colors.chromeOverlayBorder })
          .borderRadius(22)
          .shadow({ radius: 20, color: this.colors.chromeOverlayShadow, offsetX: 0, offsetY: 6 })
          .onTouch((ev: TouchEvent) => {
            if (ev.type === TouchType.Down) this.extendChromeInteractionIfVisible();
          })

          Blank()
            .layoutWeight(1)
            .onClick(() => this.scheduleChromeAutoHide())

          SessionToolbar({
            stage: this.stage,
            isFullscreen: this.isFullscreen,
            isDark: this.isDark,
            onSendCtrlAltDel: this.onSendCtrlAltDel,
            onSendWindowsKey: this.onSendWindowsKey,
            onSendEscape: this.onSendEscape,
            onShowKeyboard: (): void => {
              this.keyboardRequestToken += 1;
            },
            onToggleFullscreen: (): void => {
              this.isFullscreen = !this.isFullscreen;
            },
            onDisconnect: this.onDisconnect
          })
          .onTouch((ev: TouchEvent) => {
            if (ev.type === TouchType.Down) this.extendChromeInteractionIfVisible();
          })
        }
        .width('100%')
        .height('100%')
        .padding(this.isFullscreen ? 12 : 16)
        .hitTestBehavior(HitTestMode.Transparent)
      } else {
        Row() {
          Column({ space: 5 }) {
            Blank().width(4).height(4).borderRadius(2).backgroundColor(this.colors.dotActive)
            Blank().width(4).height(4).borderRadius(2).backgroundColor(this.colors.dotInactive)
            Blank().width(4).height(4).borderRadius(2).backgroundColor(this.colors.dotInactive)
          }
        }
        .width(18)
        .height(72)
        .justifyContent(FlexAlign.Center)
        .backgroundColor(this.colors.chromeOverlay)
        .border({ width: 1, color: this.colors.chromeOverlayBorder })
        .borderRadius({ topLeft: 12, bottomLeft: 12, topRight: 0, bottomRight: 0 })
        .position({ x: '100%', y: '50%' })
        .translate({ x: '-100%', y: '-50%' })
        .onClick(() => this.revealChrome())
        .onTouch((ev: TouchEvent) => {
          if (ev.type === TouchType.Down) this.revealChrome();
        })
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor(this.colors.background)
    .padding(this.isFullscreen ? 0 : 12)
    .animation({ duration: 300, curve: Curve.EaseIn })
  }
}
