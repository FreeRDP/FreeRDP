import { ConnectionProfile, cloneProfile } from '../model/ConnectionProfile';
import { SessionEvent, SessionEventType, SessionSnapshot, SessionStage, SessionState } from '../model/SessionModels';
import { sessionBridge } from '../service/SessionNativeBridge';
import { i18n } from '../utils/I18n';

@Observed
export class SessionViewModel {
  state: SessionState;
  snapshot: SessionSnapshot = {
    width: 0,
    height: 0,
    frameStride: 0,
    frameSequence: 0,
    dirtyLeft: 0,
    dirtyTop: 0,
    dirtyWidth: 0,
    dirtyHeight: 0
  };
  remoteClipboardText: string = '';
  private lastObservedFrameSequence: number = 0;

  onAuthRequired: (event: SessionEvent) => void = () => {};
  onCertRequired: (event: SessionEvent) => void = () => {};

  constructor(profile: ConnectionProfile) {
    this.state = {
      sessionId: 0,
      stage: SessionStage.Idle,
      title: profile.name.length > 0 ? profile.name : profile.host,
      message: i18n.get(i18n.currentLocale(), 'not_connected'),
      profile: profile
    };
  }

  private copyState(current: SessionState): SessionState {
    return {
      sessionId: current.sessionId,
      stage: current.stage,
      title: current.title,
      message: current.message,
      profile: cloneProfile(current.profile)
    };
  }

  async connect(): Promise<void> {
    const sessionId: number = sessionBridge.createSession(this.state.profile);
    const connectingState: SessionState = this.copyState(this.state);
    connectingState.sessionId = sessionId;
    connectingState.stage = SessionStage.Connecting;
    connectingState.message = i18n.get(i18n.currentLocale(), 'connecting_msg');
    this.state = connectingState;

    const connected: SessionState = await sessionBridge.connect(sessionId);
    this.snapshot = sessionBridge.getSnapshot(sessionId);
    this.lastObservedFrameSequence = this.snapshot.frameSequence;
    this.state = connected;
  }

  setFailure(message: string): void {
    const failedState: SessionState = this.copyState(this.state);
    failedState.stage = SessionStage.Error;
    failedState.message = message;
    this.state = failedState;
  }

  async disconnect(): Promise<void> {
    await sessionBridge.disconnect(this.state.sessionId);
    const disconnectedState: SessionState = this.copyState(this.state);
    disconnectedState.stage = SessionStage.Disconnected;
    disconnectedState.message = i18n.get(i18n.currentLocale(), 'disconnected_msg');
    this.state = disconnectedState;
  }

  refreshState(): void {
    if (this.state.sessionId <= 0) {
      return;
    }
    this.state = sessionBridge.getState(this.state.sessionId);
  }

  refreshSnapshot(): void {
    if (this.state.sessionId <= 0) {
      return;
    }
    this.snapshot = sessionBridge.getSnapshot(this.state.sessionId);
    this.lastObservedFrameSequence = this.snapshot.frameSequence;
  }

  drainNativeEvents(): void {
    if (this.state.sessionId <= 0) {
      return;
    }

    const events: SessionEvent[] = sessionBridge.drainEvents(this.state.sessionId);
    let shouldRefreshState: boolean = false;
    let shouldRefreshSnapshot: boolean = false;
    let index: number = 0;
    while (index < events.length) {
      const event: SessionEvent = events[index];
      if (event.type === SessionEventType.StateChanged) {
        shouldRefreshState = true;
      } else if (event.type === SessionEventType.FrameUpdated) {
        if (event.frameSequence > this.lastObservedFrameSequence) {
          shouldRefreshSnapshot = true;
        }
      } else if (event.type === SessionEventType.RemoteClipboardChanged) {
        this.remoteClipboardText = event.clipboardText;
      } else if (event.type === SessionEventType.AuthRequired) {
        shouldRefreshState = true;
        this.onAuthRequired(event);
      } else if (event.type === SessionEventType.CertRequired) {
        shouldRefreshState = true;
        this.onCertRequired(event);
      }
      index += 1;
    }

    if (shouldRefreshState) {
      this.refreshState();
    }
    if (shouldRefreshSnapshot) {
      this.refreshSnapshot();
    }
  }

  consumeRemoteClipboardText(): string {
    const text: string = this.remoteClipboardText;
    this.remoteClipboardText = '';
    return text;
  }

  async updateLocalClipboard(text: string): Promise<void> {
    if (this.state.sessionId <= 0) {
      return;
    }
    await sessionBridge.setClipboardText(this.state.sessionId, text);
  }

  getFrameBuffer(): ArrayBuffer {
    if (this.state.sessionId <= 0) {
      return new ArrayBuffer(0);
    }
    return sessionBridge.getFrameBuffer(this.state.sessionId);
  }

  async setSurfaceId(surfaceId: string): Promise<void> {
    if (this.state.sessionId > 0) {
      await sessionBridge.setSurfaceId(this.state.sessionId, surfaceId);
    }
  }

  async submitAuth(username: string, pass: string, domain: string): Promise<void> {
    if (this.state.sessionId > 0) {
      await sessionBridge.submitAuth(this.state.sessionId, username, pass, domain);
    }
  }

  async submitCert(accept: boolean): Promise<void> {
    if (this.state.sessionId > 0) {
      await sessionBridge.submitCert(this.state.sessionId, accept);
    }
  }

  async sendMouseMove(x: number, y: number): Promise<void> {
    await sessionBridge.sendMouseMove(this.state.sessionId, x, y);
  }

  async sendMouseButton(x: number, y: number, buttonFlags: number, down: boolean): Promise<void> {
    await sessionBridge.sendMouseButton(this.state.sessionId, x, y, buttonFlags, down);
  }

  async sendMouseWheel(x: number, y: number, delta: number): Promise<void> {
    await sessionBridge.sendMouseWheel(this.state.sessionId, x, y, delta);
  }

  async sendKeyScancode(scancode: number, down: boolean): Promise<void> {
    await sessionBridge.sendKeyScancode(this.state.sessionId, scancode, down);
  }

  async sendUnicodeKey(codepoint: number, down: boolean): Promise<void> {
    await sessionBridge.sendUnicodeKey(this.state.sessionId, codepoint, down);
  }

  async sendCtrlAltDel(): Promise<void> {
    await sessionBridge.sendCtrlAltDel(this.state.sessionId);
    const nextState: SessionState = this.copyState(this.state);
    nextState.message = i18n.get(i18n.currentLocale(), 'sent_ctrl_alt_del');
    this.state = nextState;
  }

  async sendWindowsKey(): Promise<void> {
    await sessionBridge.sendWindowsKey(this.state.sessionId);
    const nextState: SessionState = this.copyState(this.state);
    nextState.message = i18n.get(i18n.currentLocale(), 'sent_windows_key');
    this.state = nextState;
  }

  async sendEscape(): Promise<void> {
    await sessionBridge.sendEscape(this.state.sessionId);
    const nextState: SessionState = this.copyState(this.state);
    nextState.message = i18n.get(i18n.currentLocale(), 'sent_escape');
    this.state = nextState;
  }
}
