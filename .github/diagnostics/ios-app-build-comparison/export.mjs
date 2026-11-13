import path from 'node:path';
import {cp} from 'node:fs/promises';
import {QtStudioService} from '../../../apps/qt-studio/service.mjs';

const root = path.resolve('.cache/ios-app-build-comparison/starter');
const destination = path.resolve('.cache/ios-app-build-comparison/prepared');
await cp('examples/my-first-raising-game', root, {recursive:true});
const service = new QtStudioService();
try {
  await service.handle({method:'open', params:{root}});
  const {exported} = await service.handle({method:'export', params:{
    revision:service.editor.revision, target:'ios', destination,
  }});
  if (!exported.xcodeProject || exported.warning) {
    throw Error(exported.warning ?? 'Xcode project missing');
  }
} finally {
  await service.editor.close();
}
