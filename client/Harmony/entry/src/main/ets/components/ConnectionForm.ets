import { ConnectionProfile } from '../model/ConnectionProfile';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

@Component
export struct ConnectionForm {
  @Prop profile: ConnectionProfile;
  @Prop canConnect: boolean;
  @Prop rememberPassword: boolean;
  @Prop @Watch('onThemeChanged') isDark: boolean = false;
  @State colors: ThemeColors = LightColors;
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

  build() {
    Column({ space: 12 }) {
      Row() {
        Text('连接设置')
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.colors.foreground)
          .layoutWeight(1)

        Button('新建')
          .type(ButtonType.Normal)
          .fontSize(12)
          .height(32)
          .backgroundColor(this.colors.muted)
          .fontColor(this.colors.mutedForeground)
          .borderRadius(8)
          .onClick(() => this.onNew())
      }
      .width('100%')
      .padding({ bottom: 4 })

      TextInput({ placeholder: '连接名称', text: this.profile.name })
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onNameChange(value))

      TextInput({ placeholder: '服务器地址', text: this.profile.host })
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onHostChange(value))

      TextInput({ placeholder: '端口', text: `${this.profile.port}` })
        .type(InputType.Number)
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onPortChange(value))

      TextInput({ placeholder: '用户名', text: this.profile.username })
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onUserChange(value))

      TextInput({ placeholder: '域，可选', text: this.profile.domain })
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onDomainChange(value))

      TextInput({ placeholder: '密码', text: this.profile.password })
        .type(InputType.Password)
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onPasswordChange(value))

      if (this.profile.username.length === 0 || this.profile.password.length === 0) {
        Text('提示：不填写用户名或密码时，连接后将显示远端登录页面（Windows/Linux），可直接在远端界面输入。')
          .fontSize(12)
          .fontColor(this.colors.hintOrange)
          .backgroundColor(this.colors.hintOrangeBg)
          .padding(10)
          .borderRadius(10)
          .width('100%')
          .margin({ bottom: 4 })
      }

      Row() {
        Text('记住密码')
          .fontColor(this.colors.foreground)
          .layoutWeight(1)
        Toggle({ type: ToggleType.Switch, isOn: this.rememberPassword })
          .onChange((value: boolean) => this.onRememberPasswordChange(value))
      }
      .padding(12)
      .backgroundColor(this.colors.muted)
      .borderRadius(14)

      TextInput({ placeholder: '网关，可选，支持 host 或 host:port', text: this.profile.gateway })
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onGatewayChange(value))

      TextInput({ placeholder: '网关用户名，可选', text: this.profile.gatewayUsername })
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onGatewayUserChange(value))

      TextInput({ placeholder: '网关域，可选', text: this.profile.gatewayDomain })
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onGatewayDomainChange(value))

      TextInput({ placeholder: '网关密码，可选', text: this.profile.gatewayPassword })
        .type(InputType.Password)
        .backgroundColor(this.colors.inputBackground)
        .borderRadius(10)
        .border({ width: 1, color: this.colors.inputBorder })
        .onChange((value: string) => this.onGatewayPasswordChange(value))

      Column({ space: 8 }) {
        Text('安全模式')
          .fontSize(14)
          .fontColor(this.colors.foreground)
          .alignSelf(ItemAlign.Start)

        Row({ space: 8 }) {
          Button('自动')
            .type(this.profile.securityMode === 'auto' ? ButtonType.Capsule : ButtonType.Normal)
            .fontColor(this.profile.securityMode === 'auto' ? this.colors.primaryForeground : this.colors.mutedForeground)
            .backgroundColor(this.profile.securityMode === 'auto' ? this.colors.primary : this.colors.muted)
            .onClick(() => this.onSecurityModeChange('auto'))
          Button('NLA')
            .type(this.profile.securityMode === 'nla' ? ButtonType.Capsule : ButtonType.Normal)
            .fontColor(this.profile.securityMode === 'nla' ? this.colors.primaryForeground : this.colors.mutedForeground)
            .backgroundColor(this.profile.securityMode === 'nla' ? this.colors.primary : this.colors.muted)
            .onClick(() => this.onSecurityModeChange('nla'))
          Button('TLS')
            .type(this.profile.securityMode === 'tls' ? ButtonType.Capsule : ButtonType.Normal)
            .fontColor(this.profile.securityMode === 'tls' ? this.colors.primaryForeground : this.colors.mutedForeground)
            .backgroundColor(this.profile.securityMode === 'tls' ? this.colors.primary : this.colors.muted)
            .onClick(() => this.onSecurityModeChange('tls'))
          Button('RDP')
            .type(this.profile.securityMode === 'rdp' ? ButtonType.Capsule : ButtonType.Normal)
            .fontColor(this.profile.securityMode === 'rdp' ? this.colors.primaryForeground : this.colors.mutedForeground)
            .backgroundColor(this.profile.securityMode === 'rdp' ? this.colors.primary : this.colors.muted)
            .onClick(() => this.onSecurityModeChange('rdp'))
        }
      }
      .padding(12)
      .backgroundColor(this.colors.muted)
      .borderRadius(14)

      Row() {
        Text('双向剪贴板')
          .fontColor(this.colors.foreground)
          .layoutWeight(1)
        Toggle({ type: ToggleType.Switch, isOn: this.profile.enableClipboard })
          .onChange((value: boolean) => this.onClipboardChange(value))
      }
      .padding(12)
      .backgroundColor(this.colors.muted)
      .borderRadius(14)

      Row({ space: 12 }) {
        Button('保存配置')
          .layoutWeight(1)
          .type(ButtonType.Normal)
          .height(44)
          .backgroundColor(this.colors.muted)
          .fontColor(this.colors.mutedForeground)
          .borderRadius(12)
          .onClick(() => this.onSave())

        Button('连接桌面')
          .layoutWeight(2)
          .type(ButtonType.Normal)
          .height(44)
          .backgroundColor(this.colors.primary)
          .fontColor(this.colors.primaryForeground)
          .borderRadius(12)
          .enabled(this.canConnect)
          .onClick(() => this.onConnect())
      }
    }
    .width('100%')
  }
}
