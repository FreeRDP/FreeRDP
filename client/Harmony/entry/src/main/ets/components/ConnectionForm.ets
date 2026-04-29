import { ConnectionProfile } from '../model/ConnectionProfile';

@Component
export struct ConnectionForm {
  @Prop profile: ConnectionProfile;
  @Prop canConnect: boolean;
  @Prop rememberPassword: boolean;
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

  build() {
    Column({ space: 12 }) {
      Row() {
        Text('连接设置')
          .fontSize(16)
          .fontWeight(FontWeight.Medium)
          .fontColor('#111827')
          .layoutWeight(1)
        
        Button('新建')
          .type(ButtonType.Normal)
          .fontSize(13)
          .height(32)
          .backgroundColor('#F3F4F6')
          .fontColor('#374151')
          .borderRadius(8)
          .onClick(() => this.onNew())
      }
      .width('100%')
      .padding({ bottom: 4 })

      TextInput({ placeholder: '连接名称', text: this.profile.name })
        .onChange((value: string) => this.onNameChange(value))

      TextInput({ placeholder: '服务器地址', text: this.profile.host })
        .onChange((value: string) => this.onHostChange(value))

      TextInput({ placeholder: '端口', text: `${this.profile.port}` })
        .type(InputType.Number)
        .onChange((value: string) => this.onPortChange(value))

      TextInput({ placeholder: '用户名', text: this.profile.username })
        .onChange((value: string) => this.onUserChange(value))

      TextInput({ placeholder: '域，可选', text: this.profile.domain })
        .onChange((value: string) => this.onDomainChange(value))

      TextInput({ placeholder: '密码', text: this.profile.password })
        .type(InputType.Password)
        .onChange((value: string) => this.onPasswordChange(value))

      if (this.profile.username.length === 0 || this.profile.password.length === 0) {
        Text('提示：不填写用户名或密码时，连接后将显示远端登录页面（Windows/Linux），可直接在远端界面输入。')
          .fontSize(12)
          .fontColor('#FF8C00')
          .backgroundColor('#FFF8E6')
          .padding(10)
          .borderRadius(8)
          .width('100%')
          .margin({ bottom: 4 })
      }

      Row() {
        Text('记住密码')
          .layoutWeight(1)
        Toggle({ type: ToggleType.Switch, isOn: this.rememberPassword })
          .onChange((value: boolean) => this.onRememberPasswordChange(value))
      }
      .padding(12)
      .backgroundColor('#F5F7FA')
      .borderRadius(16)

      TextInput({ placeholder: '网关，可选，支持 host 或 host:port', text: this.profile.gateway })
        .onChange((value: string) => this.onGatewayChange(value))

      TextInput({ placeholder: '网关用户名，可选', text: this.profile.gatewayUsername })
        .onChange((value: string) => this.onGatewayUserChange(value))

      TextInput({ placeholder: '网关域，可选', text: this.profile.gatewayDomain })
        .onChange((value: string) => this.onGatewayDomainChange(value))

      TextInput({ placeholder: '网关密码，可选', text: this.profile.gatewayPassword })
        .type(InputType.Password)
        .onChange((value: string) => this.onGatewayPasswordChange(value))

      Column({ space: 8 }) {
        Text('安全模式')
          .fontSize(14)
          .fontColor('#333333')
          .alignSelf(ItemAlign.Start)

        Row({ space: 8 }) {
          Button('自动')
            .type(this.profile.securityMode === 'auto' ? ButtonType.Capsule : ButtonType.Normal)
            .onClick(() => this.onSecurityModeChange('auto'))
          Button('NLA')
            .type(this.profile.securityMode === 'nla' ? ButtonType.Capsule : ButtonType.Normal)
            .onClick(() => this.onSecurityModeChange('nla'))
          Button('TLS')
            .type(this.profile.securityMode === 'tls' ? ButtonType.Capsule : ButtonType.Normal)
            .onClick(() => this.onSecurityModeChange('tls'))
          Button('RDP')
            .type(this.profile.securityMode === 'rdp' ? ButtonType.Capsule : ButtonType.Normal)
            .onClick(() => this.onSecurityModeChange('rdp'))
        }
      }
      .padding(12)
      .backgroundColor('#F5F7FA')
      .borderRadius(16)

      Row() {
        Text('双向剪贴板')
          .layoutWeight(1)
        Toggle({ type: ToggleType.Switch, isOn: this.profile.enableClipboard })
          .onChange((value: boolean) => this.onClipboardChange(value))
      }
      .padding(12)
      .backgroundColor('#F5F7FA')
      .borderRadius(16)

      Row({ space: 12 }) {
        Button('保存配置')
          .layoutWeight(1)
          .type(ButtonType.Normal)
          .height(44)
          .backgroundColor('#F3F4F6')
          .fontColor('#374151')
          .borderRadius(12)
          .onClick(() => this.onSave())

        Button('连接桌面')
          .layoutWeight(2)
          .type(ButtonType.Normal)
          .height(44)
          .backgroundColor('#2E6BE6')
          .fontColor('#FFFFFF')
          .borderRadius(12)
          .enabled(this.canConnect)
          .onClick(() => this.onConnect())
      }
    }
    .width('100%')
  }
}
