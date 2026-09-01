import { SessionEvent } from '../model/SessionModels';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';
import { i18n } from '../utils/I18n';

@CustomDialog
export struct CertDialog {
  controller: CustomDialogController;
  event: SessionEvent | null = null;
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @StorageLink('__app_language__') appLanguage: string = 'system';
  @State colors: ThemeColors = LightColors;
  onConfirm: (accept: boolean) => void = () => {};

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  aboutToAppear(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  build() {
    Column() {
      Row() {
        Column({ space: 4 }) {
          Text(i18n.get(this.appLanguage, 'cert_verify'))
            .fontSize(18)
            .fontWeight(FontWeight.Bold)
            .fontColor(this.colors.foreground);
          Text(i18n.get(this.appLanguage, 'cert_unable_verify'))
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
          this.buildCertRow(i18n.get(this.appLanguage, 'cert_hostname'), this.event.certHost);
          this.buildCertRow(i18n.get(this.appLanguage, 'cert_subject'), this.event.certSubject);
          this.buildCertRow(i18n.get(this.appLanguage, 'cert_issuer'), this.event.certIssuer);
          this.buildCertRow(i18n.get(this.appLanguage, 'cert_fingerprint'), this.event.certFingerprint);
        }
        .alignItems(HorizontalAlign.Start)
        .width('100%')
        .padding(14)
        .backgroundColor(this.colors.muted)
        .borderRadius(12)
        .margin({ bottom: 24 })
      }

      Row({ space: 12 }) {
        Button(i18n.get(this.appLanguage, 'cert_reject'))
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

        Button(i18n.get(this.appLanguage, 'cert_accept'))
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
