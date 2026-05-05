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
        .margin({ bottom: 6 })

      Text('远程桌面服务器需要您的凭据。')
        .fontSize(14)
        .fontColor(this.colors.mutedForeground)
        .margin({ bottom: 20 })

      TextInput({ placeholder: '用户名', text: this.username })
        .backgroundColor(this.colors.inputBackground)
        .border({ width: 1, color: this.colors.inputBorder })
        .borderRadius(10)
        .padding({ left: 12, right: 12, top: 10, bottom: 10 })
        .onChange((value: string) => {
          this.username = value;
        })
        .margin({ bottom: 10 })

      TextInput({ placeholder: '密码', text: this.password })
        .type(InputType.Password)
        .backgroundColor(this.colors.inputBackground)
        .border({ width: 1, color: this.colors.inputBorder })
        .borderRadius(10)
        .padding({ left: 12, right: 12, top: 10, bottom: 10 })
        .onChange((value: string) => {
          this.password = value;
        })
        .margin({ bottom: 10 })

      TextInput({ placeholder: '域 (可选)', text: this.domain })
        .backgroundColor(this.colors.inputBackground)
        .border({ width: 1, color: this.colors.inputBorder })
        .borderRadius(10)
        .padding({ left: 12, right: 12, top: 10, bottom: 10 })
        .onChange((value: string) => {
          this.domain = value;
        })
        .margin({ bottom: 20 })

      Row() {
        Button('取消')
          .backgroundColor(this.colors.muted)
          .fontColor(this.colors.mutedForeground)
          .borderRadius(10)
          .onClick(() => {
            this.onConfirm('', '', '');
            this.controller.close();
          })
          .margin({ right: 10 })
          .layoutWeight(1)

        Button('确定')
          .backgroundColor(this.colors.primary)
          .fontColor(this.colors.primaryForeground)
          .borderRadius(10)
          .onClick(() => {
            this.onConfirm(this.username, this.password, this.domain);
            this.controller.close();
          })
          .layoutWeight(1)
      }
      .width('100%')
    }
    .padding(20)
    .width('90%')
    .backgroundColor(this.colors.card)
    .border({ width: 1, color: this.colors.cardBorder })
    .borderRadius(18)
  }
}
