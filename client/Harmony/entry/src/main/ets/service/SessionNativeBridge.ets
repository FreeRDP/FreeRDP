import { ConnectionProfile } from '../model/ConnectionProfile';
import { SessionEvent, SessionSnapshot, SessionState } from '../model/SessionModels';
import { sessionBackend } from './NativeSessionBackend';

export interface SessionBridge {
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

class DefaultSessionBridge implements SessionBridge {
  createSession(profile: ConnectionProfile): number {
    const sessionId: number = sessionBackend.createSession(profile);
    return sessionId;
  }

  async connect(sessionId: number): Promise<SessionState> {
    return sessionBackend.connect(sessionId);
  }

  async disconnect(sessionId: number): Promise<void> {
    return sessionBackend.disconnect(sessionId);
  }

  getState(sessionId: number): SessionState {
    return sessionBackend.getState(sessionId);
  }

  getSnapshot(sessionId: number): SessionSnapshot {
    return sessionBackend.getSnapshot(sessionId);
  }

  getFrameBuffer(sessionId: number): ArrayBuffer {
    return sessionBackend.getFrameBuffer(sessionId);
  }

  async setSurfaceId(sessionId: number, surfaceId: string): Promise<void> {
    await sessionBackend.setSurfaceId(sessionId, surfaceId);
  }

  drainEvents(sessionId: number): SessionEvent[] {
    return sessionBackend.drainEvents(sessionId);
  }

  async submitAuth(sessionId: number, username: string, pass: string, domain: string): Promise<void> {
    await sessionBackend.submitAuth(sessionId, username, pass, domain);
  }

  async submitCert(sessionId: number, accept: boolean): Promise<void> {
    await sessionBackend.submitCert(sessionId, accept);
  }

  async setClipboardText(sessionId: number, text: string): Promise<void> {
    return sessionBackend.setClipboardText(sessionId, text);
  }

  async sendMouseMove(sessionId: number, x: number, y: number): Promise<void> {
    return sessionBackend.sendMouseMove(sessionId, x, y);
  }

  async sendMouseButton(sessionId: number, x: number, y: number, buttonFlags: number, down: boolean): Promise<void> {
    return sessionBackend.sendMouseButton(sessionId, x, y, buttonFlags, down);
  }

  async sendMouseWheel(sessionId: number, x: number, y: number, delta: number): Promise<void> {
    return sessionBackend.sendMouseWheel(sessionId, x, y, delta);
  }

  async sendKeyScancode(sessionId: number, scancode: number, down: boolean): Promise<void> {
    return sessionBackend.sendKeyScancode(sessionId, scancode, down);
  }

  async sendUnicodeKey(sessionId: number, codepoint: number, down: boolean): Promise<void> {
    return sessionBackend.sendUnicodeKey(sessionId, codepoint, down);
  }

  async sendCtrlAltDel(sessionId: number): Promise<void> {
    return sessionBackend.sendCtrlAltDel(sessionId);
  }

  async sendWindowsKey(sessionId: number): Promise<void> {
    return sessionBackend.sendWindowsKey(sessionId);
  }

  async sendEscape(sessionId: number): Promise<void> {
    return sessionBackend.sendEscape(sessionId);
  }
}

export const sessionBridge: SessionBridge = new DefaultSessionBridge();
