import { ThemeMode, LightColors, DarkColors } from '../utils/ThemeConstants';

@Component
export struct ThemeToggle {
  @StorageLink('__app_is_dark_mode__') isDark: boolean = false;

  private toggleTheme(): void {
    this.isDark = !this.isDark;
    AppStorage.setOrCreate('__app_theme_mode__', this.isDark ? 'dark' : 'light');
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
