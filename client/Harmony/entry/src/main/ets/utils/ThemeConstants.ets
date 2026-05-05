export interface ThemeColors {
  background: string;
  foreground: string;
  primary: string;
  primaryForeground: string;
  border: string;
  muted: string;
  mutedForeground: string;
  card: string;
  cardForeground: string;
  cardBorder: string;
  destructive: string;
  destructiveForeground: string;
  accent: string;
  accentForeground: string;
  tagBackground: string;
  tagText: string;
  inputBackground: string;
  inputBorder: string;
  connectedGreen: string;
  connectingAmber: string;
  errorRed: string;
  disconnectedGray: string;
  hintOrange: string;
  hintOrangeBg: string;
  chromeOverlay: string;
  chromeOverlayText: string;
  chromeOverlayBorder: string;
  chromeOverlayShadow: string;
  dotActive: string;
  dotInactive: string;
}

export const LightColors: ThemeColors = {
  background: '#fdfbf7',
  foreground: '#1c1917',
  primary: '#2c4f54',
  primaryForeground: '#ffffff',
  border: '#e7e5e4',
  muted: '#f5f2ed',
  mutedForeground: '#6b6560',
  card: '#ffffff',
  cardForeground: '#1c1917',
  cardBorder: '#e7e5e4',
  destructive: '#b91c1c',
  destructiveForeground: '#ffffff',
  accent: '#f5f2ed',
  accentForeground: '#2c4f54',
  tagBackground: '#eef2ee',
  tagText: '#2c4f54',
  inputBackground: '#fdfbf7',
  inputBorder: '#e7e5e4',
  connectedGreen: '#339966',
  connectingAmber: '#b0882c',
  errorRed: '#b91c1c',
  disconnectedGray: '#6b6560',
  hintOrange: '#b0882c',
  hintOrangeBg: '#fbf5e8',
  chromeOverlay: 'rgba(253,251,247,0.82)',
  chromeOverlayText: '#1c1917',
  chromeOverlayBorder: 'rgba(44,79,84,0.12)',
  chromeOverlayShadow: 'rgba(0,0,0,0.06)',
  dotActive: 'rgba(28,25,23,0.9)',
  dotInactive: 'rgba(28,25,23,0.38)'
};

export const DarkColors: ThemeColors = {
  background: '#0c1416',
  foreground: '#e7e5e4',
  primary: '#6d9ea6',
  primaryForeground: '#0c1416',
  border: '#1e293b',
  muted: '#162024',
  mutedForeground: '#8899a0',
  card: '#111b1e',
  cardForeground: '#e7e5e4',
  cardBorder: '#1e293b',
  destructive: '#ef4444',
  destructiveForeground: '#ffffff',
  accent: '#162024',
  accentForeground: '#6d9ea6',
  tagBackground: 'rgba(109,158,166,0.16)',
  tagText: '#6d9ea6',
  inputBackground: '#0f181a',
  inputBorder: '#1e293b',
  connectedGreen: '#4ade80',
  connectingAmber: '#fbbf24',
  errorRed: '#f87171',
  disconnectedGray: '#8899a0',
  hintOrange: '#fbbf24',
  hintOrangeBg: 'rgba(251,191,36,0.12)',
  chromeOverlay: 'rgba(12,20,22,0.82)',
  chromeOverlayText: '#e7e5e4',
  chromeOverlayBorder: 'rgba(109,158,166,0.18)',
  chromeOverlayShadow: 'rgba(0,0,0,0.22)',
  dotActive: 'rgba(231,229,228,0.9)',
  dotInactive: 'rgba(231,229,228,0.42)'
};

const THEME_KEY: string = '__app_theme_mode__';

export enum ThemeMode {
  Light = 'light',
  Dark = 'dark'
}

export function getThemeColors(isDark: boolean): ThemeColors {
  return isDark ? DarkColors : LightColors;
}

export function getStoredThemeMode(): ThemeMode {
  try {
    const stored: string | undefined = AppStorage.get<string>(THEME_KEY);
    if (stored === ThemeMode.Dark) {
      return ThemeMode.Dark;
    }
  } catch (_e) {
  }
  return ThemeMode.Light;
}

export function persistThemeMode(mode: ThemeMode): void {
  AppStorage.setOrCreate(THEME_KEY, mode);
}
