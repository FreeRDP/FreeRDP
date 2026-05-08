import { ConnectionProfile } from '../model/ConnectionProfile';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

@Component
export struct ConnectionForm {
  @Prop profile: ConnectionProfile;
  @Prop canConnect: boolean;
  @Prop rememberPassword: boolean;
  @Prop @Watch('onThemeChanged') isDark: boolean = false;
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

  private hasGatewayConfig(): boolean {
    return !!(this.profile.gateway || this.profile.gatewayUsername ||
      this.profile.gatewayPassword || this.profile.gatewayDomain);
  }

  build() {
    Column({ space: 18 }) {
      Row() {
        Text('连接设置')
          .fontSize(18)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.colors.foreground)
          .layoutWeight(1)

        Button('新建')
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
        TextInput({ placeholder: '连接名称', text: this.profile.name })
          .backgroundColor(this.colors.inputBackground)
          .borderRadius(12)
          .border({ width: 1, color: this.colors.inputBorder })
          .fontSize(16)
          .height(50)
          .padding({ left: 16, right: 16 })
          .placeholderColor(this.colors.mutedForeground)
          .fontColor(this.colors.foreground)
          .onChange((v) => this.onNameChange(v));

        TextInput({ placeholder: '服务器地址', text: this.profile.host })
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
          TextInput({ placeholder: '端口', text: `${this.profile.port}` })
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

        TextInput({ placeholder: '用户名', text: this.profile.username })
          .backgroundColor(this.colors.inputBackground)
          .borderRadius(12)
          .border({ width: 1, color: this.colors.inputBorder })
          .fontSize(16)
          .height(50)
          .padding({ left: 16, right: 16 })
          .placeholderColor(this.colors.mutedForeground)
          .fontColor(this.colors.foreground)
          .onChange((v) => this.onUserChange(v));

        TextInput({ placeholder: '域（可选）', text: this.profile.domain })
          .backgroundColor(this.colors.inputBackground)
          .borderRadius(12)
          .border({ width: 1, color: this.colors.inputBorder })
          .fontSize(16)
          .height(50)
          .padding({ left: 16, right: 16 })
          .placeholderColor(this.colors.mutedForeground)
          .fontColor(this.colors.foreground)
          .onChange((v) => this.onDomainChange(v));

        TextInput({ placeholder: '密码', text: this.profile.password })
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
            Text('不填写凭据时，将显示远端系统登录界面，可直接在远端输入。')
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
          Text('记住密码')
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
            Text('网关设置')
              .fontSize(16)
              .fontWeight(FontWeight.Medium)
              .fontColor(this.colors.foreground);
          }
          .onClick(() => { this.gatewayExpanded = !this.gatewayExpanded; })

          Blank()

          if (this.hasGatewayConfig()) {
            Text('已配置')
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
            TextInput({ placeholder: '网关地址（host 或 host:port）', text: this.profile.gateway })
              .backgroundColor(this.colors.inputBackground)
              .borderRadius(12)
              .border({ width: 1, color: this.colors.inputBorder })
              .fontSize(16)
              .height(50)
              .padding({ left: 16, right: 16 })
              .placeholderColor(this.colors.mutedForeground)
              .fontColor(this.colors.foreground)
              .onChange((v) => this.onGatewayChange(v));
            TextInput({ placeholder: '网关用户名', text: this.profile.gatewayUsername })
              .backgroundColor(this.colors.inputBackground)
              .borderRadius(12)
              .border({ width: 1, color: this.colors.inputBorder })
              .fontSize(16)
              .height(50)
              .padding({ left: 16, right: 16 })
              .placeholderColor(this.colors.mutedForeground)
              .fontColor(this.colors.foreground)
              .onChange((v) => this.onGatewayUserChange(v));
            TextInput({ placeholder: '网关域', text: this.profile.gatewayDomain })
              .backgroundColor(this.colors.inputBackground)
              .borderRadius(12)
              .border({ width: 1, color: this.colors.inputBorder })
              .fontSize(16)
              .height(50)
              .padding({ left: 16, right: 16 })
              .placeholderColor(this.colors.mutedForeground)
              .fontColor(this.colors.foreground)
              .onChange((v) => this.onGatewayDomainChange(v));
            TextInput({ placeholder: '网关密码', text: this.profile.gatewayPassword })
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
        Text('安全模式')
          .fontSize(16)
          .fontColor(this.colors.foreground)
          .alignSelf(ItemAlign.Start)

        Row({ space: 10 }) {
          Button('自动')
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
        Text('双向剪贴板')
          .fontSize(16)
          .fontColor(this.colors.foreground)
          .layoutWeight(1)
        Toggle({ type: ToggleType.Switch, isOn: this.profile.enableClipboard })
          .onChange((value: boolean) => this.onClipboardChange(value))
      }
      .width('100%')
      .padding({ left: 4, right: 4, top: 8, bottom: 8 })

      Row({ space: 14 }) {
        Button('保存配置')
          .layoutWeight(1)
          .type(ButtonType.Normal)
          .height(52)
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
          .backgroundColor(this.colors.muted)
          .fontColor(this.colors.foreground)
          .borderRadius(14)
          .onClick(() => this.onSave())

        Button('连接桌面')
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
