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
    Row({ space: 8 }) {
      Scroll() {
        Row({ space: 8 }) {
          Button(this.isFullscreen ? '退出全屏' : '全屏')
            .type(ButtonType.Normal)
            .fontColor(this.colors.chromeOverlayText)
            .backgroundColor('transparent')
            .enabled(this.stage === SessionStage.Connected)
            .onClick(() => this.onToggleFullscreen())

          Button('键盘')
            .type(ButtonType.Normal)
            .fontColor(this.colors.chromeOverlayText)
            .backgroundColor('transparent')
            .enabled(this.stage === SessionStage.Connected)
            .onClick(() => this.onShowKeyboard())

        Button('Ctrl+Alt+Del')
          .type(ButtonType.Normal)
          .fontColor(this.colors.chromeOverlayText)
          .backgroundColor('transparent')
          .enabled(this.stage === SessionStage.Connected)
          .onClick(() => this.onSendCtrlAltDel())

        Button('Win')
          .type(ButtonType.Normal)
          .fontColor(this.colors.chromeOverlayText)
          .backgroundColor('transparent')
          .enabled(this.stage === SessionStage.Connected)
          .onClick(() => this.onSendWindowsKey())

        Button('Esc')
          .type(ButtonType.Normal)
          .fontColor(this.colors.chromeOverlayText)
          .backgroundColor('transparent')
          .enabled(this.stage === SessionStage.Connected)
          .onClick(() => this.onSendEscape())

        Button('断开')
          .type(ButtonType.Capsule)
          .enabled(this.stage === SessionStage.Connected || this.stage === SessionStage.Connecting)
          .onClick(() => this.onDisconnect())
        }
      }
      .scrollable(ScrollDirection.Horizontal)
      .scrollBar(BarState.Off)
      .width('100%')
    }
    .width('100%')
    .padding({ left: 10, right: 10, top: 10, bottom: 10 })
    .backgroundColor(this.colors.chromeOverlay)
    .border({ width: 1, color: this.colors.chromeOverlayBorder })
    .borderRadius(22)
    .shadow({ radius: 20, color: this.colors.chromeOverlayShadow, offsetX: 0, offsetY: 6 })
  }
}
