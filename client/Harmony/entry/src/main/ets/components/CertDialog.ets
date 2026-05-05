import { SessionEvent } from '../model/SessionModels';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

@CustomDialog
export struct CertDialog {
  controller: CustomDialogController;
  event: SessionEvent | null = null;
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @State colors: ThemeColors = LightColors;
  onConfirm: (accept: boolean) => void = () => {};

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  build() {
    Column() {
      Text('证书验证')
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .fontColor(this.colors.foreground)
        .margin({ bottom: 6 })

      Text('无法验证此远程计算机的身份。是否仍要连接？')
        .fontSize(14)
        .fontColor(this.colors.mutedForeground)
        .margin({ bottom: 20 })

      if (this.event) {
        Column({ space: 6 }) {
          Text(`主机: ${this.event.certHost}`)
            .fontSize(12)
            .fontColor(this.colors.mutedForeground)
          Text(`主题: ${this.event.certSubject}`)
            .fontSize(12)
            .fontColor(this.colors.mutedForeground)
          Text(`颁发者: ${this.event.certIssuer}`)
            .fontSize(12)
            .fontColor(this.colors.mutedForeground)
          Text(`指纹: ${this.event.certFingerprint}`)
            .fontSize(12)
            .fontColor(this.colors.mutedForeground)
        }
        .alignItems(HorizontalAlign.Start)
        .width('100%')
        .padding(12)
        .backgroundColor(this.colors.muted)
        .borderRadius(10)
        .margin({ bottom: 20 })
      }

      Row() {
        Button('拒绝')
          .backgroundColor(this.colors.muted)
          .fontColor(this.colors.mutedForeground)
          .borderRadius(10)
          .onClick(() => {
            this.onConfirm(false);
            this.controller.close();
          })
          .margin({ right: 10 })
          .layoutWeight(1)

        Button('接受并连接')
          .backgroundColor(this.colors.primary)
          .fontColor(this.colors.primaryForeground)
          .borderRadius(10)
          .onClick(() => {
            this.onConfirm(true);
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
