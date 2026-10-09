import { common } from '@kit.AbilityKit';
import { SessionRecord, sessionManager } from './SessionManager';
import { SessionStage } from '../model/SessionModels';
import { BusinessError } from '@kit.BasicServicesKit';
import { ConnectionProfile } from '../model/ConnectionProfile';

type WantParamValue = string | number | boolean | undefined;

type WindowClosedCallback = (sessionId: number) => void;

interface SessionAbilityParams extends Record<string, WantParamValue> {
  sessionId: number;
  title: string;
}

interface SessionStartWant {
  deviceId: string;
  bundleName: string;
  moduleName: string;
  abilityName: string;
  parameters: SessionAbilityParams;
}

function buildSessionTitle(profile: ConnectionProfile): string {
  const name: string = profile.name.trim();
  const host: string = profile.host;
  const port: number = profile.port;
  const user: string = profile.username;
  const displayHost: string = port !== 3389 ? `${host}:${port}` : host;
  const addr: string = user.length > 0 ? `${user}@${displayHost}` : displayHost;
  if (name.length > 0 && addr.length > 0) {
    return `${name}  ${addr}`;
  }
  if (name.length > 0) {
    return name;
  }
  return addr.length > 0 ? addr : '远程桌面';
}

class SessionWindowManagerImpl {
  private context: common.UIAbilityContext | null = null;
  private closedListeners: Set<WindowClosedCallback> = new Set();

  onWindowClosed(callback: WindowClosedCallback): () => void {
    this.closedListeners.add(callback);
    return () => {
      this.closedListeners.delete(callback);
    };
  }

  private notifyWindowClosed(sessionId: number): void {
    this.closedListeners.forEach((callback: WindowClosedCallback) => {
      callback(sessionId);
    });
  }

  initialize(context: common.UIAbilityContext): void {
    this.context = context;
  }

  async openSessionWindow(sessionId: number, profile?: ConnectionProfile): Promise<boolean> {
    if (this.context === null) {
      console.error('SessionWindowManager not initialized');
      return false;
    }

    const title: string = profile !== undefined ? buildSessionTitle(profile) : '远程桌面';

    try {
      const params: SessionAbilityParams = {
        sessionId: sessionId,
        title: title
      };
      const want: SessionStartWant = {
        deviceId: '',
        bundleName: this.context.abilityInfo.bundleName,
        moduleName: this.context.abilityInfo.moduleName,
        abilityName: 'SessionAbility',
        parameters: params
      };

      await this.context.startAbility(want as object);
      return true;
    } catch (e) {
      const err = e as BusinessError;
      console.error(`SessionWindowManager openSessionWindow failed: code=${err.code}, msg=${err.message}`);
      return false;
    }
  }

  async closeSessionWindow(sessionId: number): Promise<void> {
    const session: SessionRecord | undefined = sessionManager.getSession(sessionId);
    if (session !== undefined && (session.state.stage === SessionStage.Connected ||
        session.state.stage === SessionStage.Connecting)) {
      await sessionManager.disconnect(sessionId);
    }
    sessionManager.removeSession(sessionId);
    this.notifyWindowClosed(sessionId);
  }

  async closeAllWindows(): Promise<void> {
    const sessions: SessionRecord[] = sessionManager.getActiveSessions();
    for (const record of sessions) {
      await this.closeSessionWindow(record.sessionId);
    }
  }
}

export const sessionWindowManager: SessionWindowManagerImpl = new SessionWindowManagerImpl();
