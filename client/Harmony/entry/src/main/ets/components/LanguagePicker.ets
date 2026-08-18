import { i18n, LanguageMeta } from '../utils/I18n';

@CustomDialog
export struct LanguagePicker {
  controller: CustomDialogController;
  @StorageLink('__app_language__') appLanguage: string = 'system';
  onSelect: (code: string) => void = (_code: string): void => {};

  build() {
    Column() {
      Text(i18n.get(this.appLanguage, 'select_language'))
        .fontSize(20)
        .fontWeight(FontWeight.Bold)
        .fontColor('#1c1917')
        .margin({ bottom: 20 })
        .width('100%')

      Column({ space: 0 }) {
        this.buildLanguageRow('system', i18n.get(this.appLanguage, 'follow_system'), true)
        Blank().height(1).width('100%').backgroundColor('#e8e6e1')
        ForEach(i18n.supportedLanguages, (meta: LanguageMeta, index: number) => {
          Column() {
            this.buildLanguageRow(meta.code, i18n.getLabel(meta.code), false)
            if (index < i18n.supportedLanguages.length - 1) {
              Blank()
                .height(1)
                .width('100%')
                .backgroundColor('#e8e6e1')
            }
          }
        }, (meta: LanguageMeta) => meta.code)
      }
      .padding({ left: 8, right: 8 })
      .backgroundColor('#f3f1ed')
      .borderRadius(14)

      Blank().height(16)

      Button(i18n.get(this.appLanguage, 'cancel'))
        .width('100%')
        .height(48)
        .fontSize(16)
        .fontWeight(FontWeight.Medium)
        .fontColor('#1c1917')
        .backgroundColor('#f3f1ed')
        .borderRadius(14)
        .onClick(() => this.controller.close())
    }
    .padding(24)
    .width('88%')
    .backgroundColor('#ffffff')
    .border({ width: 1, color: '#e8e6e1' })
    .borderRadius(20)
    .shadow({ radius: 24, color: 'rgba(28,25,23,0.10)', offsetX: 0, offsetY: 8 })
  }

  @Builder
  private buildLanguageRow(code: string, label: string, isSystem: boolean): void {
    Row() {
      Row({ space: 12 }) {
        Text(isSystem ? '🌐' : this.getNativeLabel(code))
          .fontSize(18)
          .fontColor('#1c1917')
        Text(label)
          .fontSize(14)
          .fontColor('#6b6560')
      }

      Blank()

      if (this.appLanguage === code) {
        Text('✓')
          .fontSize(18)
          .fontWeight(FontWeight.Bold)
          .fontColor('#2c5f63')
      }
    }
    .width('100%')
    .padding({ left: 4, right: 8, top: 16, bottom: 16 })
    .alignItems(VerticalAlign.Center)
    .onClick(() => {
      this.onSelect(code);
      this.controller.close();
    })
  }

  private getNativeLabel(code: string): string {
    if (code === 'zh-CN') {
      return '中';
    }
    if (code === 'en-US') {
      return 'EN';
    }
    return code;
  }
}
