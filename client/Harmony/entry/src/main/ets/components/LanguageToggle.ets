import { i18n } from '../utils/I18n';
import { LanguagePicker } from './LanguagePicker';

@Component
export struct LanguageToggle {
  @StorageLink('__app_language__') appLanguage: string = 'system';
  @StorageLink('__app_is_dark_mode__') isDark: boolean = false;

  private dialogController: CustomDialogController = new CustomDialogController({
    builder: LanguagePicker({
      onSelect: (code: string) => {
        i18n.setLanguage(code);
      }
    }),
    alignment: DialogAlignment.Center,
    autoCancel: true,
    customStyle: true
  });

  build() {
    Button(this.displayLabel())
      .type(ButtonType.Circle)
      .width(36)
      .height(36)
      .fontSize(11)
      .fontWeight(FontWeight.Bold)
      .fontColor(this.isDark ? '#5fb8b8' : '#2c5f63')
      .backgroundColor(this.isDark ? 'rgba(95,184,184,0.12)' : 'rgba(44,95,99,0.08)')
      .onClick(() => {
        this.dialogController.open();
      })
  }

  private displayLabel(): string {
    if (this.appLanguage === 'system') {
      return '🌐';
    }
    if (this.appLanguage === 'zh-CN') {
      return '中';
    }
    return 'EN';
  }
}
