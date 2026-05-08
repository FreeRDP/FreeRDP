import {
  createSession as nativeCreateSession,
  connect as nativeConnect,
  disconnect as nativeDisconnect,
  getSessionInfo as nativeGetSessionInfo,
  getSnapshot as nativeGetSnapshot,
  getFrameBuffer as nativeGetFrameBuffer,
  setSurfaceId as nativeSetSurfaceId,
  getLastErrorString as nativeGetLastErrorString,
  drainEvents as nativeDrainEvents,
  submitAuth as nativeSubmitAuth,
  submitCert as nativeSubmitCert,
  setClipboardText as nativeSetClipboardText,
  sendMouseMove as nativeSendMouseMove,
  sendMouseButton as nativeSendMouseButton,
  sendMouseWheel as nativeSendMouseWheel,
  sendKeyScancode as nativeSendKeyScancode,
  sendUnicodeKey as nativeSendUnicodeKey,
  sendCtrlAltDel as nativeSendCtrlAltDel,
  sendWindowsKey as nativeSendWindowsKey,
  sendEscape as nativeSendEscape,
  NativeSessionConfig,
  NativeSessionEvent,
  NativeSessionInfo,
  NativeSessionSnapshot
} from 'libfreerdp_harmony_bridge.so';
import { ConnectionProfile, cloneProfile, createEmptyProfile } from '../model/ConnectionProfile';
import { SessionEvent, SessionEventType, SessionSnapshot, SessionStage, SessionState } from '../model/SessionModels';

const DEFAULT_DESKTOP_WIDTH: number = 1280;
const DEFAULT_DESKTOP_HEIGHT: number = 720;

const NATIVE_STAGE_IDLE: number = 0;
const NATIVE_STAGE_CONNECTING: number = 1;
const NATIVE_STAGE_CONNECTED: number = 2;
const NATIVE_STAGE_DISCONNECTED: number = 3;

export interface SessionBackend {
  createSession(profile: ConnectionProfile): number;
  connect(sessionId: number): Promise<SessionState>;
  disconnect(sessionId: number): Promise<void>;
  getState(sessionId: number): SessionState;
  getSnapshot(sessionId: number): SessionSnapshot;
  getFrameBuffer(sessionId: number): ArrayBuffer;
  setSurfaceId(sessionId: number, surfaceId: string): Promise<void>;
  drainEvents(sessionId: number): SessionEvent[];
  submitAuth(sessionId: number, username: string, pass: string, domain: string): Promise<void>;
  submitCert(sessionId: number, accept: boolean): Promise<void>;
  setClipboardText(sessionId: number, text: string): Promise<void>;
  sendMouseMove(sessionId: number, x: number, y: number): Promise<void>;
  sendMouseButton(sessionId: number, x: number, y: number, buttonFlags: number, down: boolean): Promise<void>;
  sendMouseWheel(sessionId: number, x: number, y: number, delta: number): Promise<void>;
  sendKeyScancode(sessionId: number, scancode: number, down: boolean): Promise<void>;
  sendUnicodeKey(sessionId: number, codepoint: number, down: boolean): Promise<void>;
  sendCtrlAltDel(sessionId: number): Promise<void>;
  sendWindowsKey(sessionId: number): Promise<void>;
  sendEscape(sessionId: number): Promise<void>;
}

interface SessionRecord {
  sessionId: number;
  profile: ConnectionProfile;
}

class NativeSessionBackendImpl implements SessionBackend {
  private sessions: SessionRecord[] = [];

  private findSession(sessionId: number): SessionRecord | null {
    let index: number = 0;
    while (index < this.sessions.length) {
      const record: SessionRecord = this.sessions[index];
      if (record.sessionId === sessionId) {
        return record;
      }
      index += 1;
    }
    return null;
  }

  private buildNativeConfig(profile: ConnectionProfile): NativeSessionConfig {
    return {
      name: profile.name,
      host: profile.host,
      port: profile.port,
      clientHostname: profile.clientHostname,
      username: profile.username,
      password: profile.password,
      domain: profile.domain,
      gateway: profile.gateway,
      gatewayUsername: profile.gatewayUsername,
      gatewayPassword: profile.gatewayPassword,
      gatewayDomain: profile.gatewayDomain,
      securityMode: profile.securityMode,
      appDataDir: profile.appDataDir,
      enableClipboard: profile.enableClipboard,
      enableAudio: profile.enableAudio,
      enableDrive: profile.enableDrive,
      ignoreCertificate: true,
      desktopWidth: DEFAULT_DESKTOP_WIDTH,
      desktopHeight: DEFAULT_DESKTOP_HEIGHT
    };
  }

  private mapStage(nativeStage: number, connected: boolean): SessionStage {
    if (connected || nativeStage === NATIVE_STAGE_CONNECTED) {
      return SessionStage.Connected;
    }
    if (nativeStage === NATIVE_STAGE_CONNECTING) {
      return SessionStage.Connecting;
    }
    if (nativeStage === NATIVE_STAGE_IDLE) {
      return SessionStage.Idle;
    }
    if (nativeStage === NATIVE_STAGE_DISCONNECTED) {
      return SessionStage.Disconnected;
    }
    return SessionStage.Error;
  }

  private mapEventType(nativeType: number): SessionEventType {
    if (nativeType === 1) {
      return SessionEventType.StateChanged;
    }
    if (nativeType === 2) {
      return SessionEventType.FrameUpdated;
    }
    if (nativeType === 3) {
      return SessionEventType.RemoteClipboardChanged;
    }
    if (nativeType === 4) {
      return SessionEventType.AuthRequired;
    }
    if (nativeType === 5) {
      return SessionEventType.CertRequired;
    }
    return SessionEventType.StateChanged;
  }

  private defaultMessage(stage: SessionStage): string {
    if (stage === SessionStage.Connected) {
      return '已连接到远程桌面';
    }
    if (stage === SessionStage.Connecting) {
      return '正在连接远程桌面...';
    }
    if (stage === SessionStage.Disconnected) {
      return '连接已断开';
    }
    if (stage === SessionStage.Error) {
      return '远程桌面连接失败';
    }
    return '尚未连接';
  }

  private enrichMessage(sessionId: number, stage: SessionStage, nativeInfo: NativeSessionInfo,
                        baseMessage: string): string {
    if (nativeInfo.lastError === 0) {
      return baseMessage;
    }
    if (stage !== SessionStage.Error && stage !== SessionStage.Disconnected) {
      return baseMessage;
    }

    const lastErrorString: string = nativeGetLastErrorString(sessionId);
    return lastErrorString.length > 0 && baseMessage.indexOf(lastErrorString) < 0
      ? `${baseMessage}: ${lastErrorString}`
      : baseMessage;
  }

  private buildState(sessionId: number, nativeInfo: NativeSessionInfo,
                     profile: ConnectionProfile): SessionState {
    const stage: SessionStage = this.mapStage(nativeInfo.stage, nativeInfo.connected);
    const title: string = profile.name.length > 0
      ? profile.name
      : (profile.host.length > 0 ? profile.host : `Session ${sessionId}`);
    const baseMessage: string = nativeInfo.message.length > 0
      ? nativeInfo.message
      : this.defaultMessage(stage);
    const message: string = this.enrichMessage(sessionId, stage, nativeInfo, baseMessage);

    return {
      sessionId: sessionId,
      stage: stage,
      title: title,
      message: message,
      profile: cloneProfile(profile)
    };
  }

  private requireRecord(sessionId: number): SessionRecord {
    const record: SessionRecord | null = this.findSession(sessionId);
    if (record === null) {
      throw new Error(`session ${sessionId} not found`);
    }
    return record;
  }

  private requireNativeOk(ok: boolean, action: string): void {
    if (!ok) {
      throw new Error(`native ${action} failed`);
    }
  }

  createSession(profile: ConnectionProfile): number {
    const sessionId: number = nativeCreateSession(this.buildNativeConfig(profile));
    if (sessionId > 0) {
      this.sessions = [
        ...this.sessions,
        {
          sessionId: sessionId,
          profile: cloneProfile(profile)
        }
      ];
    }
    return sessionId;
  }

  async connect(sessionId: number): Promise<SessionState> {
    const record: SessionRecord = this.requireRecord(sessionId);
    const nativeInfo: NativeSessionInfo = nativeConnect(sessionId);
    return this.buildState(sessionId, nativeInfo, record.profile);
  }

  async disconnect(sessionId: number): Promise<void> {
    this.requireRecord(sessionId);
    nativeDisconnect(sessionId);
  }

  getState(sessionId: number): SessionState {
    const record: SessionRecord | null = this.findSession(sessionId);
    if (record === null) {
      const missingInfo: NativeSessionInfo = {
        stage: NATIVE_STAGE_DISCONNECTED,
        lastError: 1,
        connected: false,
        message: 'session not found'
      };
      return this.buildState(sessionId, missingInfo, createEmptyProfile('missing'));
    }

    const nativeInfo: NativeSessionInfo = nativeGetSessionInfo(sessionId);
    return this.buildState(sessionId, nativeInfo, record.profile);
  }

  getSnapshot(sessionId: number): SessionSnapshot {
    this.requireRecord(sessionId);
    const snapshot: NativeSessionSnapshot = nativeGetSnapshot(sessionId);
    return {
      width: snapshot.width,
      height: snapshot.height,
      frameStride: snapshot.frameStride,
      frameSequence: snapshot.frameSequence,
      dirtyLeft: snapshot.dirtyLeft,
      dirtyTop: snapshot.dirtyTop,
      dirtyWidth: snapshot.dirtyWidth,
      dirtyHeight: snapshot.dirtyHeight
    };
  }

  getFrameBuffer(sessionId: number): ArrayBuffer {
    this.requireRecord(sessionId);
    return nativeGetFrameBuffer(sessionId);
  }

  drainEvents(sessionId: number): SessionEvent[] {
    this.requireRecord(sessionId);
    const events: NativeSessionEvent[] = nativeDrainEvents(sessionId);
    let index: number = 0;
    const mapped: SessionEvent[] = [];
    while (index < events.length) {
      mapped.push({
        type: this.mapEventType(events[index].type),
        stage: this.mapStage(events[index].stage, events[index].connected),
        lastError: events[index].lastError,
        connected: events[index].connected,
        frameSequence: events[index].frameSequence,
        message: events[index].message,
        clipboardText: events[index].clipboardText,
        certHost: events[index].certHost,
        certSubject: events[index].certSubject,
        certIssuer: events[index].certIssuer,
        certFingerprint: events[index].certFingerprint
      });
      index += 1;
    }
    return mapped;
  }

  async setClipboardText(sessionId: number, text: string): Promise<void> {
    this.requireRecord(sessionId);
    this.requireNativeOk(nativeSetClipboardText(sessionId, text), 'setClipboardText');
  }

  async submitAuth(sessionId: number, username: string, pass: string, domain: string): Promise<void> {
    this.requireRecord(sessionId);
    nativeSubmitAuth(sessionId, username, pass, domain);
  }

  async submitCert(sessionId: number, accept: boolean): Promise<void> {
    this.requireRecord(sessionId);
    nativeSubmitCert(sessionId, accept);
  }

  async setSurfaceId(sessionId: number, surfaceId: string): Promise<void> {
    this.requireRecord(sessionId);
    nativeSetSurfaceId(sessionId, surfaceId);
  }

  async sendMouseMove(sessionId: number, x: number, y: number): Promise<void> {
    this.requireRecord(sessionId);
    this.requireNativeOk(nativeSendMouseMove(sessionId, x, y), 'sendMouseMove');
  }

  async sendMouseButton(sessionId: number, x: number, y: number, buttonFlags: number,
                        down: boolean): Promise<void> {
    this.requireRecord(sessionId);
    this.requireNativeOk(nativeSendMouseButton(sessionId, x, y, buttonFlags, down),
      'sendMouseButton');
  }

  async sendMouseWheel(sessionId: number, x: number, y: number, delta: number): Promise<void> {
    this.requireRecord(sessionId);
    this.requireNativeOk(nativeSendMouseWheel(sessionId, x, y, delta), 'sendMouseWheel');
  }

  async sendKeyScancode(sessionId: number, scancode: number, down: boolean): Promise<void> {
    this.requireRecord(sessionId);
    this.requireNativeOk(nativeSendKeyScancode(sessionId, scancode, down), 'sendKeyScancode');
  }

  async sendUnicodeKey(sessionId: number, codepoint: number, down: boolean): Promise<void> {
    this.requireRecord(sessionId);
    this.requireNativeOk(nativeSendUnicodeKey(sessionId, codepoint, down), 'sendUnicodeKey');
  }

  async sendCtrlAltDel(sessionId: number): Promise<void> {
    this.requireRecord(sessionId);
    this.requireNativeOk(nativeSendCtrlAltDel(sessionId), 'sendCtrlAltDel');
  }

  async sendWindowsKey(sessionId: number): Promise<void> {
    this.requireRecord(sessionId);
    this.requireNativeOk(nativeSendWindowsKey(sessionId), 'sendWindowsKey');
  }

  async sendEscape(sessionId: number): Promise<void> {
    this.requireRecord(sessionId);
    this.requireNativeOk(nativeSendEscape(sessionId), 'sendEscape');
  }
}

export const sessionBackend: SessionBackend = new NativeSessionBackendImpl();
