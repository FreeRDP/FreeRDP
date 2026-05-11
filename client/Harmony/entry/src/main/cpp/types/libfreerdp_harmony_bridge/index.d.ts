export interface NativeSessionConfig {
  name: string;
  host: string;
  port: number;
  clientHostname: string;
  username: string;
  password: string;
  domain: string;
  gateway: string;
  gatewayUsername: string;
  gatewayPassword: string;
  gatewayDomain: string;
  securityMode: string;
  appDataDir: string;
  enableClipboard: boolean;
  enableAudio: boolean;
  enableDrive: boolean;
  ignoreCertificate: boolean;
  desktopWidth: number;
  desktopHeight: number;
}

export interface NativeSessionInfo {
  stage: number;
  lastError: number;
  connected: boolean;
  message: string;
}

export interface NativeSessionSnapshot {
  width: number;
  height: number;
  dirtyLeft: number;
  dirtyTop: number;
  dirtyWidth: number;
  dirtyHeight: number;
  frameStride: number;
  frameSequence: number;
}

export interface NativeSessionEvent {
  type: number;
  stage: number;
  lastError: number;
  connected: boolean;
  frameSequence: number;
  message: string;
  clipboardText: string;
  certHost: string;
  certSubject: string;
  certIssuer: string;
  certFingerprint: string;
  authUsername: string;
  authDomain: string;
}

export const createSession: (config: NativeSessionConfig) => number;
export const connect: (sessionId: number) => NativeSessionInfo;
export const disconnect: (sessionId: number) => void;
export const getSessionInfo: (sessionId: number) => NativeSessionInfo;
export const getSnapshot: (sessionId: number) => NativeSessionSnapshot;
export const getFrameBuffer: (sessionId: number) => ArrayBuffer;
export const setSurfaceId: (sessionId: number, surfaceId: string) => void;
export const submitAuth: (sessionId: number, username: string, pass: string, domain: string) => void;
export const submitCert: (sessionId: number, accept: boolean) => void;
export const getLastErrorString: (sessionId: number) => string;
export const drainEvents: (sessionId: number) => NativeSessionEvent[];
export const setClipboardText: (sessionId: number, text: string) => boolean;
export const sendMouseMove: (sessionId: number, x: number, y: number) => boolean;
export const sendMouseButton: (sessionId: number, x: number, y: number, buttonFlags: number, down: boolean) => boolean;
export const sendMouseWheel: (sessionId: number, x: number, y: number, delta: number) => boolean;
export const sendKeyScancode: (sessionId: number, scancode: number, down: boolean) => boolean;
export const sendUnicodeKey: (sessionId: number, codepoint: number, down: boolean) => boolean;
export const sendCtrlAltDel: (sessionId: number) => boolean;
export const sendWindowsKey: (sessionId: number) => boolean;
export const sendEscape: (sessionId: number) => boolean;
