import { hapTasks, HvigorPlugin, HvigorNode } from '@ohos/hvigor-ohos-plugin';
import * as childProcess from 'child_process';
import * as fs from 'fs';
import * as path from 'path';

function customBuildInfoPlugin(): HvigorPlugin {
  return {
    pluginId: 'customBuildInfoPlugin',
    apply(node: HvigorNode) {
      node.registerTask({
        name: 'generateBuildInfo',
        run: () => {
          try {
            const gitDescribe = childProcess.execSync('git describe --tags --always --dirty', { encoding: 'utf-8' }).trim();
            const buildInfoPath = path.join(__dirname, 'src/main/ets/utils/BuildInfo.ets');
            const content = `export const BUILD_VERSION: string = '${gitDescribe}';\n`;
            fs.writeFileSync(buildInfoPath, content, { encoding: 'utf-8' });
            console.log(`[customBuildInfoPlugin] Generated BuildInfo.ets with version: ${gitDescribe}`);
          } catch (e) {
            console.warn('[customBuildInfoPlugin] Failed to generate build info:', e);
          }
        },
        dependencies: [],
        postDependencies: ['default@PreBuild']
      });
    }
  }
}

function syncAgreementDocsPlugin(): HvigorPlugin {
  return {
    pluginId: 'syncAgreementDocsPlugin',
    apply(node: HvigorNode) {
      node.registerTask({
        name: 'syncAgreementDocs',
        run: () => {
          try {
            const syncScriptPath = path.resolve(__dirname, '../scripts/sync_agreements.py');
            const targetDir = path.join(__dirname, 'src/main/resources/rawfile');
            childProcess.execFileSync('python3', [syncScriptPath, targetDir], { stdio: 'inherit' });
          } catch (e) {
            console.warn('[syncAgreementDocsPlugin] Failed to sync agreement docs:', e);
            throw e;
          }
        },
        dependencies: [],
        postDependencies: ['default@PreBuild']
      });
    }
  }
}

export default {
  system: hapTasks,
  plugins: [customBuildInfoPlugin(), syncAgreementDocsPlugin()]
};
