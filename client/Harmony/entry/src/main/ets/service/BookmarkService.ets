import dataPreferences from '@ohos.data.preferences';
import common from '@ohos.app.ability.common';
import { ConnectionProfile, cloneProfile, createEmptyProfile } from '../model/ConnectionProfile';

export class BookmarkService {
  private static readonly PREFERENCES_NAME: string = 'freerdp_store';
  private static readonly BOOKMARKS_KEY: string = 'bookmarkPayload';
  private static readonly DRAFT_KEY: string = 'draftProfilePayload';

  async list(context: common.UIAbilityContext): Promise<ConnectionProfile[]> {
    return (await this.readBookmarks(context)).map((item: ConnectionProfile) => cloneProfile(item));
  }

  async save(context: common.UIAbilityContext, profile: ConnectionProfile): Promise<void> {
    const current: ConnectionProfile[] = (await this.readBookmarks(context))
      .filter((item: ConnectionProfile) => item.id !== profile.id);
    current.push(this.sanitizeProfile(profile));
    await this.writeBookmarks(context, current);
  }

  async delete(context: common.UIAbilityContext, profileId: string): Promise<void> {
    const current: ConnectionProfile[] = (await this.readBookmarks(context))
      .filter((item: ConnectionProfile) => item.id !== profileId);
    await this.writeBookmarks(context, current);
  }

  async loadDraft(context: common.UIAbilityContext, profileId: string): Promise<ConnectionProfile> {
    const payload: string = await this.readStorage(context, BookmarkService.DRAFT_KEY);
    if (payload.length === 0) {
      return createEmptyProfile(profileId);
    }
    const draft: ConnectionProfile | null = this.deserializeProfile(payload);
    return draft === null ? createEmptyProfile(profileId) : draft;
  }

  async saveDraft(context: common.UIAbilityContext, profile: ConnectionProfile): Promise<void> {
    await this.writeStorage(context, BookmarkService.DRAFT_KEY, this.serializeProfile(this.sanitizeProfile(profile)));
  }

  private sanitizeProfile(profile: ConnectionProfile): ConnectionProfile {
    const nextProfile: ConnectionProfile = cloneProfile(profile);
    if (!nextProfile.rememberPassword) {
      nextProfile.password = '';
    }
    return nextProfile;
  }

  private async readBookmarks(context: common.UIAbilityContext): Promise<ConnectionProfile[]> {
    const payload: string = await this.readStorage(context, BookmarkService.BOOKMARKS_KEY);
    if (payload.length === 0) {
      return [];
    }

    const lines: string[] = payload.split('\n');
    const bookmarks: ConnectionProfile[] = [];
    let index: number = 0;
    while (index < lines.length) {
      const line: string = lines[index];
      if (line.length > 0) {
        const profile: ConnectionProfile | null = this.deserializeProfile(line);
        if (profile !== null) {
          bookmarks.push(profile);
        }
      }
      index += 1;
    }
    return bookmarks.map((item: ConnectionProfile) => this.sanitizeProfile(item));
  }

  private async writeBookmarks(context: common.UIAbilityContext, bookmarks: ConnectionProfile[]): Promise<void> {
    const lines: string[] = bookmarks.map((item: ConnectionProfile) => this.serializeProfile(item));
    await this.writeStorage(context, BookmarkService.BOOKMARKS_KEY, lines.join('\n'));
  }

  private async readStorage(context: common.UIAbilityContext, key: string): Promise<string> {
    const legacyValue: string = AppStorage.get<string>(key) ?? '';
    try {
      const pref = await dataPreferences.getPreferences(context, { name: BookmarkService.PREFERENCES_NAME });
      const storedValue = (await pref.get(key, '')) as string;
      if (storedValue.length > 0) {
        return storedValue;
      }
      if (legacyValue.length > 0) {
        await pref.put(key, legacyValue);
        await pref.flush();
        return legacyValue;
      }
    } catch (_error) {
      if (legacyValue.length > 0) {
        return legacyValue;
      }
    }
    return legacyValue;
  }

  private async writeStorage(context: common.UIAbilityContext, key: string, value: string): Promise<void> {
    AppStorage.setOrCreate<string>(key, value);
    try {
      const pref = await dataPreferences.getPreferences(context, { name: BookmarkService.PREFERENCES_NAME });
      await pref.put(key, value);
      await pref.flush();
    } catch (_error) {
    }
  }

  private encode(value: string): string {
    return encodeURIComponent(value);
  }

  private decode(value: string): string {
    return decodeURIComponent(value);
  }

  private serializeProfile(profile: ConnectionProfile): string {
    const parts: string[] = [
      this.encode(profile.id),
      this.encode(profile.name),
      this.encode(profile.host),
      profile.port.toString(),
      this.encode(profile.clientHostname),
      this.encode(profile.username),
      this.encode(profile.password),
      profile.rememberPassword ? '1' : '0',
      this.encode(profile.domain),
      this.encode(profile.gateway),
      this.encode(profile.gatewayUsername),
      this.encode(profile.gatewayPassword),
      this.encode(profile.gatewayDomain),
      this.encode(profile.securityMode),
      profile.enableClipboard ? '1' : '0',
      profile.enableAudio ? '1' : '0',
      profile.enableDrive ? '1' : '0',
      this.encode(profile.appDataDir)
    ];
    return parts.join('|');
  }

  private deserializeProfile(payload: string): ConnectionProfile | null {
    const parts: string[] = payload.split('|');
    if (parts.length !== 11 && parts.length !== 12 && parts.length !== 14 &&
        parts.length !== 15 && parts.length !== 17 && parts.length !== 18) {
      return null;
    }

    const portNumber: number = Number(parts[3]);
    const hasClientHostname: boolean = parts.length === 14 || parts.length === 15 ||
      parts.length === 17 || parts.length === 18;
    const hasRememberPassword: boolean = parts.length === 12 || parts.length === 15 ||
      parts.length === 18;
    const isGatewayExtendedFormat: boolean = parts.length === 17 || parts.length === 18;
    const isExtendedFormat: boolean = parts.length === 14 || parts.length === 15 ||
      isGatewayExtendedFormat;
    const clientHostnameIndex: number = hasClientHostname ? 4 : -1;
    const usernameIndex: number = hasClientHostname ? 5 : 4;
    const passwordIndex: number = hasClientHostname ? 6 : 5;
    const rememberPasswordIndex: number = hasRememberPassword ? (hasClientHostname ? 7 : 6) : -1;
    const domainIndex: number = hasRememberPassword
      ? (hasClientHostname ? 8 : 7)
      : (hasClientHostname ? 7 : 6);
    const gatewayIndex: number = domainIndex + 1;
    const gatewayUsernameIndex: number = isGatewayExtendedFormat ? gatewayIndex + 1 : -1;
    const gatewayPasswordIndex: number = isGatewayExtendedFormat ? gatewayIndex + 2 : -1;
    const gatewayDomainIndex: number = isGatewayExtendedFormat ? gatewayIndex + 3 : -1;
    const securityModeIndex: number = isGatewayExtendedFormat ? gatewayIndex + 4 :
      (isExtendedFormat ? gatewayIndex + 1 : -1);
    const clipboardIndex: number = isExtendedFormat
      ? securityModeIndex + 1
      : (hasRememberPassword ? gatewayIndex + 1 : gatewayIndex + 1);
    const audioIndex: number = clipboardIndex + 1;
    const driveIndex: number = clipboardIndex + 2;
    const appDataDirIndex: number = isExtendedFormat ? clipboardIndex + 3 : -1;
    const rememberPassword: boolean = hasRememberPassword
      ? parts[rememberPasswordIndex] === '1'
      : this.decode(parts[passwordIndex]).length > 0;
    const password: string = rememberPassword ? this.decode(parts[passwordIndex]) : '';
    return {
      id: this.decode(parts[0]),
      name: this.decode(parts[1]),
      host: this.decode(parts[2]),
      port: Number.isFinite(portNumber) && portNumber > 0 ? portNumber : 3389,
      clientHostname: hasClientHostname ? this.decode(parts[clientHostnameIndex]) : '',
      username: this.decode(parts[usernameIndex]),
      password,
      rememberPassword,
      domain: this.decode(parts[domainIndex]),
      gateway: this.decode(parts[gatewayIndex]),
      gatewayUsername: isGatewayExtendedFormat ? this.decode(parts[gatewayUsernameIndex]) : '',
      gatewayPassword: isGatewayExtendedFormat ? this.decode(parts[gatewayPasswordIndex]) : '',
      gatewayDomain: isGatewayExtendedFormat ? this.decode(parts[gatewayDomainIndex]) : '',
      securityMode: isExtendedFormat ? this.decode(parts[securityModeIndex]) : 'auto',
      appDataDir: isExtendedFormat ? this.decode(parts[appDataDirIndex]) : '',
      enableClipboard: parts[clipboardIndex] === '1',
      enableAudio: parts[audioIndex] === '1',
      enableDrive: parts[driveIndex] === '1'
    };
  }
}
