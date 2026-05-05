import { ThemeMode, LightColors, DarkColors, getStoredThemeMode, persistThemeMode } from '../utils/ThemeConstants';

@Component
export struct ThemeToggle {
  @StorageLink('__app_is_dark_mode__') isDark: boolean = false;

  aboutToAppear(): void {
    const stored: ThemeMode = getStoredThemeMode();
    this.isDark = stored === ThemeMode.Dark;
  }

  private toggleTheme(): void {
    this.isDark = !this.isDark;
    persistThemeMode(this.isDark ? ThemeMode.Dark : ThemeMode.Light);
  }

  build() {
    Button() {
      Text(this.isDark ? '☀' : '🌙')
        .fontSize(18)
        .fontColor(this.isDark ? '#fbbf24' : '#2c4f54')
    }
    .type(ButtonType.Circle)
    .width(36)
    .height(36)
    .backgroundColor(this.isDark ? 'rgba(109,158,166,0.12)' : 'rgba(44,79,84,0.08)')
    .onClick(() => this.toggleTheme())
  }
}
