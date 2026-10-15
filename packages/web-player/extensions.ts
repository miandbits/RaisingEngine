/**
 * Web runtime extension host (ABI raising-extension-1, packages/plugin-host/CONTRACT.md §5-§8).
 * Presentation only: plugins observe committed transitions and return draw items. This host has
 * no access to the run session, so nothing an extension does can change game state.
 */
export interface ExtensionPluginBundle { id: string; version?: string; abi: string; entry: string; documents?: unknown[] }
export interface ExtensionsBundle { version: 1; plugins: ExtensionPluginBundle[] }
/** Mesh items (capability plugin-mesh-v1, CONTRACT.md §7): element-local triangles; `matrix` stays the undeformed fallback rectangle. */
export interface ExtensionMesh { positions: number[]; uvs: number[]; indices: number[] }
/** plugin-atlas-v1: the item's picture is the sub-image [x, y, width, height] (page pixels) of imageAssetId. */
export type ExtensionRegion = [number, number, number, number];
export interface ExtensionItem { imageAssetId: string; matrix: [number, number, number, number, number, number]; width: number; height: number; alpha: number; mesh?: ExtensionMesh; region?: ExtensionRegion }
export interface ExtensionElement { plugin: string; type: string; ref: string; props?: Record<string, unknown> }
/** Sound named by an `event` notification (CONTRACT.md §7 event payloads): volume 0..1, balance -1..1. */
export interface ExtensionSound { asset: string; volume: number; balance: number }
export interface ExtensionMountSpec { type: string; ref: string; width: number; height: number; props: Record<string, unknown> }
interface WasmExtensionModule {
  cwrap(name: string, returns: string | null, args: string[]): (...args: unknown[]) => unknown;
  // Heap helpers (cutout runtime 1.9.0+): large requests bypass the cwrap stack copy.
  _raising_extension_request?: (pointer: number) => number; _malloc?: (size: number) => number; _free?: (pointer: number) => void;
  stringToUTF8?: (text: string, pointer: number, size: number) => void; lengthBytesUTF8?: (text: string) => number; UTF8ToString?: (pointer: number) => string;
}
type Request = (json: string) => string;
interface MeshTopology { uvs: number[]; indices: number[] }
interface Plugin { id: string; version: string; available: boolean; error: string; request: Request | null; heap: boolean; documents: unknown[]; diagnostics: unknown[];
  frameFormat: 1 | 2; meshEpoch: number; meshes: Map<number, MeshTopology> }
interface Mount { instance: string; plugin: string; spec: ExtensionMountSpec; key: string; mounted: boolean; error: string; items: readonly ExtensionItem[]; released: boolean; frame: number }
type Reply = Record<string, unknown> & { ok?: boolean; error?: string };
export type ModuleImporter = (url: string) => Promise<{ default?: unknown }>;
// Emscripten cwrap copies string arguments onto the module stack (64 KiB by default).
const NO_ITEMS: readonly ExtensionItem[] = Object.freeze([]);
const MAX_REQUEST_BYTES = 48 * 1024;
// Through _malloc'd heap memory (no stack copy) a request may hold a large rig (`init`).
const MAX_HEAP_REQUEST_BYTES = 64 * 1024 * 1024;
type CompactItem = [string, number, number, number, number, number, number, number, number, number, ExtensionRegion | null | undefined, number | undefined, number[] | undefined];
const MAX_RECENT_EVENTS = 32;
const ENTRY = /^plugins\/[a-z][a-z0-9]*(?:-[a-z0-9]+)*\/[A-Za-z0-9._-]+\.mjs$/;

export class ExtensionHost {
  readonly plugins = new Map<string, Plugin>();
  readonly mounts = new Map<string, Mount>();
  /** Saved states of plugins missing in this build survive load→save round trips. */
  preserved: Record<string, unknown> = {};
  pendingRestore: Record<string, unknown> | null = null;
  observations = 0; clockMs = 0; lastObserve: unknown = null;
  /** `event` notifications of update replies (newest last) and the sounds they named: presentation only. */
  recentEvents: Record<string, unknown>[] = []; eventCount = 0; soundCount = 0; soundsDelivered = 0;
  /** Frame reply costs (automation reports, the large-rig benchmark): request + parse + decode. */
  timing = { frames: 0, frameMs: 0, lastFrameMs: 0, bytes: 0, lastBytes: 0 };
  private soundListener: ((sound: ExtensionSound) => void) | null = null;
  private readonly listeners = new Set<() => void>();

  /** Loads every declared plugin; any failure degrades that plugin to placeholder mode. */
  static async load(bundle: unknown, base: string, importer: ModuleImporter = url => import(/* @vite-ignore */ url) as Promise<{ default?: unknown }>): Promise<ExtensionHost> {
    const host = new ExtensionHost();
    const config = bundle as Partial<ExtensionsBundle> | undefined;
    if (!config || config.version !== 1 || !Array.isArray(config.plugins)) return host;
    for (const entry of config.plugins) {
      if (!entry || typeof entry.id !== 'string' || host.plugins.has(entry.id)) continue;
      const plugin: Plugin = { id: entry.id, version: String(entry.version ?? ''), available: false, error: '', request: null, heap: false, documents: Array.isArray(entry.documents) ? entry.documents : [], diagnostics: [],
        frameFormat: 1, meshEpoch: 0, meshes: new Map() };
      host.plugins.set(plugin.id, plugin);
      try {
        if (entry.abi !== 'raising-extension-1') throw new Error(`지원하지 않는 확장 ABI입니다: ${String(entry.abi)}`);
        if (typeof entry.entry !== 'string' || !ENTRY.test(entry.entry) || !entry.entry.startsWith(`plugins/${entry.id}/`)) throw new Error('확장 모듈 경로가 올바르지 않습니다.');
        const factory = (await importer(new URL(entry.entry, base).href)).default;
        if (typeof factory !== 'function') throw new Error('확장 모듈에 기본 팩토리가 없습니다.');
        const module = await (factory as () => Promise<WasmExtensionModule>)();
        const version = module.cwrap('raising_extension_api_version', 'number', [])();
        if (version !== 1) throw new Error(`확장 API 버전이 맞지 않습니다: ${String(version)}`);
        const call = module._raising_extension_request, malloc = module._malloc, free = module._free, write = module.stringToUTF8, size = module.lengthBytesUTF8, read = module.UTF8ToString;
        if (call && malloc && free && write && size && read) {
          plugin.heap = true;
          plugin.request = text => { const bytes = size(text) + 1, at = malloc(bytes); try { write(text, at, bytes); return read(call(at)); } finally { free(at); } };
        } else plugin.request = module.cwrap('raising_extension_request', 'string', ['string']) as Request;
        plugin.available = true;
        if (!host.init(plugin)) throw new Error(`확장 초기화 실패: ${plugin.error}`);
      } catch (error) { plugin.available = false; plugin.request = null; plugin.error = error instanceof Error ? error.message : String(error); }
    }
    return host;
  }
  get active(): boolean { return this.plugins.size > 0; }
  available(id: string): boolean { return this.plugins.get(id)?.available === true; }
  pluginError(id: string): string { return this.plugins.get(id)?.error ?? '플러그인이 없습니다.'; }
  subscribe(listener: () => void): () => void { this.listeners.add(listener); return () => { this.listeners.delete(listener); }; }
  /** The player's audio output for event sounds; null (muted) drops them. */
  setSoundListener(listener: ((sound: ExtensionSound) => void) | null): void { this.soundListener = listener; }

  /** Mounts or keeps an instance; re-mounting the same element keeps its playback state. */
  acquire(instance: string, element: ExtensionElement, width: number, height: number): boolean {
    const spec: ExtensionMountSpec = { type: element.type, ref: element.ref, width, height, props: element.props ?? {} };
    const key = JSON.stringify(spec);
    let mount = this.mounts.get(instance);
    if (mount && mount.plugin !== element.plugin) { this.unmount(mount); mount = undefined; }
    if (!mount) { mount = { instance, plugin: element.plugin, spec, key: '', mounted: false, error: '', items: NO_ITEMS, released: false, frame: 0 }; this.mounts.set(instance, mount); }
    mount.released = false;
    if (mount.mounted && mount.key === key) return true;
    mount.spec = spec; mount.key = key;
    return this.mountRequest(mount);
  }
  /** Deferred until the next tick so a React re-render (cleanup + effect) keeps the instance. */
  release(instance: string): void { const mount = this.mounts.get(instance); if (mount) mount.released = true; }
  mountError(instance: string): string { return this.mounts.get(instance)?.error ?? ''; }
  /** Called exactly once per submitted command, after its reply, never from frames. */
  observe(before: unknown, after: unknown, command: unknown, error: string | null): void {
    if (!this.active) return;
    this.observations++;
    const plugins: Record<string, unknown> = {};
    for (const plugin of this.plugins.values()) if (plugin.available) {
      const reply = this.call(plugin, { op: 'observe', before, after, command, error });
      plugins[plugin.id] = { signals: reply.signals ?? [], matched: reply.matched ?? [] };
    }
    this.lastObserve = { command, error, plugins };
  }
  /** One presentation tick: flush released mounts, update once, frame every mounted instance. */
  tick(dtMs: number): void {
    if (!this.active) return;
    for (const mount of [...this.mounts.values()]) if (mount.released) { this.unmount(mount); this.mounts.delete(mount.instance); }
    if (this.pendingRestore) { const saved = this.pendingRestore; this.pendingRestore = null; this.restore(saved); }
    const dt = Math.min(10000, Math.max(0, dtMs)); this.clockMs += dt;
    for (const plugin of this.plugins.values()) if (plugin.available) this.collectEvents(plugin.id, this.call(plugin, { op: 'update', dtMs: dt }));
    for (const mount of this.mounts.values()) {
      const plugin = this.plugins.get(mount.plugin);
      if (!mount.mounted || !plugin?.available) { mount.items = NO_ITEMS; mount.frame = 0; continue; }
      const started = performance.now();
      const size = { bytes: 0 };
      const reply = this.call(plugin, plugin.frameFormat === 2 ? { op: 'frame', instance: mount.instance, format: 2, ...(mount.frame ? { since: mount.frame } : {}) } : { op: 'frame', instance: mount.instance }, size);
      if (!reply.ok || !Array.isArray(reply.items)) { mount.items = []; mount.frame = 0; }
      else if (reply.format === 2) this.applyCompact(plugin, mount, reply);
      else { mount.items = reply.items as ExtensionItem[]; mount.frame = 0; }
      const ms = performance.now() - started;
      this.timing.frames++; this.timing.frameMs += ms; this.timing.lastFrameMs = ms; this.timing.bytes += size.bytes; this.timing.lastBytes = size.bytes;
    }
    for (const listener of this.listeners) listener();
  }
  /** Stable reference between ticks (React external-store snapshot). */
  items(instance: string): readonly ExtensionItem[] { return this.mounts.get(instance)?.items ?? NO_ITEMS; }
  /** Re-initializes plugins (new game / load) and re-mounts what is currently on screen. */
  reset(): void {
    this.preserved = {}; this.pendingRestore = null;
    for (const plugin of this.plugins.values()) if (plugin.available) this.init(plugin);
    for (const mount of this.mounts.values()) { mount.mounted = false; mount.items = NO_ITEMS; mount.frame = 0; if (!mount.released) this.mountRequest(mount); }
  }
  /** playerPresentation.extensions value, or undefined when no extension is in use. */
  save(): Record<string, unknown> | undefined {
    if (!this.active) return undefined;
    const out: Record<string, unknown> = structuredClone(this.preserved);
    for (const plugin of this.plugins.values()) if (plugin.available) { const reply = this.call(plugin, { op: 'saveAll' }); if (reply.ok && reply.state !== undefined) out[plugin.id] = reply.state; }
    return Object.keys(out).length ? out : undefined;
  }
  /** Applied after the restored screen has mounted its instances (next tick). */
  scheduleRestore(saved: Record<string, unknown> | undefined): void { this.pendingRestore = saved && typeof saved === 'object' ? saved : null; }
  restore(saved: Record<string, unknown>): void {
    for (const [id, state] of Object.entries(saved)) {
      const plugin = this.plugins.get(id);
      if (!plugin?.available) { this.preserved[id] = state; continue; }
      this.call(plugin, { op: 'restoreAll', state });
    }
  }
  describe(): unknown {
    const described: Record<string, Record<string, unknown>> = {};
    for (const plugin of this.plugins.values()) if (plugin.available) described[plugin.id] = (this.call(plugin, { op: 'describeAll' }).instances ?? {}) as Record<string, unknown>;
    return { active: this.active, observations: this.observations, clockMs: this.clockMs, lastObserve: this.lastObserve,
      events: this.recentEvents, eventCount: this.eventCount, soundCount: this.soundCount, soundsDelivered: this.soundsDelivered, timing: { ...this.timing },
      plugins: [...this.plugins.values()].map(p => ({ id: p.id, version: p.version, available: p.available, error: p.error, diagnostics: p.diagnostics, frameFormat: p.frameFormat, heap: p.heap })),
      instances: [...this.mounts.values()].map(m => ({ instance: m.instance, plugin: m.plugin, available: m.mounted && this.available(m.plugin), error: m.error, element: m.spec,
        items: m.items.map(i => ({ imageAssetId: i.imageAssetId, matrix: i.matrix, width: i.width, height: i.height, alpha: i.alpha, ...(i.mesh ? { mesh: i.mesh } : {}), ...(i.region ? { region: i.region } : {}) })), state: described[m.plugin]?.[m.instance] ?? null })) };
  }
  /**
   * Compact frame (format 2, CONTRACT.md §7): [image, a..f, width, height, alpha(, region|null(, meshId,
   * positions))] per item, 0 = unchanged since frame `since`. Unchanged items keep their object (the
   * renderer skips them); mesh uvs/indices come from the topology cached at init.
   */
  private applyCompact(plugin: Plugin, mount: Mount, reply: Reply): void {
    if (typeof reply.meshEpoch === 'number' && reply.meshEpoch !== plugin.meshEpoch) { plugin.meshes.clear(); plugin.meshEpoch = reply.meshEpoch; }
    const list = reply.items as unknown[], previous = mount.items, next: ExtensionItem[] = [];
    let complete = true;
    const unknown = new Set<number>();
    for (const [k, entry] of list.entries()) {
      if (typeof entry === 'number') { const kept = previous[k]; if (kept) next.push(kept); else complete = false; continue; }
      if (!Array.isArray(entry) || entry.length < 10 || typeof entry[0] !== 'string') continue;
      const e = entry as CompactItem;
      const item: ExtensionItem = { imageAssetId: e[0], matrix: [e[1], e[2], e[3], e[4], e[5], e[6]], width: e[7], height: e[8], alpha: e[9] };
      if (Array.isArray(e[10]) && e[10].length === 4) item.region = e[10];
      if (e.length > 12 && typeof e[11] === 'number' && Array.isArray(e[12])) {
        const topology = plugin.meshes.get(e[11]);
        if (!topology) unknown.add(e[11]);
        item.mesh = { positions: e[12], uvs: topology?.uvs ?? [], indices: topology?.indices ?? [] };
        (item as ExtensionItem & { meshId?: number }).meshId = e[11];
      }
      next.push(item);
    }
    if (unknown.size) {
      // Topology this host has not cached (a new mesh epoch): fetched once, then shared by every frame.
      const fetched = this.call(plugin, { op: 'meshes', ids: [...unknown] });
      if (fetched.ok) this.storeMeshes(plugin, fetched.meshes);
      for (const item of next) {
        const id = (item as ExtensionItem & { meshId?: number }).meshId;
        if (item.mesh && id !== undefined && !item.mesh.indices.length) { const t = plugin.meshes.get(id); if (t) item.mesh = { positions: item.mesh.positions, uvs: t.uvs, indices: t.indices }; }
      }
    }
    mount.items = next;
    // A "0" this host cannot resolve: ask for every item on the next frame.
    mount.frame = complete && typeof reply.frame === 'number' ? reply.frame : 0;
  }
  private storeMeshes(plugin: Plugin, list: unknown): void {
    if (!Array.isArray(list)) return;
    for (const entry of list as { id?: unknown; uvs?: unknown; indices?: unknown }[])
      if (entry && typeof entry.id === 'number' && Array.isArray(entry.uvs) && Array.isArray(entry.indices)) plugin.meshes.set(entry.id, { uvs: entry.uvs as number[], indices: entry.indices as number[] });
  }
  /** update reply {notifications: {instance: [..]}}: keeps `event` entries with their payload. */
  private collectEvents(pluginId: string, reply: Reply): void {
    const notes = reply.notifications;
    if (!notes || typeof notes !== 'object') return;
    for (const [instance, list] of Object.entries(notes as Record<string, unknown>)) {
      if (!Array.isArray(list)) continue;
      for (const n of list as Record<string, unknown>[]) {
        if (!n || typeof n !== 'object' || n.kind !== 'event') continue;
        this.recentEvents.push({ ...n, instance, plugin: pluginId, clockMs: this.clockMs });
        if (this.recentEvents.length > MAX_RECENT_EVENTS) this.recentEvents.shift();
        this.eventCount++;
        if (typeof n.audio !== 'string' || !n.audio) continue;
        const number = (v: unknown, fallback: number, lo: number, hi: number) => typeof v === 'number' && Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : fallback;
        this.soundCount++;
        if (!this.soundListener) continue;
        this.soundsDelivered++;
        try { this.soundListener({ asset: n.audio, volume: number(n.volume, 1, 0, 1), balance: number(n.balance, 0, -1, 1) }); } catch { /* audio never stops presentation */ }
      }
    }
  }
  private init(plugin: Plugin): boolean {
    // frameFormats: compact frames when the plugin supports them (older plugins ignore the field).
    // window.raisingExtensionFrameFormats = [1] (diagnostics, the large-rig benchmark) keeps the documented items.
    const formats = typeof window !== 'undefined' && Array.isArray((window as { raisingExtensionFrameFormats?: unknown }).raisingExtensionFrameFormats)
      ? (window as unknown as { raisingExtensionFrameFormats: number[] }).raisingExtensionFrameFormats : [2];
    const reply = this.call(plugin, { op: 'init', documents: plugin.documents, frameFormats: formats });
    if (!reply.ok) { plugin.error = String(reply.error ?? 'init'); return false; }
    plugin.diagnostics = Array.isArray(reply.diagnostics) ? reply.diagnostics : [];
    plugin.frameFormat = reply.frameFormat === 2 ? 2 : 1; plugin.meshes.clear(); plugin.meshEpoch = typeof reply.meshEpoch === 'number' ? reply.meshEpoch : 0;
    if (plugin.frameFormat === 2) this.storeMeshes(plugin, reply.meshes);
    return plugin.available;
  }
  private mountRequest(mount: Mount): boolean {
    const plugin = this.plugins.get(mount.plugin); mount.mounted = false; mount.items = NO_ITEMS; mount.frame = 0;
    if (!plugin?.available) { mount.error = plugin?.error || '플러그인이 없습니다.'; return false; }
    const reply = this.call(plugin, { op: 'mount', instance: mount.instance, element: mount.spec });
    if (!reply.ok) { mount.error = String(reply.error ?? 'mount'); return false; }
    mount.error = ''; mount.mounted = true; return true;
  }
  private unmount(mount: Mount): void {
    if (!mount.mounted) return; mount.mounted = false; mount.items = NO_ITEMS;
    const plugin = this.plugins.get(mount.plugin); if (plugin?.available) this.call(plugin, { op: 'unmount', instance: mount.instance });
  }
  /** Never throws: a broken module is disabled and the game keeps running. */
  private call(plugin: Plugin, request: unknown, size?: { bytes: number }): Reply {
    if (!plugin.available || !plugin.request) return { ok: false, error: plugin.error };
    const text = JSON.stringify(request);
    const limit = plugin.heap ? MAX_HEAP_REQUEST_BYTES : MAX_REQUEST_BYTES;
    if (text.length * 3 > limit && new TextEncoder().encode(text).length > limit) return { ok: false, error: '확장 요청이 너무 큽니다.' };
    try {
      const raw = plugin.request(text);
      if (size) size.bytes = raw.length;
      const reply = JSON.parse(raw) as unknown;
      if (!reply || typeof reply !== 'object') throw new Error('확장 모듈 응답이 올바른 JSON이 아닙니다.');
      return reply as Reply;
    } catch (error) { plugin.available = false; plugin.request = null; plugin.error = error instanceof Error ? error.message : String(error); return { ok: false, error: plugin.error }; }
  }
}
