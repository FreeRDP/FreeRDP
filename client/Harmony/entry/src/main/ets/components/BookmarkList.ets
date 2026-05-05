import { ConnectionProfile } from '../model/ConnectionProfile';
import { ThemeColors, LightColors, DarkColors } from '../utils/ThemeConstants';

@Component
export struct BookmarkList {
  @Prop items: ConnectionProfile[];
  @Prop @Watch('onThemeChanged') isDark: boolean = false;
  @State colors: ThemeColors = LightColors;
  onSelect: (profile: ConnectionProfile) => void = (_profile: ConnectionProfile): void => {};
  onDelete: (profileId: string) => void = (_profileId: string): void => {};
  onQuickConnect: (profile: ConnectionProfile) => void = (_profile: ConnectionProfile): void => {};

  onThemeChanged(): void {
    this.colors = this.isDark ? DarkColors : LightColors;
  }

  build() {
    Column({ space: 12 }) {
      Text('已保存的连接')
        .fontSize(16)
        .fontWeight(FontWeight.Medium)
        .fontColor(this.colors.foreground)
        .alignSelf(ItemAlign.Start)
        .padding({ bottom: 4 })

      if (this.items.length === 0) {
        Text('暂无保存的连接，请在上方填写信息并保存。')
          .fontSize(13)
          .fontColor(this.colors.mutedForeground)
          .padding({ top: 20, bottom: 20 })
      } else {
        ForEach(this.items, (item: ConnectionProfile) => {
          Row() {
            Column({ space: 4 }) {
              Text(item.name || item.host)
                .fontSize(15)
                .fontWeight(FontWeight.Medium)
                .fontColor(this.colors.foreground)
                .maxLines(1)
                .textOverflow({ overflow: TextOverflow.Ellipsis })
              Text(`${item.username ? item.username + '@' : ''}${item.host}:${item.port}`)
                .fontSize(12)
                .fontColor(this.colors.mutedForeground)
                .maxLines(1)
                .textOverflow({ overflow: TextOverflow.Ellipsis })
            }
            .layoutWeight(1)
            .alignItems(HorizontalAlign.Start)

            Row({ space: 8 }) {
              Button('连接')
                .type(ButtonType.Normal)
                .fontSize(12)
                .height(32)
                .backgroundColor(this.colors.primary)
                .fontColor(this.colors.primaryForeground)
                .borderRadius(8)
                .onClick(() => this.onQuickConnect(item))

              Button('载入')
                .type(ButtonType.Normal)
                .fontSize(12)
                .height(32)
                .backgroundColor(this.colors.accent)
                .fontColor(this.colors.accentForeground)
                .borderRadius(8)
                .onClick(() => this.onSelect(item))

              Button('删除')
                .type(ButtonType.Normal)
                .fontSize(12)
                .height(32)
                .backgroundColor('rgba(185,28,28,0.08)')
                .fontColor(this.colors.destructive)
                .borderRadius(8)
                .onClick(() => this.onDelete(item.id))
            }
          }
          .width('100%')
          .padding(16)
          .backgroundColor(this.colors.muted)
          .border({ width: 1, color: this.colors.border })
          .borderRadius(14)
        }, (item: ConnectionProfile) => item.id)
      }
    }
    .width('100%')
  }
}
