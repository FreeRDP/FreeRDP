import pasteboard from '@ohos.pasteboard';
import window from '@ohos.window';
import common from '@ohos.app.ability.common';
import abilityAccessCtrl, { Permissions } from '@ohos.abilityAccessCtrl';
import { SessionWindow } from '../components/SessionWindow';
import { sessionManager, SessionRecord } from '../service/SessionManager';
import { sessionWindowManager } from '../service/SessionWindowManager';
import { SessionEvent, SessionStage, SessionSnapshot } from '../model/SessionModels';
import { AuthDialog } from '../components/AuthDialog';
import { CertDialog } from '../components/CertDialog';
import { SessionLaunchParams, DEFAULT_SESSION_LAUNCH_PARAMS } from '../entryability/SessionAbility';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

interface WindowLike {
  setWindowTitle?: (t: string) => void;
  setWindowDecorVisible?: (visible: boolean) => void;
}

@Entry
@Component
struct Session {
  @State sessionId: number = 0;
  @State sessionTitle: string = '';
  @State sessionHost: string = '';
  @State sessionSubtitle: string = '';
  @State sessionStage: SessionStage = SessionStage.Idle;
  @State sessionSnapshot: SessionSnapshot = {
    width: 0,
    height: 0,
    frameStride: 0,
    frameSequence: 0,
    dirtyLeft: 0,
    dirtyTop: 0,
    dirtyWidth: 0,
    dirtyHeight: 0
  };
  @State authEvent: SessionEvent | null = null;
  @State certEvent: SessionEvent | null = null;
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @State colors: ThemeColors = LightColors;

  private systemPasteboard: pasteboard.SystemPasteboard = pasteboard.getSystemPasteboard();
  private applyingRemoteClipboard: boolean = false;
  private lastClipboardText: string = '';
  private pasteboardUpdateCallback: () => void = () => {};
  private refreshTimer: number = -1;
  private unsubSessionChanged: (() => void) | null = null;
  private hasReadPasteboardPermission: boolean = false;

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  authDialogController: CustomDialogController = new CustomDialogController({
    builder: AuthDialog({
      event: this.authEvent,
      username: '',
      password: '',
      domain: '',
      onConfirm: (username: string, pass: string, domain: string) => {
        if (this.sessionId > 0) {
          void sessionManager.submitAuth(this.sessionId, username, pass, domain);
        }
      }
    }),
    alignment: DialogAlignment.Center,
    autoCancel: false,
    customStyle: true
  });

  certDialogController: CustomDialogController = new CustomDialogController({
    builder: CertDialog({
      event: this.certEvent,
      onConfirm: (accept: boolean) => {
        if (this.sessionId > 0) {
          void sessionManager.submitCert(this.sessionId, accept);
        }
      }
    }),
    alignment: DialogAlignment.Center,
    autoCancel: false,
    customStyle: true
  });

  private readActiveSessionId(): number {
    const bridge: SessionLaunchParams = AppStorage.get<SessionLaunchParams>('__session_launch_bridge__')
      ?? DEFAULT_SESSION_LAUNCH_PARAMS;
    return bridge.sessionId > 0 ? bridge.sessionId : (AppStorage.get<number>('activeSessionId') ?? 0);
  }

  private getSessionWindowTitle(): string {
    const bridge: SessionLaunchParams = AppStorage.get<SessionLaunchParams>('__session_launch_bridge__')
      ?? DEFAULT_SESSION_LAUNCH_PARAMS;
    if (bridge.title.length > 0 && bridge.title !== '远程桌面') {
      return bridge.title;
    }
    if (this.sessionTitle.length > 0) {
      return this.sessionTitle;
    }
    return '远程桌面';
  }

  private updateWindowTitle(): void {
    try {
      const title: string = this.getSessionWindowTitle();
      const ctx = this.getUIContext()?.getHostContext?.() as common.UIAbilityContext | undefined;
      if (!ctx) {
        setTimeout(() => this.updateWindowTitle(), 300);
        return;
      }
      window.getLastWindow(ctx)
        .then((win: Object) => {
          try {
            (win as WindowLike)?.setWindowTitle?.(title);
            (win as WindowLike)?.setWindowDecorVisible?.(true);
          } catch (e) {}
        })
        .catch(() => {
          setTimeout(() => this.updateWindowTitle(), 500);
        });
    } catch (e) {}
  }

  private syncFromManager(): void {
    if (this.sessionId <= 0) {
      return;
    }
    const record: SessionRecord | undefined = sessionManager.getSession(this.sessionId);
    if (record === undefined) {
      return;
    }
    this.sessionTitle = record.state.title;
    this.sessionHost = record.profile.username.length > 0
      ? `${record.profile.username}@${record.profile.host}:${record.profile.port}`
      : `${record.profile.host}:${record.profile.port}`;
    this.sessionSubtitle = record.state.message;
    this.sessionStage = record.state.stage;
    this.sessionSnapshot = record.snapshot;
  }

  private applyRemoteClipboard(text: string): void {
    if (text.length === 0 || text === this.lastClipboardText) {
      return;
    }
    this.applyingRemoteClipboard = true;
    this.lastClipboardText = text;
    this.systemPasteboard.setData(pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, text)).then(() => {
      this.applyingRemoteClipboard = false;
    }).catch(() => {
      this.applyingRemoteClipboard = false;
    });
  }

  private onLocalClipboardChanged(): void {
    if (!this.hasReadPasteboardPermission) {
      return;
    }
    if (this.applyingRemoteClipboard) {
      return;
    }
    if (this.sessionId <= 0 || this.sessionStage !== SessionStage.Connected) {
      return;
    }
    this.systemPasteboard.getData().then((data: pasteboard.PasteData) => {
      if (this.applyingRemoteClipboard) {
        return;
      }
      const text: string = data.getPrimaryText();
      if (text.length === 0 || text === this.lastClipboardText) {
        return;
      }
      this.lastClipboardText = text;
      void sessionManager.setClipboardText(this.sessionId, text);
    }).catch(() => {});
  }

  private async requestReadPasteboardPermission(): Promise<void> {
    try {
      const context = this.getUIContext()?.getHostContext?.() as common.UIAbilityContext | undefined;
      if (context === undefined) {
        return;
      }
      const atManager = abilityAccessCtrl.createAtManager();
      const permission: Permissions = 'ohos.permission.READ_PASTEBOARD';
      const grantStatus = await atManager.checkAccessToken(context.applicationInfo.accessTokenId, permission);
      if (grantStatus === abilityAccessCtrl.GrantStatus.PERMISSION_GRANTED) {
        this.hasReadPasteboardPermission = true;
        return;
      }
      const requestResult = await atManager.requestPermissionsFromUser(context, [permission]);
      if (requestResult.authResults.length > 0 && requestResult.authResults[0] === 0) {
        this.hasReadPasteboardPermission = true;
      }
    } catch (_e) {
      this.hasReadPasteboardPermission = false;
    }
  }

  aboutToAppear(): void {
    this.sessionId = this.readActiveSessionId();
    if (this.sessionId <= 0) {
      this.sessionStage = SessionStage.Error;
      this.sessionSubtitle = '未找到会话标识';
      return;
    }

    this.syncFromManager();
    this.updateWindowTitle();

    this.unsubSessionChanged = sessionManager.onChanged(() => {
      this.syncFromManager();
    });

    this.refreshTimer = setInterval(() => {
      if (this.sessionId > 0) {
        const remoteClipboardText: string = sessionManager.consumeRemoteClipboardText(this.sessionId);
        if (remoteClipboardText.length > 0) {
          this.applyRemoteClipboard(remoteClipboardText);
        }
      }
    }, 120);

    this.requestReadPasteboardPermission().then(() => {
      if (this.hasReadPasteboardPermission) {
        this.pasteboardUpdateCallback = () => {
          this.onLocalClipboardChanged();
        };
        this.systemPasteboard.on('update', this.pasteboardUpdateCallback);
      }
    });
  }

  onPageShow(): void {
    this.updateWindowTitle();
    this.onLocalClipboardChanged();
  }

  aboutToDisappear(): void {
    this.systemPasteboard.off('update', this.pasteboardUpdateCallback);
    if (this.unsubSessionChanged !== null) {
      this.unsubSessionChanged();
      this.unsubSessionChanged = null;
    }
    if (this.refreshTimer >= 0) {
      clearInterval(this.refreshTimer);
      this.refreshTimer = -1;
    }
  }

  build() {
    Column() {
      SessionWindow({
        sessionId: this.sessionId,
        title: this.sessionTitle,
        subtitle: this.sessionHost,
        stage: this.sessionStage,
        snapshot: this.sessionSnapshot,
        isDark: this.isDark,
        getFrameBuffer: (): ArrayBuffer => {
          if (this.sessionId <= 0) {
            return new ArrayBuffer(0);
          }
          return sessionManager.getFrameBuffer(this.sessionId);
        },
        setSurfaceId: async (surfaceId: string) => {
          await sessionManager.setSurfaceId(this.sessionId, surfaceId);
        },
        onMouseMove: async (x: number, y: number) => {
          await sessionManager.sendMouseMove(this.sessionId, x, y);
        },
        onMouseButton: async (x: number, y: number, buttonFlags: number, down: boolean) => {
          await sessionManager.sendMouseButton(this.sessionId, x, y, buttonFlags, down);
        },
        onMouseWheel: async (x: number, y: number, delta: number) => {
          await sessionManager.sendMouseWheel(this.sessionId, x, y, delta);
        },
        onKeyScancode: async (scancode: number, down: boolean) => {
          await sessionManager.sendKeyScancode(this.sessionId, scancode, down);
        },
        onUnicodeKey: async (codepoint: number, down: boolean) => {
          await sessionManager.sendUnicodeKey(this.sessionId, codepoint, down);
        },
        onSendCtrlAltDel: async () => {
          await sessionManager.sendCtrlAltDel(this.sessionId);
        },
        onSendWindowsKey: async () => {
          await sessionManager.sendWindowsKey(this.sessionId);
        },
        onSendEscape: async () => {
          await sessionManager.sendEscape(this.sessionId);
        },
        onDisconnect: async () => {
          await sessionWindowManager.closeSessionWindow(this.sessionId);
        }
      })
    }
    .width('100%')
    .height('100%')
    .backgroundColor(this.colors.background)
    .animation({ duration: 300, curve: Curve.EaseIn })
  }
}
