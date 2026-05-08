import { SessionEvent } from '../model/SessionModels';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

@CustomDialog
export struct AuthDialog {
  controller: CustomDialogController;
  event: SessionEvent | null = null;
  username: string = '';
  password: string = '';
  domain: string = '';
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @State colors: ThemeColors = LightColors;
  onConfirm: (username: string, password: string, domain: string) => void = () => {};

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  build() {
    Column() {
      Text('需要身份验证')
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .fontColor(this.colors.foreground)
        .margin({ bottom: 4 })

      Text('远程桌面服务器需要您的凭据')
        .fontSize(14)
        .fontColor(this.colors.mutedForeground)
        .margin({ bottom: 24 })

      TextInput({ placeholder: '用户名', text: this.username })
        .backgroundColor(this.colors.inputBackground)
        .border({ width: 1, color: this.colors.inputBorder })
        .borderRadius(12)
        .fontSize(14)
        .padding({ left: 14, right: 14, top: 10, bottom: 10 })
        .placeholderColor(this.colors.mutedForeground)
        .fontColor(this.colors.foreground)
        .onChange((value: string) => {
          this.username = value;
        })
        .margin({ bottom: 12 })

      TextInput({ placeholder: '密码', text: this.password })
        .type(InputType.Password)
        .backgroundColor(this.colors.inputBackground)
        .border({ width: 1, color: this.colors.inputBorder })
        .borderRadius(12)
        .fontSize(14)
        .padding({ left: 14, right: 14, top: 10, bottom: 10 })
        .placeholderColor(this.colors.mutedForeground)
        .fontColor(this.colors.foreground)
        .onChange((value: string) => {
          this.password = value;
        })
        .margin({ bottom: 12 })

      TextInput({ placeholder: '域（可选）', text: this.domain })
        .backgroundColor(this.colors.inputBackground)
        .border({ width: 1, color: this.colors.inputBorder })
        .borderRadius(12)
        .fontSize(14)
        .padding({ left: 14, right: 14, top: 10, bottom: 10 })
        .placeholderColor(this.colors.mutedForeground)
        .fontColor(this.colors.foreground)
        .onChange((value: string) => {
          this.domain = value;
        })
        .margin({ bottom: 24 })

      Row({ space: 12 }) {
        Button('取消')
          .layoutWeight(1)
          .height(44)
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .backgroundColor(this.colors.muted)
          .fontColor(this.colors.foreground)
          .borderRadius(12)
          .onClick(() => {
            this.onConfirm('', '', '');
            this.controller.close();
          })

        Button('确定')
          .layoutWeight(1)
          .height(44)
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .backgroundColor(this.colors.primary)
          .fontColor(this.colors.primaryForeground)
          .borderRadius(12)
          .onClick(() => {
            this.onConfirm(this.username, this.password, this.domain);
            this.controller.close();
          })
      }
      .width('100%')
    }
    .padding(24)
    .width('90%')
    .backgroundColor(this.colors.card)
    .border({ width: 1, color: this.colors.cardBorder })
    .borderRadius(20)
    .shadow({ radius: 24, color: this.colors.shadowLg, offsetX: 0, offsetY: 8 })
  }
}
