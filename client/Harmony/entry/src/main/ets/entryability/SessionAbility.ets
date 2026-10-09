import UIAbility from '@ohos.app.ability.UIAbility';
import AbilityConstant from '@ohos.app.ability.AbilityConstant';
import Want from '@ohos.app.ability.Want';
import window from '@ohos.window';
import { SessionRecord, sessionManager } from '../service/SessionManager';

interface WantLike {
  parameters?: Record<string, Object>;
}

interface UIAbilityContextWithWant {
  getWant?: () => WantLike;
  want?: WantLike;
  setMissionLabel?: (label: string) => Promise<void>;
}

interface WindowLike {
  setWindowTitle?: (t: string) => void;
  setWindowDecorVisible?: (visible: boolean) => void;
}

interface WindowStageLike {
  getMainWindowSync?: () => Object;
  getMainWindow?: () => Promise<Object>;
}

export interface SessionLaunchParams {
  sessionId: number;
  title: string;
}

export const DEFAULT_SESSION_LAUNCH_PARAMS: SessionLaunchParams = {
  sessionId: 0,
  title: '远程桌面'
};

export default class SessionAbility extends UIAbility {
  private sessionId: number = 0;
  private sessionTitle: string = '远程桌面';

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    try {
      const params = want?.parameters ?? {};
      this.sessionId = Number(
        (params['sessionId'] !== undefined && params['sessionId'] !== null)
          ? String(params['sessionId']) : '0'
      );
      this.sessionTitle =
        (params['title'] !== undefined && params['title'] !== null)
          ? String(params['title']) : '远程桌面';

      const ctx = this.context as UIAbilityContextWithWant;
      ctx.setMissionLabel?.(this.sessionTitle);

      AppStorage.setOrCreate<SessionLaunchParams>('__session_launch_bridge__', {
        sessionId: this.sessionId,
        title: this.sessionTitle
      });
    } catch (e) {}
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    try {
      windowStage.loadContent('pages/Session');

      const ws = windowStage as WindowStageLike;
      const mainWin = ws.getMainWindowSync?.();

      const apply = (win: Object): void => {
        try {
          const w = win as WindowLike;
          w.setWindowTitle?.(this.sessionTitle);
          w.setWindowDecorVisible?.(true);
        } catch (e) {}
      };

      if (mainWin) {
        apply(mainWin);
      } else {
        ws.getMainWindow?.()
          .then((win: Object): void => apply(win))
          .catch((): void => {});
      }
    } catch (e) {}
  }

  onDestroy(): void {
    try {
      if (this.sessionId > 0) {
        const record: SessionRecord | undefined = sessionManager.getSession(this.sessionId);
        if (record !== undefined) {
          void sessionManager.disconnect(this.sessionId);
          sessionManager.removeSession(this.sessionId);
        }
      }
    } catch (e) {}
  }
}
