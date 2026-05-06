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
  shadowSm: string;
  shadowMd: string;
  shadowLg: string;
  shadowXl: string;
  overlayScrim: string;
  gradientStart: string;
  gradientEnd: string;
  surfaceElevated: string;
  surfaceElevatedBorder: string;
}

export const LightColors: ThemeColors = {
  background: '#fafaf8',
  foreground: '#1c1917',
  primary: '#2c5f63',
  primaryForeground: '#ffffff',
  border: '#e2e0dc',
  muted: '#f3f1ed',
  mutedForeground: '#6b6560',
  card: '#ffffff',
  cardForeground: '#1c1917',
  cardBorder: '#e8e6e1',
  destructive: '#b91c1c',
  destructiveForeground: '#ffffff',
  accent: '#eef4f3',
  accentForeground: '#2c5f63',
  tagBackground: '#e8f0ee',
  tagText: '#2c5f63',
  inputBackground: '#fafaf8',
  inputBorder: '#d9d7d2',
  connectedGreen: '#22a06b',
  connectingAmber: '#b0882c',
  errorRed: '#dc2626',
  disconnectedGray: '#78716c',
  hintOrange: '#b0882c',
  hintOrangeBg: '#fef8ec',
  chromeOverlay: 'rgba(250,250,248,0.88)',
  chromeOverlayText: '#1c1917',
  chromeOverlayBorder: 'rgba(44,95,99,0.12)',
  chromeOverlayShadow: 'rgba(28,25,23,0.08)',
  dotActive: 'rgba(28,25,23,0.9)',
  dotInactive: 'rgba(28,25,23,0.35)',
  shadowSm: 'rgba(28,25,23,0.04)',
  shadowMd: 'rgba(28,25,23,0.07)',
  shadowLg: 'rgba(28,25,23,0.10)',
  shadowXl: 'rgba(28,25,23,0.14)',
  overlayScrim: 'rgba(28,25,23,0.45)',
  gradientStart: '#e8f4f2',
  gradientEnd: '#f0f7f6',
  surfaceElevated: '#ffffff',
  surfaceElevatedBorder: '#e2e0dc'
};

export const DarkColors: ThemeColors = {
  background: '#0a1214',
  foreground: '#eceae6',
  primary: '#5fb8b8',
  primaryForeground: '#0a1214',
  border: '#1c2a2e',
  muted: '#131e21',
  mutedForeground: '#8899a0',
  card: '#101a1d',
  cardForeground: '#eceae6',
  cardBorder: '#1c2a2e',
  destructive: '#ef4444',
  destructiveForeground: '#ffffff',
  accent: '#152427',
  accentForeground: '#5fb8b8',
  tagBackground: 'rgba(95,184,184,0.13)',
  tagText: '#5fb8b8',
  inputBackground: '#0d1619',
  inputBorder: '#1c2a2e',
  connectedGreen: '#34d399',
  connectingAmber: '#fbbf24',
  errorRed: '#f87171',
  disconnectedGray: '#8899a0',
  hintOrange: '#fbbf24',
  hintOrangeBg: 'rgba(251,191,36,0.10)',
  chromeOverlay: 'rgba(10,18,20,0.85)',
  chromeOverlayText: '#eceae6',
  chromeOverlayBorder: 'rgba(95,184,184,0.16)',
  chromeOverlayShadow: 'rgba(0,0,0,0.30)',
  dotActive: 'rgba(236,234,230,0.90)',
  dotInactive: 'rgba(236,234,230,0.35)',
  shadowSm: 'rgba(0,0,0,0.15)',
  shadowMd: 'rgba(0,0,0,0.24)',
  shadowLg: 'rgba(0,0,0,0.32)',
  shadowXl: 'rgba(0,0,0,0.42)',
  overlayScrim: 'rgba(10,18,20,0.60)',
  gradientStart: 'rgba(95,184,184,0.06)',
  gradientEnd: 'transparent',
  surfaceElevated: '#141f22',
  surfaceElevatedBorder: '#1f2f33'
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
