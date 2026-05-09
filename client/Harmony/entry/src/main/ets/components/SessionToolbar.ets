import { SessionStage } from '../model/SessionModels';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';
import { i18n } from '../utils/I18n';

@Component
export struct SessionToolbar {
  @Prop stage: SessionStage;
  @Prop isFullscreen: boolean = false;
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @StorageLink('__app_language__') appLanguage: string = 'system';
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

  aboutToAppear(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  build() {
    Row({ space: 6 }) {
      Scroll() {
        Row({ space: 6 }) {
          this.buildToolbarBtn(this.isFullscreen ? i18n.get(this.appLanguage, 'exit_fullscreen') : i18n.get(this.appLanguage, 'fullscreen'), () => this.onToggleFullscreen());
          this.buildToolbarBtn(i18n.get(this.appLanguage, 'keyboard'), () => this.onShowKeyboard());
          this.buildToolbarBtn('Ctrl+Alt+Del', () => this.onSendCtrlAltDel());
          this.buildToolbarBtn('Win', () => this.onSendWindowsKey());
          this.buildToolbarBtn('Esc', () => this.onSendEscape());

          Blank().width(8);

          Button(i18n.get(this.appLanguage, 'disconnect'))
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
