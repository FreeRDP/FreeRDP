import { SessionStage } from '../model/SessionModels';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

@Component
export struct SessionToolbar {
  @Prop stage: SessionStage;
  @Prop isFullscreen: boolean = false;
  @Prop @Watch('onThemeChanged') isDark: boolean = false;
  @State colors: ThemeColors = LightColors;
  onSendCtrlAltDel: () => void = (): void => {};
  onSendWindowsKey: () => void = (): void => {};
  onSendEscape: () => void = (): void => {};
  onShowKeyboard: () => void = (): void => {};
  onToggleFullscreen: () => void = (): void => {};
  onDisconnect: () => void = (): void => {};

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  build() {
    Row({ space: 6 }) {
      Scroll() {
        Row({ space: 6 }) {
          this.buildToolbarBtn(this.isFullscreen ? '退出全屏' : '全屏', () => this.onToggleFullscreen());
          this.buildToolbarBtn('键盘', () => this.onShowKeyboard());
          this.buildToolbarBtn('Ctrl+Alt+Del', () => this.onSendCtrlAltDel());
          this.buildToolbarBtn('Win', () => this.onSendWindowsKey());
          this.buildToolbarBtn('Esc', () => this.onSendEscape());

          Blank().width(8);

          Button('断开')
            .type(ButtonType.Capsule)
            .height(32)
            .fontSize(12)
            .fontWeight(FontWeight.Medium)
            .fontColor('#ef4444')
            .backgroundColor('rgba(239,68,68,0.12)')
            .enabled(this.stage === SessionStage.Connected || this.stage === SessionStage.Connecting)
            .onClick(() => this.onDisconnect())
        }
      }
      .scrollable(ScrollDirection.Horizontal)
      .scrollBar(BarState.Off)
      .width('100%')
    }
    .width('100%')
    .padding({ left: 12, right: 12, top: 10, bottom: 10 })
    .backgroundColor(this.colors.chromeOverlay)
    .border({ width: 1, color: this.colors.chromeOverlayBorder })
    .borderRadius(22)
    .shadow({ radius: 20, color: this.colors.chromeOverlayShadow, offsetX: 0, offsetY: 6 })
  }

  @Builder
  private buildToolbarBtn(label: string, onTap: () => void): void {
    Button(label)
      .type(ButtonType.Normal)
      .height(32)
      .fontSize(12)
      .fontColor(this.colors.chromeOverlayText)
      .backgroundColor(Color.Transparent)
      .borderRadius(10)
      .enabled(this.stage === SessionStage.Connected)
      .onClick(onTap);
  }
}
