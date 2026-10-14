import path from 'node:path';
import { dataValidators, loadService, type ServiceDiagnostic } from '../plugin-host/service.js';
import { resolveProjectPlugins, type ResolvedPlugin } from '../plugin-host/resolve.js';
import { pluginStorePath } from '../plugin-host/store.js';
import { extensionPath, pluginElements } from '../plugin-host/bundle.js';
import { isWithin } from '../project-service/loader.js';
import { childPointer, diagnostic, isObject, type Diagnostic, type JsonObject, type LoadedProject, type ProjectDocument } from './model.js';

export interface PluginValidation { diagnostics: Diagnostic[]; rejected: Set<ProjectDocument> }
const CODE = /^[A-Z][A-Z0-9_]{0,63}$/;

/**
 * Plugin documents and plugin screen elements (CONTRACT §3, §5). Unavailable plugins only
 * produce diagnostics; their documents are never dropped. `rejected` lists extension
 * documents whose `data` fails the plugin schema: like engine schema errors they block save.
 */
export async function validatePlugins(project: LoadedProject, documents: ProjectDocument[]): Promise<PluginValidation> {
  const result: PluginValidation = { diagnostics: [], rejected: new Set() };
  const lockDoc = documents.find(d => d.kind === 'pluginLock');
  const extensions = documents.filter(d => d.kind === 'extension');
  const root = documents.find(d => d.kind === 'project');
  const visual = isObject(root?.data) ? root.data.visual : undefined;
  const elements = pluginElements(visual);
  // Plugin-free projects never touch the store (and stay byte-identical in every output).
  if (!lockDoc && !extensions.length && !elements.length && !hasStrayExtension(visual)) return result;
  const report = (doc: ProjectDocument, pointer: string, code: string, message: string, ids: string[] = [], severity: 'error' | 'warning' = 'error') =>
    result.diagnostics.push(diagnostic(doc, pointer, code, message, ids, severity));
  if (root) for (const [i, screen] of (isObject(visual) && Array.isArray(visual.screens) ? visual.screens : []).entries())
    for (const [j, element] of (isObject(screen) && Array.isArray(screen.elements) ? screen.elements : []).entries())
      if (isObject(element) && element.kind !== 'plugin' && element.extension !== undefined)
        report(root, `/visual/screens/${i}/elements/${j}/extension`, 'UI_EXTENSION_KIND', '플러그인 연결(extension)은 플러그인 요소에만 설정할 수 있습니다.');

  const resolved = await resolveProjectPlugins(pluginStorePath(project.pluginStore), lockDoc?.data ?? null);
  if (lockDoc) for (const plugin of resolved) for (const issue of plugin.diagnostics) report(lockDoc, issue.pointer, issue.code, issue.message, [plugin.id], issue.severity);
  const find = (id: unknown) => resolved.find(p => p.id === id && p.diagnostics.every(d => d.code !== 'PLUGIN_LOCK_DUPLICATE'));

  const accepted = new Map<ResolvedPlugin, ProjectDocument[]>();
  for (const doc of extensions) {
    const data = doc.data as JsonObject;
    const plugin = String(data.plugin), type = String(data.type), id = String(data.id);
    if (doc.file !== extensionPath(plugin, type, id)) {
      const parts = doc.file.split('/'), field = parts[1] !== plugin ? 'plugin' : parts[2] !== type ? 'type' : 'id';
      report(doc, `/${field}`, 'EXTENSION_PATH', `플러그인 문서 경로는 '${extensionPath(plugin, type, id)}'여야 합니다(현재 '${doc.file}').`, [id]);
    }
    const resolvedPlugin = find(plugin);
    if (!resolvedPlugin) { report(doc, '/plugin', 'PLUGIN_NOT_LOCKED', `플러그인 '${plugin}'가 잠금 파일(plugins/lock.json)에 없습니다. 문서는 검사하지 않고 그대로 보존합니다.`, [plugin], 'warning'); continue; }
    if (resolvedPlugin.state !== 'active') continue;
    const manifest = resolvedPlugin.manifest!;
    if (!manifest.documentTypes.some(t => t.type === type)) { report(doc, '/type', 'EXTENSION_TYPE_UNKNOWN', `플러그인 '${plugin}' ${manifest.version}에 문서 종류 '${type}'가 없습니다.`, [id]); continue; }
    let validate;
    try { validate = (await dataValidators({ directory: resolvedPlugin.directory!, manifest })).get(type)!; }
    catch (error) { report(doc, '/type', 'PLUGIN_SERVICE_ERROR', `플러그인 스키마를 읽을 수 없습니다: ${error instanceof Error ? error.message : String(error)}`, [plugin]); continue; }
    if (!validate(data.data)) {
      result.rejected.add(doc);
      for (const error of validate.errors ?? []) {
        let pointer = `/data${error.instancePath}`;
        if (error.keyword === 'additionalProperties') pointer = childPointer(pointer, String(error.params.additionalProperty));
        if (error.keyword === 'required') pointer = childPointer(pointer, String(error.params.missingProperty));
        report(doc, pointer, 'SCHEMA_INVALID', `플러그인 스키마 규칙 위반 (${error.keyword}): ${error.message ?? 'invalid data'}; ${JSON.stringify(error.params)}`, [id]);
      }
      continue;
    }
    accepted.set(resolvedPlugin, [...accepted.get(resolvedPlugin) ?? [], doc]);
  }

  const usedElements = new Map<ResolvedPlugin, typeof elements>();
  for (const item of elements) {
    const ext = item.element.extension;
    if (!root || !ext) continue;
    const plugin = find(ext.plugin), at = `${item.pointer}/extension`;
    if (!plugin) { report(root, `${at}/plugin`, 'PLUGIN_NOT_LOCKED', `화면 요소의 플러그인 '${String(ext.plugin)}'가 잠금 파일에 없습니다. 자리표시로 보입니다.`, [String(ext.plugin)], 'warning'); continue; }
    if (plugin.state === 'disabled' || plugin.state === 'missing') {
      report(root, `${at}/plugin`, plugin.state === 'disabled' ? 'PLUGIN_DISABLED' : 'PLUGIN_MISSING', `플러그인 '${plugin.id}'를 사용할 수 없어 화면 요소 '${String(item.element.id)}'는 자리표시로 보입니다. 내보내기 전에 설치하거나 켜세요.`, [plugin.id], 'warning');
      continue;
    }
    if (plugin.state !== 'active') continue;
    const elementType = plugin.manifest!.elementTypes.find(t => t.type === ext.type);
    if (!elementType) { report(root, `${at}/type`, 'PLUGIN_ELEMENT_TYPE', `플러그인 '${plugin.id}'에 화면 요소 종류 '${String(ext.type)}'가 없습니다.`, [plugin.id]); continue; }
    usedElements.set(plugin, [...usedElements.get(plugin) ?? [], item]);
  }

  const assetsDoc = documents.find(d => d.kind === 'assets');
  const assets = (isObject(assetsDoc?.data) && Array.isArray(assetsDoc.data.assets) ? assetsDoc.data.assets : []).filter(isObject).map(asset => {
    const absolute = path.resolve(project.root, String(asset.path));
    return { id: String(asset.id), kind: String(asset.kind), path: String(asset.path), absolutePath: isWithin(project.root, absolute) ? absolute : null };
  });
  for (const plugin of resolved.filter(p => p.state === 'active' && (accepted.has(p) || usedElements.has(p)))) {
    const docs = accepted.get(plugin) ?? [], items = usedElements.get(plugin) ?? [];
    let service;
    try { service = await loadService({ directory: plugin.directory!, manifest: plugin.manifest! }); }
    catch (error) {
      if (lockDoc) report(lockDoc, plugin.pointer, 'PLUGIN_SERVICE_ERROR', `플러그인 '${plugin.id}' 서비스 모듈을 불러올 수 없습니다: ${error instanceof Error ? error.message : String(error)}`, [plugin.id]);
      continue;
    }
    if (!service?.validate) {
      // Without a service the engine can still check that element refs name a document.
      if (root) for (const item of items) {
        const refType = plugin.manifest!.elementTypes.find(t => t.type === item.element.extension?.type)?.refType;
        if (refType && !docs.some(d => (d.data as JsonObject).type === refType && (d.data as JsonObject).id === item.element.extension?.ref))
          report(root, `${item.pointer}/extension/ref`, 'PLUGIN_ELEMENT_REF', `플러그인 문서 ${refType} '${String(item.element.extension?.ref)}'가 없습니다.`, [String(item.element.extension?.ref)]);
      }
      continue;
    }
    let issues: ServiceDiagnostic[];
    try {
      issues = await service.validate(structuredClone({
        documents: docs.map(d => { const data = d.data as JsonObject; return { file: d.file, type: String(data.type), id: String(data.id), data: data.data }; }),
        assets, elements: items.map(i => ({ file: root!.file, pointer: i.pointer, element: i.element })),
      }));
      if (!Array.isArray(issues)) throw new Error('validate는 진단 배열을 돌려줘야 합니다.');
    } catch (error) {
      if (lockDoc) report(lockDoc, plugin.pointer, 'PLUGIN_SERVICE_ERROR', `플러그인 '${plugin.id}' 검사 중 오류: ${error instanceof Error ? error.message : String(error)}`, [plugin.id]);
      continue;
    }
    for (const issue of issues.slice(0, 1000)) {
      const target = documents.find(d => d.file === issue?.file) ?? lockDoc ?? root;
      if (!target || !issue || typeof issue !== 'object') continue;
      const code = typeof issue.code === 'string' && CODE.test(issue.code) ? issue.code : 'PLUGIN_DIAGNOSTIC';
      const pointer = typeof issue.pointer === 'string' && (issue.pointer === '' || issue.pointer.startsWith('/')) ? issue.pointer : '';
      report(target, pointer, code, String(issue.message ?? '').slice(0, 1000) || code,
        Array.isArray(issue.relatedIds) ? issue.relatedIds.filter(x => typeof x === 'string').slice(0, 16) : [], issue.severity === 'warning' ? 'warning' : 'error');
    }
  }
  return result;
}

function hasStrayExtension(visual: unknown): boolean {
  return isObject(visual) && Array.isArray(visual.screens) && visual.screens.some(s => isObject(s) && Array.isArray(s.elements) && s.elements.some(e => isObject(e) && e.extension !== undefined));
}
