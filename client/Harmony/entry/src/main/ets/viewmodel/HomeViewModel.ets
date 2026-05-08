import { ConnectionProfile, createEmptyProfile, cloneProfile } from '../model/ConnectionProfile';
import { BookmarkService } from '../service/BookmarkService';
import { IdGenerator } from '../utils/IdGenerator';
import common from '@ohos.app.ability.common';

@Observed
export class HomeViewModel {
  profile: ConnectionProfile;
  bookmarks: ConnectionProfile[] = [];
  lastMessage: string = '等待连接';

  private bookmarkService: BookmarkService = new BookmarkService();
  private context: common.UIAbilityContext | null = null;

  constructor() {
    this.profile = createEmptyProfile(IdGenerator.next('profile'));
    this.bookmarks = [];
  }

  private copyProfile(profile: ConnectionProfile): ConnectionProfile {
    return cloneProfile(profile);
  }

  async initialize(context: common.UIAbilityContext): Promise<void> {
    this.context = context;
    const fallbackId: string = this.profile.id.length > 0 ? this.profile.id : IdGenerator.next('profile');
    this.profile = await this.bookmarkService.loadDraft(context, fallbackId);
    this.bookmarks = await this.bookmarkService.list(context);
  }

  private persistDraft(): void {
    const context = this.context;
    if (context === null) {
      return;
    }
    void this.bookmarkService.saveDraft(context, this.profile);
  }

  updateName(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.name = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updateHost(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.host = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updatePort(value: string): void {
    const parsed: number = Number(value);
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.port = Number.isFinite(parsed) && parsed > 0 ? parsed : 3389;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updateUsername(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.username = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updateDomain(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.domain = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updatePassword(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.password = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  toggleRememberPassword(value: boolean): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.rememberPassword = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updateGateway(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.gateway = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updateGatewayUsername(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.gatewayUsername = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updateGatewayPassword(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.gatewayPassword = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updateGatewayDomain(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.gatewayDomain = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  updateSecurityMode(value: string): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.securityMode = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  toggleClipboard(value: boolean): void {
    const nextProfile: ConnectionProfile = this.copyProfile(this.profile);
    nextProfile.enableClipboard = value;
    this.profile = nextProfile;
    this.persistDraft();
  }

  createNewProfile(): void {
    this.profile = createEmptyProfile(IdGenerator.next('profile'));
    this.persistDraft();
  }

  async deleteBookmark(profileId: string): Promise<void> {
    if (this.context === null) return;
    await this.bookmarkService.delete(this.context, profileId);
    this.bookmarks = await this.bookmarkService.list(this.context);
  }

  applyBookmark(profile: ConnectionProfile): void {
    this.profile = this.copyProfile(profile);
    this.persistDraft();
    this.lastMessage = `已载入 ${profile.name}`;
  }

  get canConnect(): boolean {
    return this.profile.host.trim().length > 0 && this.profile.username.trim().length > 0;
  }

  async saveCurrentProfile(): Promise<void> {
    const context = this.context;
    if (context === null) {
      this.lastMessage = '书签服务尚未初始化';
      return;
    }
    const saved: ConnectionProfile = this.copyProfile(this.profile);
    saved.id = this.profile.id.length > 0 ? this.profile.id : IdGenerator.next('bookmark');
    saved.name = this.profile.name.length > 0 ? this.profile.name : this.profile.host;
    await this.bookmarkService.save(context, saved);
    this.bookmarks = await this.bookmarkService.list(context);
    this.profile = this.copyProfile(saved);
    this.persistDraft();
    this.lastMessage = '书签已保存';
  }
}
