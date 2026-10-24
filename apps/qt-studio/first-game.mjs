import { createHash, randomUUID } from 'node:crypto';
import { mkdir, readFile, writeFile, rename, realpath, lstat, rm } from 'node:fs/promises';
import path from 'node:path';
const canonical = value => Array.isArray(value) ? value.map(canonical) : value && typeof value === 'object'
  ? Object.fromEntries(Object.keys(value).sort().map(key => [key, canonical(value[key])])) : value;
const hash = value => createHash('sha256').update(JSON.stringify(canonical(value))).digest('hex');
export function firstGameSections(documents) {
  const project = documents.find(d => d.kind === 'project')?.data ?? {};
  const kinds = (...names) => documents.filter(d => names.includes(d.kind)).map(d => ({ file: d.file, data: d.data })).sort((a,b) => a.file.localeCompare(b.file));
  return { name: hash([project.title, project.description]), growth: hash(kinds('attributes', 'activity')),
    story: hash(kinds('character', 'event')), endings: hash(kinds('ending')), look: hash([project.visual ?? null, kinds('assets')]) };
}
export function firstGameChecklist(documents, progress, version, diagnostics = []) {
  if (!progress || progress.version !== 1) return null;
  const sections = firstGameSections(documents), project = documents.find(d => d.kind === 'project')?.data ?? {};
  const changed = key => sections[key] !== progress.baseline[key];
  const has = kind => documents.some(d => d.kind === kind);
  const rows = [
    ['play', 'Play your starter', 'Try a garden walk, answer Mira, and continue the season.', 'playProject', !!progress.played, true],
    ['name', 'Name your game', 'Change the title and introduction in Project settings.', 'settings', changed('name'), !!project.title],
    ['growth', 'Shape growth and activities', 'Edit a stat, activity cost, or reward. Rest should always offer a way forward.', 'activity', changed('growth'), has('activity') && has('attributes')],
    ['story', 'Tell your story', 'Edit a character or conversation; connect choices and their consequences.', 'showAuthoringWorkspace', changed('story'), has('character') && has('event')],
    ['endings', 'Choose your endings', 'Edit an ending or its requirements. Keep a fallback ending.', 'ending', changed('endings'), documents.some(d => d.kind === 'ending' && d.data.id === project.defaultEndingId)],
    ['look', 'Make the screens yours', 'Edit the title screen, colors, and text, or import your own artwork.', 'look', changed('look'), !!project.visual?.screens?.length],
    ['check', 'Validate and finish a playthrough', 'Fix reported problems, then play this version through to an ending.', 'validateProject', progress.finished === version && !diagnostics.some(d => d.severity === 'error'), !diagnostics.some(d => d.severity === 'error')],
    ['export', 'Export your game', 'Export the current version, open the output, and test it outside the editor.', 'exportGame', progress.exported === version, !!progress.finished && progress.finished === version],
  ];
  return { rows: rows.map(([id,title,description,action,complete,ready]) => ({id,title,description,action,complete,ready})),
    exported: progress.exported === version ? progress.exportDirectory ?? null : null,
    note: 'The starter is playable immediately. Customization steps are optional. Play and export checks update when the content changes.' };
}
export class FirstGameProgress {
  play = null;
  async file(root) {
    const base = await realpath(root), directory = path.join(base, '.studio');
    await mkdir(directory, {recursive:true});
    if (await realpath(directory) !== directory) throw new Error('Checklist storage must be inside the project.');
    const file = path.join(directory, 'first-game.json');
    const info = await lstat(file).catch(e => { if(e.code !== 'ENOENT') throw e; return null; });
    if (info && (!info.isFile() || info.size > 65536)) throw new Error('Invalid checklist progress file.');
    return file;
  }
  async read(editor) {
    if (!editor.root) return null;
    // Reading an ordinary project must not create local metadata.
    const file = path.join(editor.root, '.studio/first-game.json');
    try { await lstat(file); } catch (e) { if(e.code === 'ENOENT') return null; throw e; }
    const value = JSON.parse(await readFile(await this.file(editor.root), 'utf8'));
    if (value.version !== 1 || !value.baseline) throw new Error('Unsupported checklist progress.');
    return value;
  }
  async write(editor, value) {
    const file = await this.file(editor.root), temporary = file + '.' + randomUUID() + '.tmp';
    try {
      await writeFile(temporary, JSON.stringify(value, null, 2), {flag:'wx'});
      await rename(temporary, file);
    } finally { await rm(temporary, {force:true}); }
  }
  async initialize(editor) { await this.write(editor, {version:1,baseline:firstGameSections(editor.documents)}); }
  async status(editor) {
    try { return firstGameChecklist(editor.documents, await this.read(editor), editor.draftVersion(), editor.diagnostics); }
    catch (error) { return {rows:[],warning:'Checklist progress could not be read. Your game is still editable: '+error.message}; }
  }
  async observe(editor, params) {
    editor.current(params.revision);
    if (!this.play || this.play.root !== editor.root || this.play.version !== editor.draftVersion() || params.fingerprint !== this.play.fingerprint) return;
    const progress = await this.read(editor); if (!progress) return;
    if (params.phase === 'activity-result' || params.phase === 'finished') progress.played = true;
    if (params.phase === 'finished') progress.finished = this.play.version;
    await this.write(editor, progress);
  }
  async exported(editor, exported) {
    const progress = await this.read(editor); if (!progress) return;
    progress.exported = editor.draftVersion(); progress.exportDirectory = exported.directory;
    await this.write(editor, progress);
  }
}
