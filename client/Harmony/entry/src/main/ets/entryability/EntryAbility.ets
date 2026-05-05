import { AbilityConstant, UIAbility, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { sessionWindowManager } from '../service/SessionWindowManager';
import { ThemeMode, getStoredThemeMode } from '../utils/ThemeConstants';

export default class EntryAbility extends UIAbility {
  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    const storedTheme: ThemeMode = getStoredThemeMode();
    AppStorage.setOrCreate('__app_is_dark_mode__', storedTheme === ThemeMode.Dark);
    AppStorage.setOrCreate('appName', '碧虎远程桌面');
    AppStorage.setOrCreate('preferredInputMode', 'touchpad');
    AppStorage.setOrCreate('showBookmarksFirst', true);
    AppStorage.setOrCreate('bookmarkPayload', '');
    AppStorage.setOrCreate('draftProfilePayload', '');
    AppStorage.setOrCreate<number>('activeSessionId', 0);
    PersistentStorage.persistProp('bookmarkPayload', '');
    PersistentStorage.persistProp('draftProfilePayload', '');

    sessionWindowManager.initialize(this.context);
  }

  onWindowStageCreate(windowStage: window.WindowStage): void {
    windowStage.loadContent('pages/Index', (error: Error | undefined) => {
      const serializedError: string | undefined = JSON.stringify(error);
      if (error !== undefined && serializedError !== undefined &&
          serializedError.indexOf('"code":0') < 0) {
        console.error(`loadContent failed: ${serializedError}`);
      }
    });
  }

  onDestroy(): void {
    void sessionWindowManager.closeAllWindows();
  }
}
