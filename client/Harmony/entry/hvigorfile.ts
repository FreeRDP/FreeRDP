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

export default {
  system: hapTasks,
  plugins: [customBuildInfoPlugin()]
};
