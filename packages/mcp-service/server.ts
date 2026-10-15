import { McpServer } from '@modelcontextprotocol/server';
import * as z from 'zod/v4';
import { McpWorkspace, toolError } from './workspace.js';

const version = z.string().regex(/^project-v1:[0-9a-f]{64}$/);
const file = z.string().min(1).max(256);
const id = z.string().regex(/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/).max(64);
const edit = z.discriminatedUnion('type', [
  z.strictObject({ type: z.literal('replace'), file, data: z.json() }),
  z.strictObject({ type: z.literal('add'), kind: z.enum(['activity', 'event', 'ending', 'character']), id }),
  z.strictObject({ type: z.literal('delete'), file }),
  z.strictObject({ type: z.literal('create'), file, data: z.json() }),
]);

export function createMcpServer(root: string, readOnly = false, workerUrl?: URL, hostOptions: { pluginStore?: string } = {}): McpServer {
  const server = new McpServer({ name: 'raising-engine', version: '0.1.0' });
  const workspace = new McpWorkspace(root, readOnly, workerUrl, hostOptions.pluginStore);
  // Serialize calls from a single client; cross-process writers use commitProject's lock.
  let queue: Promise<unknown> = Promise.resolve();
  function call(action: () => Promise<Record<string, unknown>>) {
    const next = queue.then(async () => {
      let output: Record<string, unknown>;
      try { output = await action(); } catch (error) { output = toolError(error); }
      return { content: [{ type: 'text' as const, text: JSON.stringify(output) }], structuredContent: output, isError: output.ok === false };
    });
    queue = next.catch(() => {});
    return next;
  }
  const read = { readOnlyHint: true, destructiveHint: false, openWorldHint: false };
  server.registerTool('read_project', {
    description: 'List project documents, diagnostics, and the current opaque version. Read again and review changes after VERSION_CONFLICT; never blindly retry a stale replacement.',
    inputSchema: z.strictObject({}), annotations: read,
  }, () => call(() => workspace.readProject()));
  server.registerTool('read_document', {
    description: 'Read a registered JSON document at the expected project version. Paths are project-relative.',
    inputSchema: z.strictObject({ file, expectedVersion: version }), annotations: read,
  }, args => call(() => workspace.readDocument(args.file, args.expectedVersion)));
  server.registerTool('get_schema', {
    description: 'Read the same authoritative JSON Schema used by the editor. References resolve to other schemas returned by this tool. name "extension" alone is the plugin document envelope; with plugin + type it is that plugin\'s data schema for the version pinned in plugins/lock.json.',
    inputSchema: z.strictObject({ name: z.enum(['common', 'project', 'attributes', 'flags', 'character', 'activity', 'event', 'ending', 'assets', 'scenario', 'extension', 'plugin-lock']), plugin: id.optional(), type: id.optional() }), annotations: read,
  }, args => call(() => workspace.readSchema(args.name, args.plugin, args.type)));
  server.registerTool('validate_project', {
    description: 'Validate structures, references, ranges, narrative flow and assets using the editor snapshot service. Returns file, JSON Pointer, line and column diagnostics.',
    inputSchema: z.strictObject({ expectedVersion: version }), annotations: read,
  }, args => call(() => workspace.validate(args.expectedVersion)));
  server.registerTool('apply_project_changes', {
    description: 'Save a batch through StudioEditor. All changes require the version from read_project and a reason. add creates the same template as the GUI; follow with replace to fill it. create makes a new plugin document (plugins/lock.json or plugins/<plugin>/<type>/<id>.json, full envelope) and fails if it exists. Default rejects all errors; allowInvalidDraft permits semantic errors only. A conflict or rejected batch writes no proposed changes. No force overwrite.',
    inputSchema: z.strictObject({ expectedVersion: version, edits: z.array(edit).min(1).max(100), reason: z.string().trim().min(1).max(1000), allowInvalidDraft: z.boolean().default(false) }),
    annotations: { readOnlyHint: false, destructiveHint: true, idempotentHint: false, openWorldHint: false },
  }, args => call(() => workspace.apply(args.expectedVersion, args.edits, args.reason, args.allowInvalidDraft)));
  server.registerTool('run_scenario', {
    description: 'Run an existing scenario with the deterministic runtime. Returns assertions and final state. No project or save files are written.',
    inputSchema: z.strictObject({ expectedVersion: version, scenarioId: id }), annotations: read,
  }, args => call(() => workspace.runScenario(args.expectedVersion, args.scenarioId)));
  server.registerTool('explain_events', {
    description: 'Replay a scenario up to afterCommands accepted CORE commands (not scenario commands). Explain event window, completion, requirements and priority with actual/expected values. afterCommands=0 means initial state; decisions are actionable only at period-end.',
    inputSchema: z.strictObject({ expectedVersion: version, scenarioId: id, afterCommands: z.number().int().min(0).max(100000) }), annotations: read,
  }, args => call(() => workspace.explainEvents(args.expectedVersion, args.scenarioId, args.afterCommands)));
  server.registerTool('plugin_status', {
    description: 'Installed engine plugins and this project\'s plugins/lock.json resolution (active/disabled/missing/incompatible with diagnostics, document and element types).',
    inputSchema: z.strictObject({}), annotations: read,
  }, () => call(() => workspace.pluginStatus()));
  server.registerTool('plugin_call', {
    description: 'Run a plugin service op for a locked, enabled plugin (e.g. cutout-animation: validate, sample, template, edit). Never writes files: apply returned data with apply_project_changes using the same expectedVersion.',
    inputSchema: z.strictObject({ expectedVersion: version, plugin: id, op: z.string().regex(/^[a-zA-Z][a-zA-Z0-9]{0,63}$/), payload: z.record(z.string(), z.json()).default({}) }), annotations: read,
  }, args => call(() => workspace.pluginCall(args.expectedVersion, args.plugin, args.op, args.payload)));
  server.registerTool('plugin_import', {
    description: 'Run a plugin import hook (e.g. cutout-animation op importLayers: a layered PSD/PSB file or a PNG folder into a character rig; payload {file, id? | rig?, origin?, trim?, scale?, exclude?}). dryRun returns the preview (layer tree, bone/slot/attachment mapping, warnings, changes). Otherwise the content-hashed PNG assets and plugin documents save in one version-checked transaction. Read again after VERSION_CONFLICT.',
    inputSchema: z.strictObject({ expectedVersion: version, plugin: id, op: z.string().regex(/^[a-zA-Z][a-zA-Z0-9]{0,63}$/), payload: z.record(z.string(), z.json()).default({}), dryRun: z.boolean().default(false), reason: z.string().min(1).max(500).default('플러그인 가져오기') }), annotations: { readOnlyHint: false, destructiveHint: false, openWorldHint: false },
  }, args => call(() => workspace.pluginImport(args.expectedVersion, args.plugin, args.op, args.payload, args.dryRun, args.reason)));
  const jobId = z.string().uuid();
  server.registerTool('search_content', { description: 'Search the same item index as Studio, including individual attributes and flags. Returns keys for relationship lookup.', inputSchema: z.strictObject({ expectedVersion: version, query: z.strictObject({ kind: z.string().optional(), text: z.string().optional(), tag: z.string().optional(), person: z.string().optional(), period: z.number().int().min(0).optional(), issuesOnly: z.boolean().optional(), sort: z.enum(['label','id','period','priority']).optional(), descending: z.boolean().optional() }).default({}), offset: z.number().int().min(0).default(0), limit: z.number().int().min(1).max(200).default(100) }), annotations: read }, a => call(() => workspace.searchContent(a.expectedVersion, JSON.parse(JSON.stringify(a.query)), a.offset, a.limit)));
  server.registerTool('content_relationships', { description: 'Bounded direct/read/write relation graph. A relation is not proof of reachability; AND/OR/NOT context and JSON pointers are retained.', inputSchema: z.strictObject({ expectedVersion: version, key: z.string(), depth: z.number().int().min(1).max(3).default(1) }), annotations: read }, a => call(() => workspace.relationships(a.expectedVersion,a.key,a.depth)));
  const options = z.strictObject({ workers: z.number().int().min(1).max(32).default(1), runs: z.number().int().min(1).max(10000).default(100), seed: z.number().int().min(0).max(0xffffffff).default(42), strategy: z.enum(['random','focus','conserve','explore']).default('random'), attributeId: z.string().optional(), maxCommands: z.number().int().min(1).max(10000).default(2000), timeoutMs: z.number().int().min(100).max(3600000).default(60000), target: z.strictObject({ kind:z.enum(['event','ending','choice']),id,nodeId:id.optional(),choiceId:id.optional() }).optional(), beamWidth:z.number().int().min(1).max(64).optional(),maxStates:z.number().int().min(1).max(100000).optional() });
  const write = { readOnlyHint:false, destructiveHint:false, openWorldHint:false };
  server.registerTool('import_sprites',{
    description:'Import rectangles from a selected absolute PNG sheet path through the same sprite service as Qt. Preserves original PNG and provenance; optional RGB color key produces transparent RGBA frames. All asset registrations save in one version-checked manifest transaction. Read again after VERSION_CONFLICT.',
    inputSchema:z.strictObject({expectedVersion:version,filePath:z.string().min(1).max(4096),prefix:id.max(56),regions:z.array(z.strictObject({x:z.number().int().min(0).max(8191),y:z.number().int().min(0).max(8191),width:z.number().int().min(1).max(8192),height:z.number().int().min(1).max(8192)})).min(1).max(256),colorKey:z.strictObject({color:z.string().regex(/^#[0-9a-f]{6}$/i),tolerance:z.number().int().min(0).max(255)}).optional()}),annotations:write
  },a=>call(()=>workspace.importSprites(a.expectedVersion,JSON.parse(JSON.stringify(a)))));
  server.registerTool('start_analysis', { description: 'Start a background batch or bounded target search on a frozen snapshot. Writes evidence under .studio/analysis. Poll analysis_status; not-found is NOT impossible.', inputSchema:z.strictObject({expectedVersion:version,options}),annotations:write }, a=>call(()=>workspace.startAnalysis(a.expectedVersion,JSON.parse(JSON.stringify(a.options)))));
  server.registerTool('analysis_status', { description:'Read shared Studio/MCP jobs, coverage and up to 30 replay traces. Reports stale disk versions.',inputSchema:z.strictObject({id:jobId.optional()}),annotations:read },a=>call(()=>workspace.analysisStatus(a.id)));
  server.registerTool('cancel_analysis',{description:'Request cooperative cancellation, retaining completed results.',inputSchema:z.strictObject({id:jobId}),annotations:write},a=>call(()=>workspace.cancelAnalysis(a.id)));
  server.registerTool('replay_analysis',{description:'Replay recorded accepted commands using the original core snapshot, verify final state when replaying the full trace.',inputSchema:z.strictObject({id:jobId,trace:z.number().int().min(0).max(29),steps:z.number().int().min(0).optional()}),annotations:read},a=>call(()=>workspace.replayAnalysis(a.id,a.trace,a.steps)));
  server.registerTool('export_analysis_scenario',{description:'Re-run a finished trace as a scenario, then save through the same version-checked editor transaction. Original content fingerprint must match.',inputSchema:z.strictObject({expectedVersion:version,id:jobId,trace:z.number().int().min(0).max(29),scenarioId:id}),annotations:write},a=>call(()=>workspace.exportAnalysis(a.expectedVersion,a.id,a.trace,a.scenarioId)));
  return server;
}
