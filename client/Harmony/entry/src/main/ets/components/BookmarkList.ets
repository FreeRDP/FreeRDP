import { ConnectionProfile } from '../model/ConnectionProfile';

@Component
export struct BookmarkList {
  @Prop items: ConnectionProfile[];
  onSelect: (profile: ConnectionProfile) => void = (_profile: ConnectionProfile): void => {};
  onDelete: (profileId: string) => void = (_profileId: string): void => {};
  onQuickConnect: (profile: ConnectionProfile) => void = (_profile: ConnectionProfile): void => {};

  build() {
    Column({ space: 12 }) {
      Text('已保存的连接')
        .fontSize(16)
        .fontWeight(FontWeight.Medium)
        .fontColor('#111827')
        .alignSelf(ItemAlign.Start)
        .padding({ bottom: 4 })

      if (this.items.length === 0) {
        Text('暂无保存的连接，请在上方填写信息并保存。')
          .fontSize(13)
          .fontColor('#6B7280')
          .padding({ top: 20, bottom: 20 })
      } else {
        ForEach(this.items, (item: ConnectionProfile) => {
          Row() {
            Column({ space: 4 }) {
              Text(item.name || item.host)
                .fontSize(15)
                .fontWeight(FontWeight.Medium)
                .fontColor('#111827')
                .maxLines(1)
                .textOverflow({ overflow: TextOverflow.Ellipsis })
              Text(`${item.username ? item.username + '@' : ''}${item.host}:${item.port}`)
                .fontSize(12)
                .fontColor('#6B7280')
                .maxLines(1)
                .textOverflow({ overflow: TextOverflow.Ellipsis })
            }
            .layoutWeight(1)
            .alignItems(HorizontalAlign.Start)

            Row({ space: 8 }) {
              Button('连接')
                .type(ButtonType.Normal)
                .fontSize(13)
                .height(32)
                .backgroundColor('#2E6BE6')
                .fontColor('#FFFFFF')
                .borderRadius(8)
                .onClick(() => this.onQuickConnect(item))

              Button('载入')
                .type(ButtonType.Normal)
                .fontSize(13)
                .height(32)
                .backgroundColor('#EBF5FF')
                .fontColor('#2E6BE6')
                .borderRadius(8)
                .onClick(() => this.onSelect(item))

              Button('删除')
                .type(ButtonType.Normal)
                .fontSize(13)
                .height(32)
                .backgroundColor('#FEE2E2')
                .fontColor('#DC2626')
                .borderRadius(8)
                .onClick(() => this.onDelete(item.id))
            }
          }
          .width('100%')
          .padding(16)
          .backgroundColor('#F9FAFB')
          .border({ width: 1, color: '#F3F4F6' })
          .borderRadius(12)
        }, (item: ConnectionProfile) => item.id)
      }
    }
    .width('100%')
  }
}
