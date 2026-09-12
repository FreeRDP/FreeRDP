export interface ConnectionProfile {
  id: string;
  name: string;
  host: string;
  port: number;
  clientHostname: string;
  username: string;
  password: string;
  rememberPassword: boolean;
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
}

export function createEmptyProfile(id: string): ConnectionProfile {
  return {
    id: id,
    name: '',
    host: '',
    port: 3389,
    clientHostname: '',
    username: '',
    password: '',
    rememberPassword: false,
    domain: '',
    gateway: '',
    gatewayUsername: '',
    gatewayPassword: '',
    gatewayDomain: '',
    securityMode: 'auto',
    appDataDir: '',
    enableClipboard: true,
    enableAudio: false,
    enableDrive: false
  };
}

export function cloneProfile(profile: ConnectionProfile): ConnectionProfile {
  return {
    id: profile.id,
    name: profile.name,
    host: profile.host,
    port: profile.port,
    clientHostname: profile.clientHostname,
    username: profile.username,
    password: profile.password,
    rememberPassword: profile.rememberPassword,
    domain: profile.domain,
    gateway: profile.gateway,
    gatewayUsername: profile.gatewayUsername,
    gatewayPassword: profile.gatewayPassword,
    gatewayDomain: profile.gatewayDomain,
    securityMode: profile.securityMode,
    appDataDir: profile.appDataDir,
    enableClipboard: profile.enableClipboard,
    enableAudio: profile.enableAudio,
    enableDrive: profile.enableDrive
  };
}
