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
import { BUILD_VERSION } from '../utils/BuildInfo';
import dataPreferences from '@ohos.data.preferences';
import { util } from '@kit.ArkTS';

interface MarkdownBlock {
  key: string;
  type: number;
  level: number;
  text: string;
}

const MARKDOWN_BLOCK_PARAGRAPH = 0;
const MARKDOWN_BLOCK_HEADING = 1;
const MARKDOWN_BLOCK_BULLET = 2;
const MARKDOWN_BLOCK_BLOCKQUOTE = 3;
const AGREEMENT_DIALOG_NONE = 0;
const AGREEMENT_DIALOG_PRIVACY = 1;
const AGREEMENT_DIALOG_USER = 2;
const AGREEMENT_DIALOG_LAUNCH = 3;
const LAUNCH_AGREEMENTS_ACCEPTED_KEY = 'bihu_launch_agreements_v1';
const PRIVACY_POLICY_RAWFILE = 'bihu-privacy-policy.md';
const USER_AGREEMENT_RAWFILE = 'bihu-user-agreement.md';
const PROJECT_CONTACT_EMAIL = 'contact@hengqu.world';
const PROJECT_HOMEPAGE_URL = 'https://www.hengqu.world';
const PROJECT_SOURCE_URL = 'https://github.com/hengquworld/freerdp-harmony';

@Entry
@Component
struct Index {
  @State homeViewModel: HomeViewModel = new HomeViewModel();
  @State pageWidth: number = 1280;
  @State activeSessions: SessionRecord[] = [];
  @State isConnecting: boolean = false;
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @State colors: ThemeColors = LightColors;
  @State aboutOpen: boolean = false;
  @State agreementDialogMode: number = AGREEMENT_DIALOG_NONE;
  @State launchAgreementsAccepted: boolean = false;
  @State privacyPolicyBlocks: MarkdownBlock[] = [];
  @State userAgreementBlocks: MarkdownBlock[] = [];
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

    void this.loadAgreements();
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

  private createFallbackAgreementText(title: string): string {
    return `# ${title}\n\n协议内容加载失败，请稍后重试。`;
  }

  private stripInlineMarkdown(value: string): string {
    return value
      .replace(/\*\*(.*?)\*\*/g, '$1')
      .replace(/__(.*?)__/g, '$1')
      .replace(/`(.*?)`/g, '$1')
      .replace(/\[(.*?)\]\((.*?)\)/g, '$1');
  }

  private parseMarkdownBlocks(markdown: string): MarkdownBlock[] {
    const blocks: MarkdownBlock[] = [];
    const paragraphBuffer: string[] = [];

    const pushBlock = (type: number, level: number, text: string): void => {
      blocks.push({
        key: `${type}-${blocks.length}`,
        type,
        level,
        text
      });
    };

    const flushParagraph = (): void => {
      if (paragraphBuffer.length === 0) return;
      pushBlock(
        MARKDOWN_BLOCK_PARAGRAPH,
        0,
        paragraphBuffer.map(line => this.stripInlineMarkdown(line.trim())).join('\n')
      );
      paragraphBuffer.length = 0;
    };

    markdown.split('\n').forEach((rawLine: string) => {
      const line = rawLine.trim();
      if (!line) {
        flushParagraph();
        return;
      }
      if (line.startsWith('### ')) {
        flushParagraph();
        pushBlock(MARKDOWN_BLOCK_HEADING, 3, this.stripInlineMarkdown(line.substring(4).trim()));
        return;
      }
      if (line.startsWith('## ')) {
        flushParagraph();
        pushBlock(MARKDOWN_BLOCK_HEADING, 2, this.stripInlineMarkdown(line.substring(3).trim()));
        return;
      }
      if (line.startsWith('# ')) {
        flushParagraph();
        pushBlock(MARKDOWN_BLOCK_HEADING, 1, this.stripInlineMarkdown(line.substring(2).trim()));
        return;
      }
      if (line.startsWith('> ')) {
        flushParagraph();
        pushBlock(MARKDOWN_BLOCK_BLOCKQUOTE, 0, this.stripInlineMarkdown(line.substring(2).trim()));
        return;
      }
      if (line.startsWith('- ')) {
        flushParagraph();
        pushBlock(MARKDOWN_BLOCK_BULLET, 0, this.stripInlineMarkdown(line.substring(2).trim()));
        return;
      }
      paragraphBuffer.push(line);
    });

    flushParagraph();
    return blocks;
  }

  private loadAgreementText(rawfileName: string, fallbackTitle: string): string {
    try {
      const ctx = this.getHostContext();
      if (!ctx) {
        return this.createFallbackAgreementText(fallbackTitle);
      }
      const rawFile = ctx.resourceManager.getRawFileContentSync(rawfileName);
      return util.TextDecoder.create('utf-8', { ignoreBOM: true }).decodeToString(rawFile);
    } catch (_e) {
      console.error(`Failed to load agreement rawfile: ${rawfileName}`);
      return this.createFallbackAgreementText(fallbackTitle);
    }
  }

  private async loadAgreements(): Promise<void> {
    const privacyText = this.loadAgreementText(PRIVACY_POLICY_RAWFILE, '隐私协议');
    const userText = this.loadAgreementText(USER_AGREEMENT_RAWFILE, '用户协议');

    this.privacyPolicyBlocks = this.parseMarkdownBlocks(privacyText);
    this.userAgreementBlocks = this.parseMarkdownBlocks(userText);

    try {
      const ctx = this.getHostContext();
      if (!ctx) return;
      const pref = await dataPreferences.getPreferences(ctx, { name: 'freerdp_store' });
      const accepted = !!((await pref.get(LAUNCH_AGREEMENTS_ACCEPTED_KEY, false)) as boolean);
      this.launchAgreementsAccepted = accepted;
      if (!accepted) {
        this.agreementDialogMode = AGREEMENT_DIALOG_LAUNCH;
      }
    } catch (_e) {
      console.error('Failed to load launch agreements state');
      this.launchAgreementsAccepted = false;
      this.agreementDialogMode = AGREEMENT_DIALOG_LAUNCH;
    }
  }

  private getHostContext(): common.UIAbilityContext | null {
    try {
      return this.getUIContext().getHostContext() as common.UIAbilityContext;
    } catch {
      return null;
    }
  }

  private openAboutDialog(): void {
    this.aboutOpen = true;
  }

  private closeAboutDialog(): void {
    this.aboutOpen = false;
  }

  private openAgreementDialog(mode: number): void {
    this.agreementDialogMode = mode;
  }

  private closeAgreementDialog(): void {
    if (this.agreementDialogMode === AGREEMENT_DIALOG_LAUNCH) {
      return;
    }
    this.agreementDialogMode = AGREEMENT_DIALOG_NONE;
  }

  private isLaunchAgreementDialog(): boolean {
    return this.agreementDialogMode === AGREEMENT_DIALOG_LAUNCH;
  }

  private async acceptLaunchAgreements(): Promise<void> {
    try {
      const ctx = this.getHostContext();
      if (!ctx) return;
      const pref = await dataPreferences.getPreferences(ctx, { name: 'freerdp_store' });
      await pref.put(LAUNCH_AGREEMENTS_ACCEPTED_KEY, true);
      await pref.flush();
      this.launchAgreementsAccepted = true;
      this.agreementDialogMode = AGREEMENT_DIALOG_NONE;
    } catch (_e) {
      console.error('Failed to persist launch agreements consent');
    }
  }

  private async exitApplication(): Promise<void> {
    try {
      const ctx = this.getHostContext();
      if (!ctx) return;
      await ctx.terminateSelf();
    } catch (_e) {
      console.error('Failed to terminate app');
    }
  }

  private getAgreementDialogTitle(): string {
    if (this.agreementDialogMode === AGREEMENT_DIALOG_PRIVACY) {
      return '隐私协议';
    }
    if (this.agreementDialogMode === AGREEMENT_DIALOG_USER) {
      return '用户协议';
    }
    return '协议阅读与确认';
  }

  private getMarkdownHeadingFontSize(level: number): number {
    if (level <= 1) return 18;
    if (level === 2) return 16;
    return 15;
  }

  private openExternalUrl(url: string): void {
    try {
      const ctx = this.getHostContext();
      if (ctx) {
        ctx.openLink(url);
      }
    } catch (_e) {
      console.error('Failed to open URL');
    }
  }

  private copyText(text: string, message: string): void {
    console.info(`Copied: ${text}`);
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
    Stack() {
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

                Button('关于')
                  .height(30)
                  .fontSize(12)
                  .fontColor(this.colors.accent)
                  .backgroundColor(this.colors.card)
                  .border({ width: 1, color: this.colors.cardBorder })
                  .borderRadius(12)
                  .padding({ left: 12, right: 12 })
                  .onClick(() => this.openAboutDialog())
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

      this.buildAboutOverlay()
      this.buildAgreementOverlay()
    }
    .width('100%')
    .height('100%')
  }

  @Builder
  private buildAboutOverlay(): void {
    if (this.aboutOpen) {
      Stack() {
        Column() {
        }
        .width('100%')
        .height('100%')
        .backgroundColor('rgba(12,20,22,0.60)')
        .onClick(() => this.closeAboutDialog());

        Column() {
          Row() {
            Column() {
              Row() {
                Text('🐆')
                  .fontSize(32)
                  .margin({ right: 12 });
                Column() {
                  Text('碧虎远程桌面')
                    .fontSize(22)
                    .fontWeight(FontWeight.Bold)
                    .fontColor(this.colors.foreground);
                  Text(`Version ${BUILD_VERSION}`)
                    .fontSize(13)
                    .fontColor(this.colors.mutedForeground)
                    .margin({ top: 2 });
                }
                .alignItems(HorizontalAlign.Start);
              }
              .alignItems(VerticalAlign.Center);
            }
            .layoutWeight(1)
            .alignItems(HorizontalAlign.Start);

            Button() {
              Text('✕')
                .fontSize(18)
                .fontColor(this.colors.mutedForeground);
            }
            .width(36)
            .height(36)
            .backgroundColor(Color.Transparent)
            .borderRadius(18)
            .onClick(() => this.closeAboutDialog());
          }
          .alignItems(VerticalAlign.Center)
          .width('100%')
          .padding({ left: 24, right: 20, top: 24, bottom: 20 });

          Blank()
            .height(1)
            .color(this.colors.cardBorder)
            .margin({ left: 24, right: 24 });

          Scroll() {
            Column() {
              Column() {
                Row() {
                  Text('🏢')
                    .fontSize(18)
                    .margin({ right: 8 });
                  Text('公司信息')
                    .fontSize(14)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(this.colors.foreground);
                }
                .alignItems(VerticalAlign.Center)
                .margin({ bottom: 16 });

                Column() {
                  Row() {
                    Text('开发团队')
                      .fontSize(13)
                      .fontColor(this.colors.mutedForeground)
                      .width(80);
                    Text('横渠天地（北京）科技有限公司')
                      .fontSize(15)
                      .lineHeight(22)
                      .fontWeight(FontWeight.Medium)
                      .fontColor(this.colors.foreground)
                      .layoutWeight(1);
                  }
                  .alignItems(VerticalAlign.Top)
                  .margin({ bottom: 14 });

                  this.buildAboutActionRow(
                    '商务合作',
                    PROJECT_CONTACT_EMAIL,
                    '写邮件',
                    `mailto:${PROJECT_CONTACT_EMAIL}`,
                    '邮箱已复制'
                  );

                  Blank().height(14);

                  this.buildAboutActionRow(
                    '官方网站',
                    PROJECT_HOMEPAGE_URL,
                    '访问',
                    PROJECT_HOMEPAGE_URL,
                    '网址已复制'
                  );
                }
                .width('100%')
                .padding(18)
                .backgroundColor(this.colors.card)
                .border({ width: 1, color: this.colors.cardBorder })
                .borderRadius(16);
              }
              .padding({ left: 24, right: 24, top: 20 });

              Column() {
                Row() {
                  Text('📋')
                    .fontSize(18)
                    .margin({ right: 8 });
                  Text('协议与隐私')
                    .fontSize(14)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(this.colors.foreground);
                }
                .alignItems(VerticalAlign.Center)
                .margin({ bottom: 16 });

                Column() {
                  this.buildAboutAgreementRow('隐私协议', '查看隐私协议', AGREEMENT_DIALOG_PRIVACY);
                  Blank().height(14);
                  this.buildAboutAgreementRow('用户协议', '查看用户协议', AGREEMENT_DIALOG_USER);
                }
                .width('100%')
                .padding(18)
                .backgroundColor(this.colors.card)
                .border({ width: 1, color: this.colors.cardBorder })
                .borderRadius(16);
              }
              .padding({ left: 24, right: 24, top: 16 });

              Column() {
                Row() {
                  Text('⚖️')
                    .fontSize(18)
                    .margin({ right: 8 });
                  Text('开源许可')
                    .fontSize(14)
                    .fontWeight(FontWeight.Medium)
                    .fontColor(this.colors.foreground);
                }
                .alignItems(VerticalAlign.Center)
                .margin({ bottom: 16 });

                Column() {
                  Row() {
                    Text('开源许可')
                      .fontSize(13)
                      .fontColor(this.colors.mutedForeground)
                      .width(80);
                    Text('本应用采用 Apache-2.0 许可发布，基于 FreeRDP 开源项目开发。')
                      .fontSize(14)
                      .lineHeight(21)
                      .fontColor(this.colors.foreground)
                      .layoutWeight(1);
                  }
                  .alignItems(VerticalAlign.Top)
                  .margin({ bottom: 14 });

                  this.buildAboutActionRow(
                    '源码获取',
                    PROJECT_SOURCE_URL,
                    '访问',
                    PROJECT_SOURCE_URL,
                    '网址已复制'
                  );

                  Blank().height(14);

                  Row() {
                    Text('第三方组件')
                      .fontSize(13)
                      .fontColor(this.colors.mutedForeground)
                      .width(80);
                    Text('• FreeRDP (Apache-2.0)\n• OpenSSL (Apache-2.0)')
                      .fontSize(14)
                      .lineHeight(21)
                      .fontColor(this.colors.foreground)
                      .layoutWeight(1);
                  }
                  .alignItems(VerticalAlign.Top)
                  .margin({ bottom: 14 });

                  Row() {
                    Text('免责声明')
                      .fontSize(13)
                      .fontColor(this.colors.mutedForeground)
                      .width(80);
                    Text('本软件按"现状"提供，不附带任何明示或暗示担保。详见各许可证全文。')
                      .fontSize(14)
                      .lineHeight(21)
                      .fontColor(this.colors.foreground)
                      .layoutWeight(1);
                  }
                  .alignItems(VerticalAlign.Top);
                }
                .width('100%')
                .padding(18)
                .backgroundColor(this.colors.card)
                .border({ width: 1, color: this.colors.cardBorder })
                .borderRadius(16);
              }
              .padding({ left: 24, right: 24, top: 16, bottom: 24 });
            }
          }
          .layoutWeight(1)
          .scrollBar(BarState.Auto)
          .align(Alignment.Top);
        }
        .width('100%')
        .height('86%')
        .constraintSize({ maxWidth: 480 })
        .backgroundColor(this.colors.background)
        .borderRadius(24)
        .shadow({ radius: 32, color: 'rgba(12,20,22,0.18)', offsetX: 0, offsetY: 8 });
      }
      .alignContent(Alignment.Center)
      .width('100%')
      .height('100%');
    }
  }

  @Builder
  private buildAgreementOverlay(): void {
    if (this.agreementDialogMode !== AGREEMENT_DIALOG_NONE) {
      Stack() {
        Column() {
        }
        .width('100%')
        .height('100%')
        .backgroundColor('rgba(12,20,22,0.66)')
        .onClick(() => {
          if (!this.isLaunchAgreementDialog()) {
            this.closeAgreementDialog();
          }
        });

        Column() {
          Row() {
            Column() {
              Text(this.getAgreementDialogTitle())
                .fontSize(22)
                .fontWeight(FontWeight.Bold)
                .fontColor(this.colors.foreground);
              Text(this.isLaunchAgreementDialog()
                ? '首次使用前，请完整阅读并确认《碧虎远程桌面隐私协议》与《碧虎远程桌面用户协议》，同意后方可继续使用应用。'
                : '以下内容与首启展示保持一致，支持在应用内随时查看完整文本。')
                .fontSize(13)
                .lineHeight(20)
                .fontColor(this.colors.mutedForeground)
                .margin({ top: 6 });
            }
            .layoutWeight(1)
            .alignItems(HorizontalAlign.Start);

            if (!this.isLaunchAgreementDialog()) {
              Button() {
                Text('✕')
                  .fontSize(18)
                  .fontColor(this.colors.mutedForeground);
              }
              .width(36)
              .height(36)
              .backgroundColor(Color.Transparent)
              .borderRadius(18)
              .onClick(() => this.closeAgreementDialog());
            }
          }
          .alignItems(VerticalAlign.Top)
          .width('100%')
          .padding({ left: 24, right: 20, top: 24, bottom: 20 });

          Blank()
            .height(1)
            .color(this.colors.cardBorder)
            .margin({ left: 24, right: 24 });

          Scroll() {
            Column({ space: 20 }) {
              if (this.agreementDialogMode === AGREEMENT_DIALOG_LAUNCH) {
                this.buildAgreementSection('隐私协议', this.privacyPolicyBlocks);
                Blank().height(1).color(this.colors.cardBorder).width('100%');
                this.buildAgreementSection('用户协议', this.userAgreementBlocks);
              } else if (this.agreementDialogMode === AGREEMENT_DIALOG_PRIVACY) {
                this.buildAgreementSection('隐私协议', this.privacyPolicyBlocks);
              } else {
                this.buildAgreementSection('用户协议', this.userAgreementBlocks);
              }
            }
            .width('100%')
            .padding({ left: 24, right: 24, top: 20, bottom: 20 })
            .alignItems(HorizontalAlign.Start);
          }
          .layoutWeight(1)
          .scrollBar(BarState.Auto)
          .align(Alignment.Top);

          Blank()
            .height(1)
            .color(this.colors.cardBorder)
            .margin({ left: 24, right: 24 });

          Row() {
            if (this.isLaunchAgreementDialog()) {
              Button('退出应用')
                .layoutWeight(1)
                .height(44)
                .fontSize(15)
                .fontWeight(FontWeight.Medium)
                .fontColor(this.colors.foreground)
                .backgroundColor(this.colors.muted)
                .border({ width: 1, color: this.colors.cardBorder })
                .borderRadius(14)
                .onClick(() => {
                  void this.exitApplication();
                });

              Blank().width(12);

              Button('同意并继续')
                .layoutWeight(1)
                .height(44)
                .fontSize(15)
                .fontWeight(FontWeight.Medium)
                .fontColor(this.colors.accentForeground)
                .backgroundColor(this.colors.accent)
                .borderRadius(14)
                .onClick(() => {
                  void this.acceptLaunchAgreements();
                });
            } else {
              Button('关闭')
                .width('100%')
                .height(44)
                .fontSize(15)
                .fontWeight(FontWeight.Medium)
                .fontColor(this.colors.accentForeground)
                .backgroundColor(this.colors.accent)
                .borderRadius(14)
                .onClick(() => this.closeAgreementDialog());
            }
          }
          .width('100%')
          .padding({ left: 24, right: 24, top: 16, bottom: 20 })
          .alignItems(VerticalAlign.Center);
        }
        .width('100%')
        .height('88%')
        .constraintSize({ maxWidth: 720 })
        .backgroundColor(this.colors.background)
        .borderRadius(24)
        .shadow({ radius: 32, color: 'rgba(12,20,22,0.18)', offsetX: 0, offsetY: 8 });
      }
      .alignContent(Alignment.Center)
      .width('100%')
      .height('100%');
    }
  }

  @Builder
  private buildAboutActionRow(label: string, value: string, actionLabel: string, uri: string, copyMessage: string): void {
    Row() {
      Text(label)
        .fontSize(13)
        .fontColor(this.colors.mutedForeground)
        .width(80);
      Column() {
        Text(value)
          .fontSize(15)
          .lineHeight(22)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.colors.accent)
          .onClick(() => this.openExternalUrl(uri));
        Row() {
          Text(actionLabel)
            .fontSize(12)
            .lineHeight(18)
            .fontColor(this.colors.accent)
            .onClick(() => this.openExternalUrl(uri));
          Text('复制')
            .fontSize(12)
            .lineHeight(18)
            .fontColor(this.colors.mutedForeground)
            .margin({ left: 16 })
            .onClick(() => this.copyText(value, copyMessage));
        }
        .margin({ top: 4 });
      }
      .layoutWeight(1)
      .alignItems(HorizontalAlign.Start);
    }
    .alignItems(VerticalAlign.Top);
  }

  @Builder
  private buildAboutAgreementRow(label: string, title: string, mode: number): void {
    Row() {
      Text(label)
        .fontSize(13)
        .fontColor(this.colors.mutedForeground)
        .width(80);
      Column() {
        Text(title)
          .fontSize(15)
          .lineHeight(22)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.colors.accent);
        Text('应用内完整查看')
          .fontSize(12)
          .lineHeight(18)
          .fontColor(this.colors.mutedForeground)
          .margin({ top: 2 });
      }
      .layoutWeight(1)
      .alignItems(HorizontalAlign.Start);
    }
    .alignItems(VerticalAlign.Top)
    .onClick(() => this.openAgreementDialog(mode));
  }

  @Builder
  private buildAgreementMarkdownBlock(block: MarkdownBlock): void {
    if (block.type === MARKDOWN_BLOCK_HEADING) {
      Text(block.text)
        .width('100%')
        .fontSize(this.getMarkdownHeadingFontSize(block.level))
        .lineHeight(block.level <= 1 ? 28 : 24)
        .fontWeight(FontWeight.Medium)
        .fontColor(this.colors.foreground);
    } else if (block.type === MARKDOWN_BLOCK_BULLET) {
      Row() {
        Text('•')
          .width(18)
          .fontSize(14)
          .lineHeight(22)
          .fontColor(this.colors.foreground);
        Text(block.text)
          .layoutWeight(1)
          .fontSize(14)
          .lineHeight(22)
          .fontColor(this.colors.mutedForeground);
      }
      .width('100%')
      .alignItems(VerticalAlign.Top);
    } else if (block.type === MARKDOWN_BLOCK_BLOCKQUOTE) {
      Row() {
        Blank()
          .width(3)
          .backgroundColor(this.colors.cardBorder)
          .borderRadius(1.5);
        Text(block.text)
          .layoutWeight(1)
          .fontSize(14)
          .lineHeight(22)
          .fontColor(this.colors.mutedForeground)
          .padding({ left: 12, right: 8, top: 6, bottom: 6 });
      }
      .width('100%')
      .alignItems(VerticalAlign.Top);
    } else {
      Text(block.text)
        .width('100%')
        .fontSize(14)
        .lineHeight(22)
        .fontColor(this.colors.mutedForeground);
    }
  }

  @Builder
  private buildAgreementMarkdown(blocks: MarkdownBlock[]): void {
    Column({ space: 12 }) {
      if (blocks.length === 0) {
        Text('协议内容加载中...')
          .width('100%')
          .fontSize(14)
          .lineHeight(22)
          .fontColor(this.colors.mutedForeground);
      } else {
        ForEach(blocks, (block: MarkdownBlock) => {
          this.buildAgreementMarkdownBlock(block);
        }, (block: MarkdownBlock) => block.key);
      }
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start);
  }

  @Builder
  private buildAgreementSection(title: string, blocks: MarkdownBlock[]): void {
    Column({ space: 12 }) {
      Text(title)
        .fontSize(18)
        .fontWeight(FontWeight.Bold)
        .fontColor(this.colors.foreground)
        .width('100%');

      Blank()
        .height(1)
        .color(this.colors.cardBorder);

      this.buildAgreementMarkdown(blocks);
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start);
  }
}
