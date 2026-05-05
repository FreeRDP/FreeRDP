import { ConnectionForm } from '../components/ConnectionForm';
import { BookmarkList } from '../components/BookmarkList';
import { ThemeToggle } from '../components/ThemeToggle';
import { HomeViewModel } from '../viewmodel/HomeViewModel';
import { SessionRecord } from '../service/SessionManager';
import { sessionManager } from '../service/SessionManager';
import { sessionWindowManager } from '../service/SessionWindowManager';
import { SessionStage } from '../model/SessionModels';
import { common } from '@kit.AbilityKit';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

@Entry
@Component
struct Index {
  @State homeViewModel: HomeViewModel = new HomeViewModel();
  @State pageWidth: number = 1280;
  @State activeSessions: SessionRecord[] = [];
  @State isConnecting: boolean = false;
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @State colors: ThemeColors = LightColors;
  private unsubSessionChanged: (() => void) | null = null;
  private unsubWindowClosed: (() => void) | null = null;
  private refreshTimer: number = -1;

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  private isPhoneLayout(): boolean {
    return this.pageWidth < 840;
  }

  private isTabletLayout(): boolean {
    return this.pageWidth >= 840 && this.pageWidth < 1440;
  }

  private layoutSpacing(): number {
    return this.isPhoneLayout() ? 16 : 24;
  }

  private horizontalPadding(): number {
    if (this.isPhoneLayout()) {
      return 16;
    }
    if (this.isTabletLayout()) {
      return 24;
    }
    return 32;
  }

  private contentMaxWidth(): number {
    if (this.isPhoneLayout()) {
      return 960;
    }
    return 800;
  }

  aboutToAppear(): void {
    const context = this.getUIContext().getHostContext() as common.UIAbilityContext;
    void this.homeViewModel.initialize(context);

    this.activeSessions = sessionManager.getActiveSessions();

    this.unsubSessionChanged = sessionManager.onChanged(() => {
      this.activeSessions = sessionManager.getActiveSessions();
    });

    this.unsubWindowClosed = sessionWindowManager.onWindowClosed((_sessionId: number) => {
      this.activeSessions = sessionManager.getActiveSessions();
    });

    this.refreshTimer = setInterval(() => {
      this.activeSessions = sessionManager.getActiveSessions();
    }, 2000);
  }

  aboutToDisappear(): void {
    if (this.unsubSessionChanged !== null) {
      this.unsubSessionChanged();
      this.unsubSessionChanged = null;
    }
    if (this.unsubWindowClosed !== null) {
      this.unsubWindowClosed();
      this.unsubWindowClosed = null;
    }
    if (this.refreshTimer >= 0) {
      clearInterval(this.refreshTimer);
      this.refreshTimer = -1;
    }
  }

  private async connectAndOpenWindow(): Promise<void> {
    if (this.isConnecting) {
      return;
    }

    const profile = this.homeViewModel.profile;
    if (!this.homeViewModel.canConnect) {
      this.homeViewModel.lastMessage = '请填写服务器地址和用户名';
      return;
    }

    this.isConnecting = true;
    this.homeViewModel.lastMessage = '正在连接远程桌面...';

    try {
      const sessionId: number = await sessionManager.createAndConnect(profile);
      if (sessionId <= 0) {
        this.homeViewModel.lastMessage = '创建会话失败';
        this.isConnecting = false;
        return;
      }

      const opened: boolean = await sessionWindowManager.openSessionWindow(sessionId, profile);
      if (!opened) {
        this.homeViewModel.lastMessage = '打开远程桌面窗口失败';
        await sessionManager.disconnect(sessionId);
        sessionManager.removeSession(sessionId);
      } else {
        this.homeViewModel.lastMessage = `已连接到 ${profile.host}，远程桌面窗口已打开`;
      }
    } catch (e) {
      this.homeViewModel.lastMessage = '连接失败: ' + (e as Error).message;
    }

    this.isConnecting = false;
    this.activeSessions = sessionManager.getActiveSessions();
  }

  private async focusSession(sessionId: number): Promise<void> {
    await sessionWindowManager.openSessionWindow(sessionId);
  }

  private async disconnectSession(sessionId: number): Promise<void> {
    await sessionWindowManager.closeSessionWindow(sessionId);
    this.activeSessions = sessionManager.getActiveSessions();
  }

  private stageLabel(stage: SessionStage): string {
    if (stage === SessionStage.Connected) {
      return '已连接';
    }
    if (stage === SessionStage.Connecting) {
      return '连接中';
    }
    if (stage === SessionStage.Disconnected) {
      return '已断开';
    }
    if (stage === SessionStage.Error) {
      return '错误';
    }
    return '空闲';
  }

  private stageColor(stage: SessionStage): string {
    if (stage === SessionStage.Connected) {
      return this.colors.connectedGreen;
    }
    if (stage === SessionStage.Connecting) {
      return this.colors.connectingAmber;
    }
    if (stage === SessionStage.Error) {
      return this.colors.errorRed;
    }
    return this.colors.disconnectedGray;
  }

  build() {
    Navigation() {
      Scroll() {
        Column() {
          Column({ space: this.layoutSpacing() }) {
            Row() {
              Column({ space: 4 }) {
                Text('碧虎远程桌面')
                  .fontSize(this.isPhoneLayout() ? 26 : 36)
                  .fontWeight(FontWeight.Bold)
                  .fontColor(this.colors.foreground)
                  .animation({ duration: 200, curve: Curve.EaseIn })

                Text('安全的远程访问，克制的技术实践')
                  .fontSize(13)
                  .fontColor(this.colors.mutedForeground)
                  .animation({ duration: 200, curve: Curve.EaseIn })
              }
              .alignItems(HorizontalAlign.Start)
              .layoutWeight(1)

              Row({ space: 10 }) {
                ThemeToggle()

                Text('HarmonyOS')
                  .fontSize(11)
                  .fontColor(this.colors.tagText)
                  .padding({ left: 10, right: 10, top: 6, bottom: 6 })
                  .backgroundColor(this.colors.tagBackground)
                  .borderRadius(999)
              }
            }
            .alignItems(VerticalAlign.Top)

            if (this.activeSessions.length > 0) {
              Column({ space: 12 }) {
                Row() {
                  Text('活跃会话')
                    .fontSize(18)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(this.colors.foreground)
                    .layoutWeight(1)

                  Text(`${this.activeSessions.length} 个连接`)
                    .fontSize(12)
                    .fontColor(this.colors.mutedForeground)
                }
                .width('100%')

                ForEach(this.activeSessions, (record: SessionRecord) => {
                  Row() {
                    Column({ space: 4 }) {
                      Text(record.state.title)
                        .fontSize(15)
                        .fontWeight(FontWeight.Medium)
                        .fontColor(this.colors.cardForeground)
                        .maxLines(1)
                        .textOverflow({ overflow: TextOverflow.Ellipsis })

                      Text(record.state.message)
                        .fontSize(12)
                        .fontColor(this.colors.mutedForeground)
                        .maxLines(1)
                        .textOverflow({ overflow: TextOverflow.Ellipsis })
                    }
                    .layoutWeight(1)
                    .alignItems(HorizontalAlign.Start)

                    Text(this.stageLabel(record.state.stage))
                      .fontSize(11)
                      .fontColor(this.stageColor(record.state.stage))
                      .padding({ left: 10, right: 10, top: 5, bottom: 5 })
                      .backgroundColor(this.colors.muted)
                      .borderRadius(999)

                    Row({ space: 8 }) {
                      Button('切换')
                        .type(ButtonType.Normal)
                        .fontSize(12)
                        .height(32)
                        .backgroundColor(this.colors.accent)
                        .fontColor(this.colors.accentForeground)
                        .borderRadius(8)
                        .onClick(() => this.focusSession(record.sessionId))

                      Button('断开')
                        .type(ButtonType.Normal)
                        .fontSize(12)
                        .height(32)
                        .backgroundColor('rgba(185,28,28,0.08)')
                        .fontColor(this.colors.destructive)
                        .borderRadius(8)
                        .onClick(() => this.disconnectSession(record.sessionId))
                    }
                  }
                  .width('100%')
                  .padding(16)
                  .backgroundColor(this.colors.card)
                  .border({ width: 1, color: this.colors.cardBorder })
                  .borderRadius(14)
                }, (record: SessionRecord) => `${record.sessionId}`)
              }
              .padding(24)
              .backgroundColor(this.colors.card)
              .borderRadius(24)
              .border({ width: 1, color: this.colors.cardBorder })
            }

            Column({ space: 16 }) {
              Column({ space: 12 }) {
                Text('连接工作台')
                  .fontSize(20)
                  .fontWeight(FontWeight.Medium)
                  .fontColor(this.colors.foreground)
                  .alignSelf(ItemAlign.Start)
              }
              .padding(24)
              .backgroundColor(this.colors.card)
              .border({ width: 1, color: this.colors.cardBorder })
              .borderRadius(24)

              Column({ space: 16 }) {
                ConnectionForm({
                  profile: this.homeViewModel.profile,
                  rememberPassword: this.homeViewModel.profile.rememberPassword,
                  isDark: this.isDark,
                  onNameChange: (value: string) => this.homeViewModel.updateName(value),
                  onHostChange: (value: string) => this.homeViewModel.updateHost(value),
                  onPortChange: (value: string) => this.homeViewModel.updatePort(value),
                  onUserChange: (value: string) => this.homeViewModel.updateUsername(value),
                  onDomainChange: (value: string) => this.homeViewModel.updateDomain(value),
                  onPasswordChange: (value: string) => this.homeViewModel.updatePassword(value),
                  onRememberPasswordChange: (value: boolean) => this.homeViewModel.toggleRememberPassword(value),
                  onGatewayChange: (value: string) => this.homeViewModel.updateGateway(value),
                  onGatewayUserChange: (value: string) => this.homeViewModel.updateGatewayUsername(value),
                  onGatewayPasswordChange: (value: string) => this.homeViewModel.updateGatewayPassword(value),
                  onGatewayDomainChange: (value: string) => this.homeViewModel.updateGatewayDomain(value),
                  onSecurityModeChange: (value: string) => this.homeViewModel.updateSecurityMode(value),
                  onClipboardChange: (value: boolean) => this.homeViewModel.toggleClipboard(value),
                  onNew: () => { this.homeViewModel.createNewProfile(); },
                  onSave: () => { void this.homeViewModel.saveCurrentProfile(); },
                  canConnect: this.homeViewModel.canConnect && !this.isConnecting,
                  onConnect: () => { void this.connectAndOpenWindow(); }
                })

                BookmarkList({
                  items: this.homeViewModel.bookmarks,
                  isDark: this.isDark,
                  onSelect: (profile) => {
                    this.homeViewModel.applyBookmark(profile);
                  },
                  onDelete: (profileId) => {
                    void this.homeViewModel.deleteBookmark(profileId);
                  },
                  onQuickConnect: (profile) => {
                    this.homeViewModel.applyBookmark(profile);
                    void this.connectAndOpenWindow();
                  }
                })
              }
              .padding(20)
              .backgroundColor(this.colors.card)
              .border({ width: 1, color: this.colors.cardBorder })
              .borderRadius(24)

              Text(this.homeViewModel.lastMessage)
                .fontSize(12)
                .fontColor(this.colors.mutedForeground)
                .alignSelf(ItemAlign.Start)
            }
            .width('100%')
            .alignItems(HorizontalAlign.Start)
          }
          .width(this.contentMaxWidth())
          .margin({ left: 'auto', right: 'auto' })
          .padding({
            left: this.horizontalPadding(),
            right: this.horizontalPadding(),
            top: this.isPhoneLayout() ? 24 : 40,
            bottom: 40
          })
        }
        .width('100%')
      }
      .width('100%')
      .height('100%')
      .backgroundColor(this.colors.background)
      .animation({ duration: 300, curve: Curve.EaseIn })
      .onAreaChange((_, newValue) => {
        this.pageWidth = Number(newValue.width);
      })
    }
    .hideTitleBar(true)
    .navBarWidth(this.isPhoneLayout() ? '100%' : '30%')
    .mode(NavigationMode.Stack)
    .backgroundColor(this.colors.background)
  }
}
