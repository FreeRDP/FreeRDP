import { appTasks } from '@ohos/hvigor-ohos-plugin';
import * as fs from 'fs';
import * as path from 'path';
import * as childProcess from 'child_process';

function getGitVersionName(): string {
  try {
    return childProcess.execSync('git describe --tags --always --dirty', { encoding: 'utf-8' }).trim();
  } catch (e) {
    return '';
  }
}

function isValidSemver(version: string): boolean {
  return /^\d+\.\d+\.\d+/.test(version.replace(/^v/, ''));
}

function getGitVersionCode(versionName: string): number {
  try {
    const cleanVersionName = versionName.replace(/^v/, '').split('-')[0];
    const parts = cleanVersionName.split('.');
    const major = parseInt(parts[0]) || 1;
    const minor = parseInt(parts[1]) || 0;
    const patch = parseInt(parts[2]) || 0;

    let commitCount = 0;
    if (versionName.includes('-')) {
      const splits = versionName.split('-');
      if (splits.length > 1) {
        commitCount = parseInt(splits[1]) || 0;
      }
    }

    let versionCode = major * 1000000 + minor * 100000 + patch * 10000 + commitCount;
    if (versionCode > 2147483647) {
      versionCode = major * 1000000 + minor * 100000 + patch * 10000;
    }
    return versionCode;
  } catch (e) {
    return 1000000;
  }
}

function readAppJson5(): { versionName: string; versionCode: number } {
  try {
    const appJson5Path = path.resolve(__dirname, 'AppScope', 'app.json5');
    const content = fs.readFileSync(appJson5Path, 'utf-8');
    const vnMatch = content.match(/"versionName"\s*:\s*"([^"]+)"/);
    const vcMatch = content.match(/"versionCode"\s*:\s*(\d+)/);
    return {
      versionName: vnMatch ? vnMatch[1] : '1.0.0',
      versionCode: vcMatch ? parseInt(vcMatch[1], 10) : 1000000
    };
  } catch (e) {
    return { versionName: '1.0.0', versionCode: 1000000 };
  }
}

const appConfig = readAppJson5();
let gitVersionName = getGitVersionName();
let gitVersionCode: number;

if (gitVersionName && isValidSemver(gitVersionName)) {
  gitVersionCode = getGitVersionCode(gitVersionName);
} else {
  console.log(`[Build] Git version "${gitVersionName || 'unavailable'}" not a valid semver, using app.json5 fallback`);
  gitVersionName = appConfig.versionName;
  gitVersionCode = appConfig.versionCode;
}

console.log(`[Build] Dynamic App Version: ${gitVersionName} (${gitVersionCode})`);

function getSigningConfig() {
  const propertiesPath = path.resolve(__dirname, 'local.properties');
  let props: Record<string, string> = {};

  if (fs.existsSync(propertiesPath)) {
    const content = fs.readFileSync(propertiesPath, 'utf-8');
    content.split('\n').forEach((line: string) => {
      if (line && !line.trim().startsWith('#')) {
        const [key, ...valueParts] = line.split('=');
        if (key && valueParts.length > 0) {
          props[key.trim()] = valueParts.join('=').trim();
        }
      }
    });
  }

  const buildAllModeFile = path.resolve(__dirname, '.build-signing-mode');
  let buildAllMode: string | undefined;
  if (fs.existsSync(buildAllModeFile)) {
    buildAllMode = fs.readFileSync(buildAllModeFile, 'utf8').trim();
  }

  const signActiveFromEnv = process.env.SIGN_ACTIVE;
  const defaultSignActive = props['sign.active'] || 'debug';

  let activeMode: string;
  let modeSource: string;
  if (buildAllMode) {
    activeMode = buildAllMode;
    modeSource = 'build script';
  } else if (signActiveFromEnv) {
    activeMode = signActiveFromEnv;
    modeSource = 'SIGN_ACTIVE env';
  } else {
    activeMode = defaultSignActive;
    modeSource = 'sign.active in local.properties';
  }

  const prefix = `sign.${activeMode}.`;
  console.log(`[Build] Signing config - activeMode: ${activeMode} (source: ${modeSource})`);

  if (!props[`${prefix}storeFile`]) {
    console.warn(`[Build] No signing config found for mode "${activeMode}", using default debug cert if available`);
    return undefined;
  }

  return {
    type: "HarmonyOS",
    material: {
      certpath: props[`${prefix}certpath`] || '',
      keyAlias: props[`${prefix}keyAlias`] || '',
      keyPassword: props[`${prefix}keyPassword`] || '',
      profile: props[`${prefix}profile`] || '',
      signAlg: "SHA256withECDSA",
      storeFile: props[`${prefix}storeFile`] || '',
      storePassword: props[`${prefix}storePassword`] || ''
    }
  };
}

const signingConfig = getSigningConfig();

export default {
  system: appTasks,
  plugins: [],
  config: {
    ohos: {
      overrides: {
        appOpt: {
          versionCode: gitVersionCode,
          versionName: gitVersionName
        },
        signingConfig: signingConfig
      }
    }
  }
};
