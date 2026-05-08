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
      Row() {
        Column({ space: 4 }) {
          Text('证书验证')
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .fontColor(this.colors.foreground);
          Text('无法验证此远程计算机的身份，是否仍要连接？')
            .fontSize(13)
            .fontColor(this.colors.mutedForeground)
            .lineHeight(19);
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)

        Button() {
          Text('×')
            .fontSize(22)
            .fontWeight(FontWeight.Lighter)
            .fontColor(this.colors.mutedForeground);
        }
        .width(32)
        .height(32)
        .backgroundColor(Color.Transparent)
        .borderRadius(16)
        .onClick(() => {
          this.onConfirm(false);
          this.controller.close();
        });
      }
      .width('100%')
      .alignItems(VerticalAlign.Top)
      .margin({ bottom: 20 });

      if (this.event) {
        Column({ space: 8 }) {
          this.buildCertRow('主机名', this.event.certHost);
          this.buildCertRow('主题', this.event.certSubject);
          this.buildCertRow('颁发者', this.event.certIssuer);
          this.buildCertRow('指纹', this.event.certFingerprint);
        }
        .alignItems(HorizontalAlign.Start)
        .width('100%')
        .padding(14)
        .backgroundColor(this.colors.muted)
        .borderRadius(12)
        .margin({ bottom: 24 })
      }

      Row({ space: 12 }) {
        Button('拒绝连接')
          .layoutWeight(1)
          .height(44)
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .backgroundColor(this.colors.muted)
          .fontColor(this.colors.foreground)
          .borderRadius(12)
          .onClick(() => {
            this.onConfirm(false);
            this.controller.close();
          })

        Button('接受并连接')
          .layoutWeight(1)
          .height(44)
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .backgroundColor(this.colors.primary)
          .fontColor(this.colors.primaryForeground)
          .borderRadius(12)
          .onClick(() => {
            this.onConfirm(true);
            this.controller.close();
          })
      }
      .width('100%')
    }
    .padding(24)
    .width('92%')
    .backgroundColor(this.colors.card)
    .border({ width: 1, color: this.colors.cardBorder })
    .borderRadius(20)
    .shadow({ radius: 24, color: this.colors.shadowLg, offsetX: 0, offsetY: 8 })
  }

  @Builder
  private buildCertRow(label: string, value: string): void {
    Row() {
      Text(label)
        .fontSize(12)
        .fontColor(this.colors.mutedForeground)
        .width(52)
        .constraintSize({ minWidth: 52 });
      Text(value || '—')
        .fontSize(13)
        .fontColor(this.colors.foreground)
        .maxLines(1)
        .textOverflow({ overflow: TextOverflow.Ellipsis })
        .layoutWeight(1);
    }
    .width('100%')
    .alignItems(VerticalAlign.Center);
  }
}
