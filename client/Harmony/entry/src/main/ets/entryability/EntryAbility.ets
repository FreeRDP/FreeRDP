import { AbilityConstant, UIAbility, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import { sessionWindowManager } from '../service/SessionWindowManager';
import { ThemeMode, getStoredThemeMode } from '../utils/ThemeConstants';
import { i18n } from '../utils/I18n';

interface WindowLike {
  setWindowTitle?: (t: string) => void;
}

interface WindowStageLike {
  getMainWindowSync?: () => Object;
  getMainWindow?: () => Promise<Object>;
}

export default class EntryAbility extends UIAbility {
  private mainWindow: Object | null = null;

  onCreate(want: Want, launchParam: AbilityConstant.LaunchParam): void {
    const storedTheme: ThemeMode = getStoredThemeMode();
    AppStorage.setOrCreate('__app_is_dark_mode__', storedTheme === ThemeMode.Dark);

    AppStorage.setOrCreate('__app_context__', this.context);
    i18n.init(this.context);
    AppStorage.setOrCreate('appName', i18n.get(i18n.currentLocale(), 'app_title'));
    AppStorage.setOrCreate('preferredInputMode', 'touchpad');
    AppStorage.setOrCreate('showBookmarksFirst', true);
    AppStorage.setOrCreate('bookmarkPayload', '');
    AppStorage.setOrCreate('draftProfilePayload', '');
    AppStorage.setOrCreate<number>('activeSessionId', 0);
    PersistentStorage.persistProp('bookmarkPayload', '');
    PersistentStorage.persistProp('draftProfilePayload', '');

    i18n.onLanguageChanged((_code: string) => {
      this.applyWindowTitle();
    });

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

    try {
      const ws = windowStage as WindowStageLike;
      const mainWin = ws.getMainWindowSync?.();
      if (mainWin) {
        this.mainWindow = mainWin;
        this.applyWindowTitle();
      }
    } catch (_e) {
    }
  }

  private applyWindowTitle(): void {
    try {
      const title = AppStorage.get<string>('appName') ?? '碧虎远程桌面';
      const w = this.mainWindow as WindowLike;
      w?.setWindowTitle?.(title);
    } catch (_e) {
    }
  }

  onDestroy(): void {
    void sessionWindowManager.closeAllWindows();
  }
}
