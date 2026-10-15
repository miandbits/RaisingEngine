/**
 * Web runtime extension host (ABI raising-extension-1, packages/plugin-host/CONTRACT.md §5-§8).
 * Presentation only: plugins observe committed transitions and return draw items. This host has
 * no access to the run session, so nothing an extension does can change game state.
 */
export interface ExtensionPluginBundle { id: string; version?: string; abi: string; entry: string; documents?: unknown[] }
export interface ExtensionsBundle { version: 1; plugins: ExtensionPluginBundle[] }
/** Mesh items (capability plugin-mesh-v1, CONTRACT.md §7): element-local triangles; `matrix` stays the undeformed fallback rectangle. */
export interface ExtensionMesh { positions: number[]; uvs: number[]; indices: number[] }
export interface ExtensionItem { imageAssetId: string; matrix: [number, number, number, number, number, number]; width: number; height: number; alpha: number; mesh?: ExtensionMesh }
export interface ExtensionElement { plugin: string; type: string; ref: string; props?: Record<string, unknown> }
/** Sound named by an `event` notification (CONTRACT.md §7 event payloads): volume 0..1, balance -1..1. */
export interface ExtensionSound { asset: string; volume: number; balance: number }
export interface ExtensionMountSpec { type: string; ref: string; width: number; height: number; props: Record<string, unknown> }
interface WasmExtensionModule { cwrap(name: string, returns: string | null, args: string[]): (...args: unknown[]) => unknown }
type Request = (json: string) => string;
interface Plugin { id: string; version: string; available: boolean; error: string; request: Request | null; documents: unknown[]; diagnostics: unknown[] }
interface Mount { instance: string; plugin: string; spec: ExtensionMountSpec; key: string; mounted: boolean; error: string; items: readonly ExtensionItem[]; released: boolean }
type Reply = Record<string, unknown> & { ok?: boolean; error?: string };
export type ModuleImporter = (url: string) => Promise<{ default?: unknown }>;
// Emscripten cwrap copies string arguments onto the module stack (64 KiB by default).
const NO_ITEMS: readonly ExtensionItem[] = Object.freeze([]);
const MAX_REQUEST_BYTES = 48 * 1024;
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
  private soundListener: ((sound: ExtensionSound) => void) | null = null;
  private readonly listeners = new Set<() => void>();

  /** Loads every declared plugin; any failure degrades that plugin to placeholder mode. */
  static async load(bundle: unknown, base: string, importer: ModuleImporter = url => import(/* @vite-ignore */ url) as Promise<{ default?: unknown }>): Promise<ExtensionHost> {
    const host = new ExtensionHost();
    const config = bundle as Partial<ExtensionsBundle> | undefined;
    if (!config || config.version !== 1 || !Array.isArray(config.plugins)) return host;
    for (const entry of config.plugins) {
      if (!entry || typeof entry.id !== 'string' || host.plugins.has(entry.id)) continue;
      const plugin: Plugin = { id: entry.id, version: String(entry.version ?? ''), available: false, error: '', request: null, documents: Array.isArray(entry.documents) ? entry.documents : [], diagnostics: [] };
      host.plugins.set(plugin.id, plugin);
      try {
        if (entry.abi !== 'raising-extension-1') throw new Error(`지원하지 않는 확장 ABI입니다: ${String(entry.abi)}`);
        if (typeof entry.entry !== 'string' || !ENTRY.test(entry.entry) || !entry.entry.startsWith(`plugins/${entry.id}/`)) throw new Error('확장 모듈 경로가 올바르지 않습니다.');
        const factory = (await importer(new URL(entry.entry, base).href)).default;
        if (typeof factory !== 'function') throw new Error('확장 모듈에 기본 팩토리가 없습니다.');
        const module = await (factory as () => Promise<WasmExtensionModule>)();
        const version = module.cwrap('raising_extension_api_version', 'number', [])();
        if (version !== 1) throw new Error(`확장 API 버전이 맞지 않습니다: ${String(version)}`);
        plugin.request = module.cwrap('raising_extension_request', 'string', ['string']) as Request;
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
    if (!mount) { mount = { instance, plugin: element.plugin, spec, key: '', mounted: false, error: '', items: NO_ITEMS, released: false }; this.mounts.set(instance, mount); }
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
      if (!mount.mounted || !plugin?.available) { mount.items = NO_ITEMS; continue; }
      const reply = this.call(plugin, { op: 'frame', instance: mount.instance });
      mount.items = reply.ok && Array.isArray(reply.items) ? reply.items as ExtensionItem[] : [];
    }
    for (const listener of this.listeners) listener();
  }
  /** Stable reference between ticks (React external-store snapshot). */
  items(instance: string): readonly ExtensionItem[] { return this.mounts.get(instance)?.items ?? NO_ITEMS; }
  /** Re-initializes plugins (new game / load) and re-mounts what is currently on screen. */
  reset(): void {
    this.preserved = {}; this.pendingRestore = null;
    for (const plugin of this.plugins.values()) if (plugin.available) this.init(plugin);
    for (const mount of this.mounts.values()) { mount.mounted = false; mount.items = NO_ITEMS; if (!mount.released) this.mountRequest(mount); }
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
      events: this.recentEvents, eventCount: this.eventCount, soundCount: this.soundCount, soundsDelivered: this.soundsDelivered,
      plugins: [...this.plugins.values()].map(p => ({ id: p.id, version: p.version, available: p.available, error: p.error, diagnostics: p.diagnostics })),
      instances: [...this.mounts.values()].map(m => ({ instance: m.instance, plugin: m.plugin, available: m.mounted && this.available(m.plugin), error: m.error, element: m.spec,
        items: m.items.map(i => ({ imageAssetId: i.imageAssetId, matrix: i.matrix, width: i.width, height: i.height, alpha: i.alpha, ...(i.mesh ? { mesh: i.mesh } : {}) })), state: described[m.plugin]?.[m.instance] ?? null })) };
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
    const reply = this.call(plugin, { op: 'init', documents: plugin.documents });
    if (!reply.ok) { plugin.error = String(reply.error ?? 'init'); return false; }
    plugin.diagnostics = Array.isArray(reply.diagnostics) ? reply.diagnostics : []; return plugin.available;
  }
  private mountRequest(mount: Mount): boolean {
    const plugin = this.plugins.get(mount.plugin); mount.mounted = false; mount.items = NO_ITEMS;
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
  private call(plugin: Plugin, request: unknown): Reply {
    if (!plugin.available || !plugin.request) return { ok: false, error: plugin.error };
    const text = JSON.stringify(request);
    if (text.length * 3 > MAX_REQUEST_BYTES && new TextEncoder().encode(text).length > MAX_REQUEST_BYTES) return { ok: false, error: '확장 요청이 너무 큽니다.' };
    try {
      const reply = JSON.parse(plugin.request(text)) as unknown;
      if (!reply || typeof reply !== 'object') throw new Error('확장 모듈 응답이 올바른 JSON이 아닙니다.');
      return reply as Reply;
    } catch (error) { plugin.available = false; plugin.request = null; plugin.error = error instanceof Error ? error.message : String(error); return { ok: false, error: plugin.error }; }
  }
}
