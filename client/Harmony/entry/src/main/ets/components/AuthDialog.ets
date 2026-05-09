import { SessionEvent } from '../model/SessionModels';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';
import { i18n } from '../utils/I18n';

@CustomDialog
export struct AuthDialog {
  controller: CustomDialogController;
  event: SessionEvent | null = null;
  username: string = '';
  password: string = '';
  domain: string = '';
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @StorageLink('__app_language__') appLanguage: string = 'system';
  @State colors: ThemeColors = LightColors;
  onConfirm: (username: string, password: string, domain: string) => void = () => {};

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  build() {
    Column() {
      Text(i18n.get(this.appLanguage, 'auth_required'))
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .fontColor(this.colors.foreground)
        .margin({ bottom: 4 })

      Text(i18n.get(this.appLanguage, 'auth_credentials_prompt'))
        .fontSize(14)
        .fontColor(this.colors.mutedForeground)
        .margin({ bottom: 24 })

      TextInput({ placeholder: i18n.get(this.appLanguage, 'username_placeholder'), text: this.username })
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

      TextInput({ placeholder: i18n.get(this.appLanguage, 'password_placeholder'), text: this.password })
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

      TextInput({ placeholder: i18n.get(this.appLanguage, 'domain_optional_placeholder'), text: this.domain })
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
        Button(i18n.get(this.appLanguage, 'cancel'))
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

        Button(i18n.get(this.appLanguage, 'confirm'))
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
