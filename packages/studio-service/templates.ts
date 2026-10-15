import { mkdir, readdir, readFile, writeFile, realpath, lstat } from 'node:fs/promises';
import path from 'node:path';

export const studioTemplates = ['academy-demo', 'academy-year', 'academy-visual', 'academy-journey', 'academy-adventure'] as const;
export type StudioTemplate = typeof studioTemplates[number];

export function checkStudioTemplate(template: unknown): asserts template is StudioTemplate {
  if (!studioTemplates.includes(template as StudioTemplate)) throw new Error('알 수 없는 샘플입니다.');
}

// Electron's virtual ASAR filesystem supports reads, but native fs.cp cannot traverse it.
async function copyTemplate(source: string, target: string): Promise<void> {
  for (const entry of await readdir(source, { withFileTypes: true })) {
    if (entry.name === '.studio') continue;
    const from = path.join(source, entry.name), to = path.join(target, entry.name);
    if (entry.isDirectory()) { await mkdir(to); await copyTemplate(from, to); }
    else if (entry.isFile()) await writeFile(to, await readFile(from), { flag: 'wx' });
    else throw new Error('샘플에 지원하지 않는 파일 종류가 있습니다.');
  }
}

export async function createTemplateProject(samplesRoot: string, template: StudioTemplate, target: string): Promise<void> {
  checkStudioTemplate(template);
  const { sourceRoot, destination } = await resolveCopyPaths(path.join(samplesRoot, template), target);
  await mkdir(destination, { recursive: true });
  if ((await readdir(destination)).length) throw new Error('빈 폴더를 선택해 주세요. 기존 파일은 덮어쓰지 않았습니다.');
  await copyTemplate(sourceRoot, destination);
}

async function resolveCopyPaths(source: string, target: string): Promise<{ sourceRoot: string; destination: string }> {
  const sourceRoot = await realpath(source);
  let parent = path.resolve(target), suffix: string[] = [];
  while (true) {
    try { parent = await realpath(parent); break; }
    catch (error) {
      if ((error as NodeJS.ErrnoException).code !== 'ENOENT') throw error;
      suffix.unshift(path.basename(parent)); const next = path.dirname(parent);
      if (next === parent) throw error; parent = next;
    }
  }
  const destination = path.join(parent, ...suffix), relative = path.relative(sourceRoot, destination);
  if (!relative || (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative)))
    throw new Error('원본 프로젝트 안에는 복제할 수 없습니다. 다른 폴더를 선택해 주세요.');
  return { sourceRoot, destination };
}

// A reusable content copy: editor history, saves, analyses and unrelated root files stay behind.
export async function createProjectFromSource(source: string, target: string): Promise<void> {
  const { sourceRoot, destination } = await resolveCopyPaths(source, target);
  const files: string[] = [];
  async function collect(file: string): Promise<void> {
    const info = await lstat(path.join(sourceRoot, file));
    if (info.isSymbolicLink()) throw new Error(`링크 파일은 복제하지 않습니다: ${file}`);
    if (info.isFile()) files.push(file);
    else if (info.isDirectory()) for (const entry of await readdir(path.join(sourceRoot, file))) await collect(path.join(file, entry));
    else throw new Error(`지원하지 않는 파일 종류입니다: ${file}`);
  }
  // Preflight source before creating the destination or copying any bytes.
  for (const file of ['project.json', 'attributes.json', 'flags.json', 'assets']) await collect(file);
  JSON.parse(await readFile(path.join(sourceRoot, 'project.json'), 'utf8'));
  for (const folder of ['characters', 'activities', 'events', 'endings', 'scenarios', 'plugins']) {
    try { await collect(folder); } catch (error) { if ((error as NodeJS.ErrnoException).code !== 'ENOENT') throw error; }
  }
  await mkdir(destination, { recursive: true });
  if ((await readdir(destination)).length) throw new Error('빈 폴더를 선택해 주세요. 기존 파일은 덮어쓰지 않았습니다.');
  for (const file of files) {
    const to = path.join(destination, file); await mkdir(path.dirname(to), { recursive: true });
    await writeFile(to, await readFile(path.join(sourceRoot, file)), { flag: 'wx' });
  }
}
