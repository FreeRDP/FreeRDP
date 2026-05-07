import { ConnectionForm } from '../components/ConnectionForm';
import { BookmarkList } from '../components/BookmarkList';
import { ThemeToggle } from '../components/ThemeToggle';
import { HomeViewModel } from '../viewmodel/HomeViewModel';
import { SessionRecord } from '../service/SessionManager';
import { sessionManager } from '../service/SessionManager';
import { sessionWindowManager } from '../service/SessionWindowManager';
import { SessionStage } from '../model/SessionModels';
import { ConnectionProfile } from '../model/ConnectionProfile';
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
  @State keyword: string = '';
  @State selectedConnId: string = '';
  @State showDetail: boolean = false;
  @State showDeleteConfirm: boolean = false;
  private unsubSessionChanged: (() => void) | null = null;
  private unsubWindowClosed: (() => void) | null = null;
  private refreshTimer: number = -1;
  private connectionListScroller: Scroller = new Scroller();

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

  private getFilteredBookmarks(): ConnectionProfile[] {
    const kw = (this.keyword ?? '').trim().toLowerCase();
    if (!kw) return this.homeViewModel.bookmarks;
    return this.homeViewModel.bookmarks.filter((c: ConnectionProfile) => {
      const name = (c.name ?? '').toLowerCase();
      const host = (c.host ?? '').toLowerCase();
      const user = (c.username ?? '').toLowerCase();
      return name.includes(kw) || host.includes(kw) || user.includes(kw);
    });
  }

  private selectConnection(id: string): void {
    if (!id) return;
    this.selectedConnId = id;
    const c = this.homeViewModel.bookmarks.find((it: ConnectionProfile) => it.id === id);
    if (c) {
      this.homeViewModel.applyBookmark(c);
    }
  }

  private getSelectedConnection(): ConnectionProfile | null {
    if (!this.selectedConnId) return null;
    return this.homeViewModel.bookmarks.find((it: ConnectionProfile) => it.id === this.selectedConnId) ?? null;
  }

  private formatLastUsed(ts: number | undefined): string {
    if (!ts) return '从未使用';
    const now = Date.now();
    const diff = now - ts;
    if (diff < 60000) return '刚刚';
    if (diff < 3600000) return `${Math.floor(diff / 60000)} 分钟前`;
    if (diff < 86400000) return `${Math.floor(diff / 3600000)} 小时前`;
    if (diff < 604800000) return `${Math.floor(diff / 86400000)} 天前`;
    const d = new Date(ts);
    return `${d.getMonth() + 1}/${d.getDate()}`;
  }

  build() {
    Stack() {
      if (this.showDetail) {
        this.buildDetailPane()
      } else {
        this.buildConnectionManager()
      }
      this.buildAboutOverlay()
      this.buildAgreementOverlay()
      this.buildDeleteConfirmDialog()
    }
    .width('100%')
    .height('100%')
  }

  @Builder
  private buildConnectionManager(): void {
    Column() {
      Row() {
        Column({ space: this.isPhoneLayout() ? 2 : 6 }) {
          Row({ space: this.isPhoneLayout() ? 8 : 12 }) {
            Column({ space: 0 }) {
              Text('碧虎')
                .fontSize(this.isPhoneLayout() ? 20 : 24)
                .fontWeight(FontWeight.Bold)
                .fontColor(this.colors.primary)
                .letterSpacing(1)
              Text('远程桌面')
                .fontSize(this.isPhoneLayout() ? 20 : 24)
                .fontWeight(FontWeight.Lighter)
                .fontColor(this.colors.foreground)
                .letterSpacing(this.isPhoneLayout() ? 1 : 2)
            }
          }
          if (!this.isPhoneLayout()) {
            Text('安全的远程访问')
              .fontSize(13)
              .fontColor(this.colors.mutedForeground)
          }
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)

        Row({ space: this.isPhoneLayout() ? 6 : 10 }) {
          ThemeToggle()

          if (!this.isPhoneLayout()) {
            Text(`v${BUILD_VERSION}`)
              .fontSize(12)
              .fontColor(this.colors.mutedForeground)
          }

          Button('关于')
            .height(this.isPhoneLayout() ? 30 : 32)
            .fontSize(this.isPhoneLayout() ? 12 : 13)
            .fontColor(this.colors.accentForeground)
            .backgroundColor(this.colors.accent)
            .borderRadius(10)
            .padding({ left: this.isPhoneLayout() ? 10 : 14, right: this.isPhoneLayout() ? 10 : 14 })
            .onClick(() => this.openAboutDialog())
        }
      }
      .alignItems(VerticalAlign.Top)
      .width('100%')
      .padding({
        left: this.horizontalPadding(),
        right: this.horizontalPadding(),
        top: this.isPhoneLayout() ? 12 : 16,
        bottom: this.isPhoneLayout() ? 8 : 12
      })

      if (this.activeSessions.length > 0) {
        Column({ space: this.isPhoneLayout() ? 8 : 10 }) {
          Row() {
            Text('活跃会话')
              .fontSize(this.isPhoneLayout() ? 15 : 16)
              .fontWeight(FontWeight.Medium)
              .fontColor(this.colors.foreground)
              .layoutWeight(1)
            Text(`${this.activeSessions.length} 个`)
              .fontSize(13)
              .fontColor(this.colors.mutedForeground)
          }
          .width('100%')

          ForEach(this.activeSessions, (record: SessionRecord) => {
            Row() {
              Column({ space: this.isPhoneLayout() ? 2 : 4 }) {
                Text(record.state.title)
                  .fontSize(this.isPhoneLayout() ? 14 : 15)
                  .fontWeight(FontWeight.Medium)
                  .fontColor(this.colors.foreground)
                  .maxLines(1)
                  .textOverflow({ overflow: TextOverflow.Ellipsis })
                Text(record.state.message)
                  .fontSize(this.isPhoneLayout() ? 12 : 13)
                  .fontColor(this.colors.mutedForeground)
                  .maxLines(1)
                  .textOverflow({ overflow: TextOverflow.Ellipsis })
              }
              .layoutWeight(1)
              .alignItems(HorizontalAlign.Start)

              Text(this.stageLabel(record.state.stage))
                .fontSize(11)
                .fontColor(this.stageColor(record.state.stage))
                .padding({ left: 8, right: 8, top: 4, bottom: 4 })
                .backgroundColor(this.colors.muted)
                .borderRadius(999)

              if (!this.isPhoneLayout()) {
                Row({ space: 8 }) {
                  Button('切换')
                    .type(ButtonType.Normal)
                    .fontSize(13)
                    .height(34)
                    .backgroundColor(this.colors.accent)
                    .fontColor(this.colors.accentForeground)
                    .borderRadius(10)
                    .onClick(() => this.focusSession(record.sessionId))
                  Button('断开')
                    .type(ButtonType.Normal)
                    .fontSize(13)
                    .height(34)
                    .backgroundColor('rgba(185,28,28,0.08)')
                    .fontColor(this.colors.destructive)
                    .borderRadius(10)
                    .onClick(() => this.disconnectSession(record.sessionId))
                }
              } else {
                Row({ space: 6 }) {
                  Button('切')
                    .type(ButtonType.Normal)
                    .fontSize(12)
                    .height(32)
                    .backgroundColor(this.colors.accent)
                    .fontColor(this.colors.accentForeground)
                    .borderRadius(8)
                    .padding({ left: 10, right: 10 })
                    .onClick(() => this.focusSession(record.sessionId))
                  Button('断')
                    .type(ButtonType.Normal)
                    .fontSize(12)
                    .height(32)
                    .backgroundColor('rgba(185,28,28,0.08)')
                    .fontColor(this.colors.destructive)
                    .borderRadius(8)
                    .padding({ left: 10, right: 10 })
                    .onClick(() => this.disconnectSession(record.sessionId))
                }
              }
            }
            .width('100%')
            .padding({ left: this.isPhoneLayout() ? 12 : 14, right: this.isPhoneLayout() ? 12 : 14, top: this.isPhoneLayout() ? 10 : 14, bottom: this.isPhoneLayout() ? 10 : 14 })
            .backgroundColor(this.colors.surfaceElevated)
            .border({ width: 1, color: this.colors.surfaceElevatedBorder })
            .borderRadius(this.isPhoneLayout() ? 12 : 14)
          }, (record: SessionRecord) => `${record.sessionId}`)
        }
        .width('100%')
        .padding({
          left: this.horizontalPadding(),
          right: this.horizontalPadding(),
          top: this.isPhoneLayout() ? 6 : 8,
          bottom: this.isPhoneLayout() ? 6 : 8
        })
      }

      Row() {
        TextInput({ text: this.keyword, placeholder: '搜索：名称 / 地址 / 账号' })
          .onChange((v: string) => this.keyword = v)
          .height(this.isPhoneLayout() ? 42 : 44)
          .fontSize(this.isPhoneLayout() ? 14 : 15)
          .backgroundColor(this.colors.inputBackground)
          .border({ width: 1, color: this.colors.inputBorder })
          .borderRadius(12)
          .padding({ left: 14, right: 14 })
          .layoutWeight(1)
        if ((this.keyword ?? '').trim()) {
          Button('清除')
            .height(this.isPhoneLayout() ? 34 : 36)
            .fontSize(13)
            .margin({ left: 8 })
            .fontColor(this.colors.primary)
            .backgroundColor(this.colors.muted)
            .borderRadius(10)
            .onClick(() => this.keyword = '')
        }
        Button() {
          Text('+')
            .fontSize(this.isPhoneLayout() ? 20 : 22)
            .fontWeight(FontWeight.Bold)
            .fontColor(this.colors.primaryForeground)
        }
        .type(ButtonType.Circle)
        .width(this.isPhoneLayout() ? 42 : 44)
        .height(this.isPhoneLayout() ? 42 : 44)
        .backgroundColor(this.colors.primary)
        .borderRadius(12)
        .margin({ left: 8 })
        .onClick(() => {
          this.homeViewModel.createNewProfile();
          this.selectedConnId = '';
          this.showDetail = true;
        })
      }
      .alignItems(VerticalAlign.Center)
      .padding({
        left: this.horizontalPadding(),
        right: this.horizontalPadding(),
        bottom: this.isPhoneLayout() ? 8 : 12
      })

      if (this.getFilteredBookmarks().length === 0) {
        Column() {
          Column({ space: this.isPhoneLayout() ? 16 : 20 }) {
            if (this.homeViewModel.bookmarks.length === 0) {
              Text('🖥')
                .fontSize(this.isPhoneLayout() ? 48 : 56)
                .margin({ bottom: this.isPhoneLayout() ? 4 : 8 })
            }
            Text(this.homeViewModel.bookmarks.length === 0 ? '暂无连接配置' : '未找到匹配的连接')
              .fontSize(this.isPhoneLayout() ? 16 : 17)
              .fontWeight(FontWeight.Medium)
              .fontColor(this.colors.foreground)
            Text(this.homeViewModel.bookmarks.length === 0 ? '点击下方按钮创建你的第一个远程桌面连接' : '尝试其他关键词搜索')
              .fontSize(this.isPhoneLayout() ? 13 : 14)
              .fontColor(this.colors.mutedForeground)
              .textAlign(TextAlign.Center)
              .maxLines(2)
              .textOverflow({ overflow: TextOverflow.Ellipsis })
              .constraintSize({ maxWidth: 260 })
              .margin({ top: this.isPhoneLayout() ? 2 : 4 })

            if (this.homeViewModel.bookmarks.length === 0) {
              Button() {
                Row({ space: 8 }) {
                  Text('+')
                    .fontSize(this.isPhoneLayout() ? 17 : 18)
                    .fontWeight(FontWeight.Bold)
                    .fontColor(this.colors.primaryForeground)
                  Text('新增连接')
                    .fontColor(this.colors.primaryForeground)
                    .fontSize(this.isPhoneLayout() ? 15 : 16)
                    .fontWeight(FontWeight.Medium)
                }
                .alignItems(VerticalAlign.Center)
              }
              .height(this.isPhoneLayout() ? 46 : 48)
              .backgroundColor(this.colors.primary)
              .borderRadius(14)
              .padding({ left: 22, right: 22 })
              .margin({ top: this.isPhoneLayout() ? 12 : 16 })
              .onClick(() => {
                this.homeViewModel.createNewProfile();
                this.selectedConnId = '';
                this.showDetail = true;
              })
            }
          }
          .justifyContent(FlexAlign.Center)
          .alignItems(HorizontalAlign.Center)
          .width('100%')
          .layoutWeight(1)
        }
        .padding({
          left: this.horizontalPadding(),
          right: this.horizontalPadding(),
          top: this.isPhoneLayout() ? 16 : 24,
          bottom: this.isPhoneLayout() ? 16 : 24
        })
        .margin({
          left: this.isPhoneLayout() ? 12 : 20,
          right: this.isPhoneLayout() ? 12 : 20,
          top: this.isPhoneLayout() ? 4 : 8,
          bottom: this.isPhoneLayout() ? 4 : 8
        })
        .backgroundColor(this.colors.surfaceElevated)
        .border({ width: 1, color: this.colors.surfaceElevatedBorder })
        .borderRadius(this.isPhoneLayout() ? 18 : 20)
        .layoutWeight(1)
        .clip(true)
      } else {
        List({ scroller: this.connectionListScroller }) {
          ForEach(this.getFilteredBookmarks(), (c: ConnectionProfile) => {
            ListItem() {
              Row() {
                Column() {
                  Row() {
                    Text(c.name || c.host)
                      .fontSize(this.isPhoneLayout() ? 15 : 16)
                      .fontWeight(c.id === this.selectedConnId ? FontWeight.Medium : FontWeight.Regular)
                      .fontColor(this.colors.foreground)
                      .layoutWeight(1)
                      .maxLines(1)
                      .textOverflow({ overflow: TextOverflow.Ellipsis })
                    if (c.securityMode && c.securityMode !== 'auto') {
                      Text(c.securityMode.toUpperCase())
                        .fontSize(11)
                        .fontWeight(FontWeight.Medium)
                        .fontColor(this.colors.primary)
                        .backgroundColor(this.colors.tagBackground)
                        .borderRadius(6)
                        .padding({ left: 7, right: 7, top: 2, bottom: 2 })
                        .margin({ left: 7 })
                    }
                  }
                  .width('100%')
                  .alignItems(VerticalAlign.Center)
                  Text(`${c.username ? c.username + '@' : ''}${c.host}:${c.port}`)
                    .fontSize(this.isPhoneLayout() ? 12 : 13)
                    .fontColor(this.colors.mutedForeground)
                    .maxLines(1)
                    .textOverflow({ overflow: TextOverflow.Ellipsis })
                    .margin({ top: 3 })
                  if (c.gateway) {
                    Text(`网关：${c.gateway}`)
                      .fontSize(11)
                      .fontColor(this.colors.mutedForeground)
                      .maxLines(1)
                      .textOverflow({ overflow: TextOverflow.Ellipsis })
                      .margin({ top: 2 })
                  }
                }
                .layoutWeight(1)
                .alignItems(HorizontalAlign.Start)

                Row({ space: this.isPhoneLayout() ? 4 : 8 }) {
                  Button() {
                    Text('→')
                      .fontSize(this.isPhoneLayout() ? 16 : 18)
                      .fontWeight(FontWeight.Bold)
                      .fontColor(this.colors.primaryForeground)
                  }
                  .type(ButtonType.Circle)
                  .width(this.isPhoneLayout() ? 36 : 40)
                  .height(this.isPhoneLayout() ? 36 : 40)
                  .backgroundColor(this.colors.primary)
                  .borderRadius(this.isPhoneLayout() ? 10 : 10)
                  .onClick(() => {
                    this.homeViewModel.applyBookmark(c);
                    void this.connectAndOpenWindow();
                  })

                  Button() {
                    Text('✎')
                      .fontSize(this.isPhoneLayout() ? 15 : 18)
                      .fontColor(this.colors.primary)
                  }
                  .type(ButtonType.Circle)
                  .width(this.isPhoneLayout() ? 36 : 40)
                  .height(this.isPhoneLayout() ? 36 : 40)
                  .backgroundColor(this.colors.muted)
                  .borderRadius(this.isPhoneLayout() ? 10 : 10)
                  .onClick(() => {
                    this.selectConnection(c.id);
                    this.showDetail = true;
                  })

                  Button() {
                    Text('✕')
                      .fontSize(this.isPhoneLayout() ? 15 : 18)
                      .fontColor(this.colors.destructive)
                  }
                  .type(ButtonType.Circle)
                  .width(this.isPhoneLayout() ? 36 : 40)
                  .height(this.isPhoneLayout() ? 36 : 40)
                  .backgroundColor('rgba(185,28,28,0.08)')
                  .borderRadius(this.isPhoneLayout() ? 10 : 10)
                  .onClick(() => {
                    this.selectedConnId = c.id;
                    this.showDeleteConfirm = true;
                  })
                }
              }
              .alignItems(VerticalAlign.Center)
              .padding({
                left: this.isPhoneLayout() ? 14 : 14,
                right: this.isPhoneLayout() ? 10 : 14,
                top: this.isPhoneLayout() ? 12 : 14,
                bottom: this.isPhoneLayout() ? 12 : 14
              })
              .backgroundColor(c.id === this.selectedConnId ? this.colors.tagBackground : this.colors.surfaceElevated)
              .border({ width: c.id === this.selectedConnId ? 2 : 1, color: c.id === this.selectedConnId ? this.colors.primary : this.colors.surfaceElevatedBorder })
              .borderRadius(this.isPhoneLayout() ? 14 : 14)
              .onClick(() => this.selectConnection(c.id))
            }
            .padding({
              left: this.horizontalPadding(),
              right: this.horizontalPadding(),
              top: this.isPhoneLayout() ? 4 : 6,
              bottom: this.isPhoneLayout() ? 4 : 6
            })
          }, (c: ConnectionProfile) => c.id)
        }
        .cachedCount(4)
        .edgeEffect(EdgeEffect.Spring)
        .layoutWeight(1)
      }
    }
    .width('100%')
    .height('100%')
    .backgroundColor(this.colors.background)
    .onAreaChange((_, newValue) => {
      this.pageWidth = Number(newValue.width);
    })
  }

  @Builder
  private buildDetailPane(): void {
    Column() {
      Row() {
        Button() {
          Text('←')
            .fontSize(this.isPhoneLayout() ? 18 : 20)
            .fontWeight(FontWeight.Bold)
            .fontColor(this.colors.foreground)
        }
        .type(ButtonType.Circle)
        .width(this.isPhoneLayout() ? 38 : 40)
        .height(this.isPhoneLayout() ? 38 : 40)
        .backgroundColor(this.colors.muted)
        .borderRadius(10)
        .onClick(() => { this.showDetail = false; })

        Text(this.selectedConnId ? '编辑连接' : '新建连接')
          .fontSize(this.isPhoneLayout() ? 17 : 18)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.colors.foreground)
          .layoutWeight(1)
          .margin({ left: this.isPhoneLayout() ? 12 : 0 })

        if (this.selectedConnId) {
          Button('删除')
            .height(this.isPhoneLayout() ? 34 : 36)
            .fontSize(this.isPhoneLayout() ? 13 : 14)
            .fontColor(this.colors.destructive)
            .backgroundColor('rgba(185,28,28,0.08)')
            .borderRadius(10)
            .padding({ left: this.isPhoneLayout() ? 12 : 14, right: this.isPhoneLayout() ? 12 : 14 })
            .onClick(() => {
              this.showDeleteConfirm = true;
            })
        }
      }
      .width('100%')
      .padding({
        left: this.horizontalPadding(),
        right: this.horizontalPadding(),
        top: this.isPhoneLayout() ? 12 : 16,
        bottom: this.isPhoneLayout() ? 8 : 12
      })
      .alignItems(VerticalAlign.Center)

      Scroll() {
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
            onNew: () => { this.homeViewModel.createNewProfile(); this.selectedConnId = ''; },
            onSave: () => { void this.homeViewModel.saveCurrentProfile(); },
            canConnect: this.homeViewModel.canConnect && !this.isConnecting,
            onConnect: () => { void this.connectAndOpenWindow(); }
          })

          Text(this.homeViewModel.lastMessage)
            .fontSize(13)
            .fontColor(this.colors.mutedForeground)
        }
        .padding({ left: 20, right: 20, bottom: 24 })
      }
      .layoutWeight(1)
      .scrollBar(BarState.Auto)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(this.colors.background)
  }

  @Builder
  private buildDeleteConfirmDialog(): void {
    if (this.showDeleteConfirm) {
      Stack() {
        Column()
          .width('100%')
          .height('100%')
          .backgroundColor(this.colors.overlayScrim)
          .onClick(() => { this.showDeleteConfirm = false; })

        Column({ space: 20 }) {
          Column({ space: 12 }) {
            Text('确认删除')
              .fontSize(18)
              .fontWeight(FontWeight.Medium)
              .fontColor(this.colors.foreground)

            Text('确定要删除此连接配置吗？此操作无法撤销。')
              .fontSize(14)
              .fontColor(this.colors.mutedForeground)
              .textAlign(TextAlign.Center)
          }

          Row({ space: 12 }) {
            Button('取消')
              .width(100)
              .height(40)
              .fontSize(15)
              .fontColor(this.colors.foreground)
              .backgroundColor(this.colors.muted)
              .borderRadius(10)
              .onClick(() => { this.showDeleteConfirm = false; })

            Button('删除')
              .width(100)
              .height(40)
              .fontSize(15)
              .fontColor(this.colors.primaryForeground)
              .backgroundColor(this.colors.destructive)
              .borderRadius(10)
              .onClick(() => {
                void this.homeViewModel.deleteBookmark(this.selectedConnId);
                this.selectedConnId = '';
                this.showDeleteConfirm = false;
                this.showDetail = false;
              })
          }
        }
        .padding(24)
        .backgroundColor(this.colors.card)
        .borderRadius(20)
        .width(280)
      }
      .width('100%')
      .height('100%')
    }
  }

  @Builder
  private buildAboutOverlay(): void {
    if (this.aboutOpen) {
      Stack() {
        Column()
          .width('100%')
          .height('100%')
          .backgroundColor(this.colors.overlayScrim)
          .onClick(() => this.closeAboutDialog());

        Column() {
          Column() {
            Row() {
              Column({ space: 4 }) {
                Row({ space: 10 }) {
                  Text('碧虎')
                    .fontSize(24)
                    .fontWeight(FontWeight.Bold)
                    .fontColor(this.colors.primaryForeground)
                    .letterSpacing(1)
                  Text('远程桌面')
                    .fontSize(24)
                    .fontWeight(FontWeight.Lighter)
                    .fontColor(this.colors.primaryForeground)
                }
                Text(`v${BUILD_VERSION}`)
                  .fontSize(12)
                  .fontColor('rgba(255,255,255,0.70)')
              }
              .alignItems(HorizontalAlign.Start)

              Button() {
                Text('×')
                    .fontSize(22)
                    .fontWeight(FontWeight.Lighter)
                  .fontColor('rgba(255,255,255,0.65)')
              }
              .width(36)
              .height(36)
              .backgroundColor('rgba(255,255,255,0.10)')
              .borderRadius(18)
              .onClick(() => this.closeAboutDialog())
            }
            .width('100%')
            .alignItems(VerticalAlign.Center)
            .padding({ left: 24, right: 20, top: 28, bottom: 24 })
          }
          .width('100%')
          .linearGradient({
            angle: 135,
            colors: [[this.colors.primary, 0], ['#1a3d40', 0.5], ['#0f2628', 1]]
          })
          .borderRadius({ topLeft: 24, topRight: 24 })

          Scroll() {
            Column({ space: 16 }) {
              Column({ space: 14 }) {
                Text('公司信息')
                  .fontSize(14)
                  .fontWeight(FontWeight.Medium)
                  .fontColor(this.colors.mutedForeground)
                  .letterSpacing(0.5);

                Column({ space: 12 }) {
                  this.buildAboutInfoRow('开发团队', '横渠天地（北京）科技有限公司');
                  Blank().height(2);
                  this.buildAboutLinkRow('商务合作', PROJECT_CONTACT_EMAIL, `mailto:${PROJECT_CONTACT_EMAIL}`);
                  Blank().height(2);
                  this.buildAboutLinkRow('官方网站', PROJECT_HOMEPAGE_URL, PROJECT_HOMEPAGE_URL);
                }
              }
              .width('100%')
              .padding(18)
              .backgroundColor(this.colors.surfaceElevated)
              .border({ width: 1, color: this.colors.surfaceElevatedBorder })
              .borderRadius(18);

              Column({ space: 14 }) {
                Text('协议与隐私')
                  .fontSize(14)
                  .fontWeight(FontWeight.Medium)
                  .fontColor(this.colors.mutedForeground)
                  .letterSpacing(0.5);

                Column({ space: 10 }) {
                  this.buildAboutTapRow('隐私协议', '查看完整内容', () => this.openAgreementDialog(AGREEMENT_DIALOG_PRIVACY));
                  Blank().height(1).color(this.colors.cardBorder).width('100%');
                  this.buildAboutTapRow('用户协议', '查看完整内容', () => this.openAgreementDialog(AGREEMENT_DIALOG_USER));
                }
              }
              .width('100%')
              .padding(18)
              .backgroundColor(this.colors.surfaceElevated)
              .border({ width: 1, color: this.colors.surfaceElevatedBorder })
              .borderRadius(18);

              Column({ space: 14 }) {
                Text('开源许可')
                  .fontSize(14)
                  .fontWeight(FontWeight.Medium)
                  .fontColor(this.colors.mutedForeground)
                  .letterSpacing(0.5);

                Column({ space: 12 }) {
                  this.buildAboutInfoRowMulti('许可声明', '本应用采用 Apache-2.0 许可发布，基于 FreeRDP 开源项目。');
                  Blank().height(2);
                  this.buildAboutLinkRow('源码仓库', PROJECT_SOURCE_URL, PROJECT_SOURCE_URL);
                  Blank().height(2);
                  this.buildAboutInfoRowMulti('第三方组件', 'FreeRDP (Apache-2.0) · OpenSSL (Apache-2.0)');
                  Blank().height(2);
                  this.buildAboutInfoRowMulti('免责声明', '本软件按"现状"提供，不附带任何明示或暗示担保。');
                }
              }
              .width('100%')
              .padding(18)
              .backgroundColor(this.colors.surfaceElevated)
              .border({ width: 1, color: this.colors.surfaceElevatedBorder })
              .borderRadius(18);
            }
            .padding({ left: 24, right: 24, top: 24, bottom: 28 })
          }
          .layoutWeight(1)
          .scrollBar(BarState.Auto)
          .align(Alignment.Top)
        }
        .width('100%')
        .height('84%')
        .constraintSize({ maxWidth: 480 })
        .backgroundColor(this.colors.background)
        .borderRadius(24)
        .shadow({ radius: 40, color: this.colors.shadowXl, offsetX: 0, offsetY: 12 })
      }
      .alignContent(Alignment.Center)
      .width('100%')
      .height('100%');
    }
  }

  @Builder
  private buildAboutInfoRow(label: string, value: string): void {
    Row() {
      Text(label)
        .fontSize(13)
        .fontColor(this.colors.mutedForeground)
        .width(72)
        .constraintSize({ minWidth: 72 });
      Text(value)
        .fontSize(14)
        .lineHeight(21)
        .fontWeight(FontWeight.Medium)
        .fontColor(this.colors.foreground)
        .layoutWeight(1);
    }
    .alignItems(VerticalAlign.Center)
    .width('100%');
  }

  @Builder
  private buildAboutInfoRowMulti(label: string, value: string): void {
    Column({ space: 2 }) {
      Text(label)
        .fontSize(12)
        .fontColor(this.colors.mutedForeground);
      Text(value)
        .fontSize(13)
        .lineHeight(20)
        .fontColor(this.colors.foreground);
    }
    .alignItems(HorizontalAlign.Start)
    .width('100%');
  }

  @Builder
  private buildAboutLinkRow(label: string, value: string, uri: string): void {
    Row() {
      Text(label)
        .fontSize(13)
        .fontColor(this.colors.mutedForeground)
        .width(72)
        .constraintSize({ minWidth: 72 });
      Column({ space: 4 }) {
        Text(value)
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.colors.primary)
          .maxLines(1)
          .textOverflow({ overflow: TextOverflow.Ellipsis })
          .onClick(() => this.openExternalUrl(uri));
        Row({ space: 12 }) {
          Text('访问链接')
            .fontSize(11)
            .fontColor(this.colors.primary)
            .onClick(() => this.openExternalUrl(uri));
          Text('复制文本')
            .fontSize(11)
            .fontColor(this.colors.mutedForeground)
            .onClick(() => this.copyText(value, '已复制'));
        }
      }
      .layoutWeight(1)
      .alignItems(HorizontalAlign.Start);
    }
    .alignItems(VerticalAlign.Top)
    .width('100%');
  }

  @Builder
  private buildAboutTapRow(label: string, subLabel: string, onTap: () => void): void {
    Row() {
      Column({ space: 2 }) {
        Text(label)
          .fontSize(14)
          .fontWeight(FontWeight.Medium)
          .fontColor(this.colors.foreground);
        Text(subLabel)
          .fontSize(12)
          .fontColor(this.colors.mutedForeground);
      }
      .alignItems(HorizontalAlign.Start)
      .layoutWeight(1);

      Text('›')
        .fontSize(20)
        .fontWeight(FontWeight.Lighter)
        .fontColor(this.colors.mutedForeground)
    }
    .width('100%')
    .alignItems(VerticalAlign.Center)
    .padding({ top: 4, bottom: 4 })
    .onClick(onTap);
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
                Text('×')
                  .fontSize(22)
                  .fontWeight(FontWeight.Lighter)
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
