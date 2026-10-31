import { ConnectionProfile } from '../model/ConnectionProfile';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';
import { i18n } from '../utils/I18n';

@Component
export struct BookmarkList {
  @Prop items: ConnectionProfile[] = [];
  @StorageLink('__app_is_dark_mode__') @Watch('onThemeChanged') isDark: boolean = false;
  @StorageLink('__app_language__') appLanguage: string = 'system';
  @State colors: ThemeColors = LightColors;
  onSelect: (profile: ConnectionProfile) => void = (_profile: ConnectionProfile): void => {};
  onDelete: (profileId: string) => void = (_profileId: string): void => {};
  onQuickConnect: (profile: ConnectionProfile) => void = (_profile: ConnectionProfile): void => {};

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  aboutToAppear(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  build() {
    Column({ space: 12 }) {
      if (this.items.length === 0) {
        Column({ space: 12 }) {
          Text('—')
            .fontSize(32)
            .fontColor(this.colors.mutedForeground)
          Text(i18n.get(this.appLanguage, 'bookmarks_empty_title'))
            .fontSize(16)
            .fontColor(this.colors.mutedForeground)
          Text(i18n.get(this.appLanguage, 'bookmarks_empty_subtitle'))
            .fontSize(14)
            .fontColor(this.colors.muted)
        }
        .width('100%')
        .padding(48)
      } else {
        ForEach(this.items, (item: ConnectionProfile) => {
          Column({ space: 0 }) {
            Row() {
              Column({ space: 8 }) {
                Row({ space: 10 }) {
                  Column({ space: 3 }) {
                    Text(item.name || item.host)
                      .fontSize(17)
                      .fontWeight(FontWeight.Medium)
                      .fontColor(this.colors.foreground)
                      .maxLines(1)
                      .textOverflow({ overflow: TextOverflow.Ellipsis })
                  }

                  if (item.securityMode && item.securityMode !== 'auto') {
                    Text(item.securityMode.toUpperCase())
                      .fontSize(12)
                      .fontWeight(FontWeight.Medium)
                      .fontColor(this.colors.tagText)
                      .padding({ left: 8, right: 8, top: 3, bottom: 3 })
                      .backgroundColor(this.colors.tagBackground)
                      .borderRadius(4)
                  }
                }
                .alignItems(VerticalAlign.Center)

                Text(`${item.username ? item.username + '@' : ''}${item.host}:${item.port}`)
                  .fontSize(14)
                  .fontColor(this.colors.mutedForeground)
                  .maxLines(1)
                  .textOverflow({ overflow: TextOverflow.Ellipsis })
              }
              .layoutWeight(1)
              .alignItems(HorizontalAlign.Start)

              Button() {
                Text('→')
                  .fontSize(18)
                  .fontWeight(FontWeight.Bold)
                  .fontColor(this.colors.primaryForeground)
              }
              .type(ButtonType.Circle)
              .width(44)
              .height(44)
              .backgroundColor(this.colors.primary)
              .onClick(() => this.onQuickConnect(item))
            }
            .width('100%')
            .padding({ left: 20, right: 16, top: 18, bottom: 18 })
            .alignItems(VerticalAlign.Center)

            Row({ space: 0 }) {
              Button(i18n.get(this.appLanguage, 'load_config'))
                .type(ButtonType.Normal)
                .flexGrow(1)
                .height(44)
                .fontSize(15)
                .backgroundColor(Color.Transparent)
                .fontColor(this.colors.accentForeground)
                .borderRadius(0)
                .border({ width: { right: 1 }, color: this.colors.surfaceElevatedBorder })
                .onClick(() => this.onSelect(item))

              Button(i18n.get(this.appLanguage, 'delete'))
                .type(ButtonType.Normal)
                .flexGrow(1)
                .height(44)
                .fontSize(15)
                .backgroundColor(Color.Transparent)
                .fontColor(this.colors.destructive)
                .borderRadius(0)
                .onClick(() => this.onDelete(item.id))
            }
            .width('100%')
          }
          .width('100%')
          .backgroundColor(this.colors.surfaceElevated)
          .border({ width: 1, color: this.colors.surfaceElevatedBorder })
          .borderRadius(16)
          .clip(true)
        }, (item: ConnectionProfile) => item.id)
      }
    }
    .width('100%')
  }
}
