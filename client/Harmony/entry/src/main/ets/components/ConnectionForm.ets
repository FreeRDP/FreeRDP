import { ConnectionProfile } from '../model/ConnectionProfile';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';
import { i18n } from '../utils/I18n';

@Component
export struct ConnectionForm {
  @Prop profile: ConnectionProfile;
  @Prop canConnect: boolean;
  @Prop rememberPassword: boolean;
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @StorageLink('__app_language__') appLanguage: string = 'system';
  @State colors: ThemeColors = LightColors;
  @State gatewayExpanded: boolean = false;
  onNameChange: (value: string) => void = (_value: string): void => {};
  onHostChange: (value: string) => void = (_value: string): void => {};
  onPortChange: (value: string) => void = (_value: string): void => {};
  onUserChange: (value: string) => void = (_value: string): void => {};
  onDomainChange: (value: string) => void = (_value: string): void => {};
  onPasswordChange: (value: string) => void = (_value: string): void => {};
  onRememberPasswordChange: (value: boolean) => void = (_value: boolean): void => {};
  onGatewayChange: (value: string) => void = (_value: string): void => {};
  onGatewayUserChange: (value: string) => void = (_value: string): void => {};
  onGatewayPasswordChange: (value: string) => void = (_value: string): void => {};
  onGatewayDomainChange: (value: string) => void = (_value: string): void => {};
  onSecurityModeChange: (value: string) => void = (_value: string): void => {};
  onClipboardChange: (value: boolean) => void = (_value: boolean): void => {};
  onNew: () => void = (): void => {};
  onSave: () => void = (): void => {};
  onConnect: () => void = (): void => {};

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  aboutToAppear(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  private hasGatewayConfig(): boolean {
    return !!(this.profile.gateway || this.profile.gatewayUsername ||
      this.profile.gatewayPassword || this.profile.gatewayDomain);
  }

  build() {
    Column({ space: 18 }) {
      Row() {
        Text(i18n.get(this.appLanguage, 'connection_settings'))
          .fontSize(18)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.colors.foreground)
          .layoutWeight(1)

        Button(i18n.get(this.appLanguage, 'new_connection'))
          .type(ButtonType.Normal)
          .fontSize(14)
          .height(38)
          .backgroundColor(this.colors.muted)
          .fontColor(this.colors.mutedForeground)
          .borderRadius(10)
          .padding({ left: 16, right: 16 })
          .onClick(() => this.onNew())
      }
      .width('100%')

      Column({ space: 14 }) {
        TextInput({ placeholder: i18n.get(this.appLanguage, 'connection_name_placeholder'), text: this.profile.name })
          .backgroundColor(this.colors.inputBackground)
          .borderRadius(12)
          .border({ width: 1, color: this.colors.inputBorder })
          .fontSize(16)
          .height(50)
          .padding({ left: 16, right: 16 })
          .placeholderColor(this.colors.mutedForeground)
          .fontColor(this.colors.foreground)
          .onChange((v) => this.onNameChange(v));

        TextInput({ placeholder: i18n.get(this.appLanguage, 'server_address_placeholder'), text: this.profile.host })
          .backgroundColor(this.colors.inputBackground)
          .borderRadius(12)
          .border({ width: 1, color: this.colors.inputBorder })
          .fontSize(16)
          .height(50)
          .padding({ left: 16, right: 16 })
          .placeholderColor(this.colors.mutedForeground)
          .fontColor(this.colors.foreground)
          .onChange((v) => this.onHostChange(v));

        Row({ space: 12 }) {
          TextInput({ placeholder: i18n.get(this.appLanguage, 'port_placeholder'), text: `${this.profile.port}` })
            .type(InputType.Number)
            .backgroundColor(this.colors.inputBackground)
            .borderRadius(12)
            .border({ width: 1, color: this.colors.inputBorder })
            .fontSize(16)
            .height(50)
            .padding({ left: 16, right: 16 })
            .placeholderColor(this.colors.mutedForeground)
            .fontColor(this.colors.foreground)
            .layoutWeight(1)
            .onChange((v) => this.onPortChange(v));
        }

        TextInput({ placeholder: i18n.get(this.appLanguage, 'username_placeholder'), text: this.profile.username })
          .backgroundColor(this.colors.inputBackground)
          .borderRadius(12)
          .border({ width: 1, color: this.colors.inputBorder })
          .fontSize(16)
          .height(50)
          .padding({ left: 16, right: 16 })
          .placeholderColor(this.colors.mutedForeground)
          .fontColor(this.colors.foreground)
          .onChange((v) => this.onUserChange(v));

        TextInput({ placeholder: i18n.get(this.appLanguage, 'domain_optional_placeholder'), text: this.profile.domain })
          .backgroundColor(this.colors.inputBackground)
          .borderRadius(12)
          .border({ width: 1, color: this.colors.inputBorder })
          .fontSize(16)
          .height(50)
          .padding({ left: 16, right: 16 })
          .placeholderColor(this.colors.mutedForeground)
          .fontColor(this.colors.foreground)
          .onChange((v) => this.onDomainChange(v));

        TextInput({ placeholder: i18n.get(this.appLanguage, 'password_placeholder'), text: this.profile.password })
          .type(InputType.Password)
          .backgroundColor(this.colors.inputBackground)
          .borderRadius(12)
          .border({ width: 1, color: this.colors.inputBorder })
          .fontSize(16)
          .height(50)
          .padding({ left: 16, right: 16 })
          .placeholderColor(this.colors.mutedForeground)
          .fontColor(this.colors.foreground)
          .onChange((v) => this.onPasswordChange(v));

        if (this.profile.username.length === 0 || this.profile.password.length === 0) {
          Row({ space: 10 }) {
            Text('i')
              .fontSize(14)
              .fontWeight(FontWeight.Bold)
              .fontColor(this.colors.hintOrange)
              .width(22)
              .height(22)
              .textAlign(TextAlign.Center)
              .backgroundColor(this.colors.hintOrangeBg)
              .borderRadius(11)
            Text(i18n.get(this.appLanguage, 'empty_credentials_hint'))
              .fontSize(14)
              .fontColor(this.colors.hintOrange)
              .lineHeight(20)
              .layoutWeight(1)
          }
          .width('100%')
          .padding(14)
          .backgroundColor(this.colors.hintOrangeBg)
          .borderRadius(12)
        }

        Row() {
          Text(i18n.get(this.appLanguage, 'remember_password'))
            .fontSize(16)
            .fontColor(this.colors.foreground)
            .layoutWeight(1)
          Toggle({ type: ToggleType.Switch, isOn: this.rememberPassword })
            .onChange((value: boolean) => this.onRememberPasswordChange(value))
        }
        .width('100%')
        .padding({ left: 4, right: 4, top: 8, bottom: 8 })
      }

      Column({ space: 0 }) {
        Row() {
          Row({ space: 10 }) {
            Text(this.gatewayExpanded ? '▾' : '▸')
              .fontSize(14)
              .fontColor(this.colors.primary);
            Text(i18n.get(this.appLanguage, 'gateway_settings'))
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
              .fontColor(this.colors.foreground);
          }
          .onClick(() => { this.gatewayExpanded = !this.gatewayExpanded; })

          Blank()

          if (this.hasGatewayConfig()) {
            Text(i18n.get(this.appLanguage, 'gateway_configured'))
              .fontSize(13)
              .fontColor(this.colors.connectedGreen)
              .padding({ left: 10, right: 10, top: 4, bottom: 4 })
              .backgroundColor(this.colors.tagBackground)
              .borderRadius(999)
          }
        }
        .width('100%')
        .padding(16)
        .onClick(() => { this.gatewayExpanded = !this.gatewayExpanded; });

        if (this.gatewayExpanded) {
          Column({ space: 14 }) {
            TextInput({ placeholder: i18n.get(this.appLanguage, 'gateway_address_placeholder'), text: this.profile.gateway })
              .backgroundColor(this.colors.inputBackground)
              .borderRadius(12)
              .border({ width: 1, color: this.colors.inputBorder })
              .fontSize(16)
              .height(50)
              .padding({ left: 16, right: 16 })
              .placeholderColor(this.colors.mutedForeground)
              .fontColor(this.colors.foreground)
              .onChange((v) => this.onGatewayChange(v));
            TextInput({ placeholder: i18n.get(this.appLanguage, 'gateway_username_placeholder'), text: this.profile.gatewayUsername })
              .backgroundColor(this.colors.inputBackground)
              .borderRadius(12)
              .border({ width: 1, color: this.colors.inputBorder })
              .fontSize(16)
              .height(50)
              .padding({ left: 16, right: 16 })
              .placeholderColor(this.colors.mutedForeground)
              .fontColor(this.colors.foreground)
              .onChange((v) => this.onGatewayUserChange(v));
            TextInput({ placeholder: i18n.get(this.appLanguage, 'gateway_domain_placeholder'), text: this.profile.gatewayDomain })
              .backgroundColor(this.colors.inputBackground)
              .borderRadius(12)
              .border({ width: 1, color: this.colors.inputBorder })
              .fontSize(16)
              .height(50)
              .padding({ left: 16, right: 16 })
              .placeholderColor(this.colors.mutedForeground)
              .fontColor(this.colors.foreground)
              .onChange((v) => this.onGatewayDomainChange(v));
            TextInput({ placeholder: i18n.get(this.appLanguage, 'gateway_password_placeholder'), text: this.profile.gatewayPassword })
              .type(InputType.Password)
              .backgroundColor(this.colors.inputBackground)
              .borderRadius(12)
              .border({ width: 1, color: this.colors.inputBorder })
              .fontSize(16)
              .height(50)
              .padding({ left: 16, right: 16 })
              .placeholderColor(this.colors.mutedForeground)
              .fontColor(this.colors.foreground)
              .onChange((v) => this.onGatewayPasswordChange(v));
          }
          .width('100%')
          .padding({ top: 4, bottom: 16 })
          .animation({ duration: 200, curve: Curve.EaseInOut })
        }
      }
      .width('100%')
      .backgroundColor(this.colors.surfaceElevated)
      .border({ width: 1, color: this.colors.surfaceElevatedBorder })
      .borderRadius(16)

      Column({ space: 14 }) {
        Text(i18n.get(this.appLanguage, 'security_mode'))
          .fontSize(16)
          .fontColor(this.colors.foreground)
          .alignSelf(ItemAlign.Start)

        Row({ space: 10 }) {
          Button(i18n.get(this.appLanguage, 'security_auto'))
            .type(ButtonType.Capsule)
            .fontSize(15)
            .height(40)
            .padding({ left: 20, right: 20 })
            .fontColor(this.profile.securityMode === 'auto' ? this.colors.primaryForeground : this.colors.mutedForeground)
            .backgroundColor(this.profile.securityMode === 'auto' ? this.colors.primary : this.colors.muted)
            .onClick(() => this.onSecurityModeChange('auto'))
          Button('NLA')
            .type(ButtonType.Capsule)
            .fontSize(15)
            .height(40)
            .padding({ left: 20, right: 20 })
            .fontColor(this.profile.securityMode === 'nla' ? this.colors.primaryForeground : this.colors.mutedForeground)
            .backgroundColor(this.profile.securityMode === 'nla' ? this.colors.primary : this.colors.muted)
            .onClick(() => this.onSecurityModeChange('nla'))
          Button('TLS')
            .type(ButtonType.Capsule)
            .fontSize(15)
            .height(40)
            .padding({ left: 20, right: 20 })
            .fontColor(this.profile.securityMode === 'tls' ? this.colors.primaryForeground : this.colors.mutedForeground)
            .backgroundColor(this.profile.securityMode === 'tls' ? this.colors.primary : this.colors.muted)
            .onClick(() => this.onSecurityModeChange('tls'))
          Button('RDP')
            .type(ButtonType.Capsule)
            .fontSize(15)
            .height(40)
            .padding({ left: 20, right: 20 })
            .fontColor(this.profile.securityMode === 'rdp' ? this.colors.primaryForeground : this.colors.mutedForeground)
            .backgroundColor(this.profile.securityMode === 'rdp' ? this.colors.primary : this.colors.muted)
            .onClick(() => this.onSecurityModeChange('rdp'))
        }
      }
      .width('100%')
      .padding(16)
      .backgroundColor(this.colors.surfaceElevated)
      .border({ width: 1, color: this.colors.surfaceElevatedBorder })
      .borderRadius(16)

      Row() {
        Text(i18n.get(this.appLanguage, 'clipboard_bidirectional'))
          .fontSize(16)
          .fontColor(this.colors.foreground)
          .layoutWeight(1)
        Toggle({ type: ToggleType.Switch, isOn: this.profile.enableClipboard })
          .onChange((value: boolean) => this.onClipboardChange(value))
      }
      .width('100%')
      .padding({ left: 4, right: 4, top: 8, bottom: 8 })

      Row({ space: 14 }) {
        Button(i18n.get(this.appLanguage, 'save_config'))
          .layoutWeight(1)
          .type(ButtonType.Normal)
          .height(52)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
          .backgroundColor(this.colors.muted)
          .fontColor(this.colors.foreground)
          .borderRadius(14)
          .onClick(() => this.onSave())

        Button(i18n.get(this.appLanguage, 'connect_desktop'))
          .layoutWeight(2)
          .type(ButtonType.Normal)
          .height(52)
          .fontSize(17)
          .fontWeight(FontWeight.Medium)
          .backgroundColor(this.canConnect ? this.colors.primary : this.colors.muted)
          .fontColor(this.canConnect ? this.colors.primaryForeground : this.colors.mutedForeground)
          .borderRadius(14)
          .enabled(this.canConnect)
          .onClick(() => this.onConnect())
      }
      .width('100%')
    }
    .width('100%')
  }
}
