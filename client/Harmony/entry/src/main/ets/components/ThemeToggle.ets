import { ThemeMode, LightColors, DarkColors, getStoredThemeMode } from '../utils/ThemeConstants';

@Component
export struct ThemeToggle {
  @StorageLink('__app_is_dark_mode__') isDark: boolean = false;

  aboutToAppear(): void {
    const stored: ThemeMode = getStoredThemeMode();
    this.isDark = stored === ThemeMode.Dark;
  }

  private toggleTheme(): void {
    this.isDark = !this.isDark;
  }

  build() {
    Button() {
      Text(this.isDark ? '☀' : '◐')
        .fontSize(16)
        .fontColor(this.isDark ? '#fbbf24' : '#2c5f63')
    }
    .type(ButtonType.Circle)
    .width(36)
    .height(36)
    .backgroundColor(this.isDark ? 'rgba(95,184,184,0.12)' : 'rgba(44,95,99,0.08)')
    .onClick(() => this.toggleTheme())
  }
}
