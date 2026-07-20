import { ConnectionProfile } from './ConnectionProfile';

export enum SessionStage {
  Idle = 'idle',
  Connecting = 'connecting',
  Connected = 'connected',
  Disconnected = 'disconnected',
  Error = 'error',
  AuthRequired = 'auth_required',
  CertRequired = 'cert_required'
}

export interface SessionState {
  sessionId: number;
  stage: SessionStage;
  title: string;
  message: string;
  profile: ConnectionProfile;
}

export interface SessionSnapshot {
  width: number;
  height: number;
  frameStride: number;
  frameSequence: number;
  dirtyLeft: number;
  dirtyTop: number;
  dirtyWidth: number;
  dirtyHeight: number;
}

export enum SessionEventType {
  StateChanged = 1,
  FrameUpdated = 2,
  RemoteClipboardChanged = 3,
  AuthRequired = 4,
  CertRequired = 5
}

export interface SessionEvent {
  type: SessionEventType;
  stage: SessionStage;
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
