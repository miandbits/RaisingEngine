import {immutableJson} from './immutable-json.js';
/** Browser-safe adapter; module is generated from the same native/src/runtime.cpp. */
export interface WasmCoreModule {
  HEAPU8:Uint8Array;
  _malloc(size:number):number;
  _free(pointer:number):void;
  _raising_process(pointer:number):number;
  UTF8ToString(pointer:number):string;
}
type ContentCache={contents:Map<object,string>;nextId:number};
// Adapters sharing one module must also share identifiers and the bounded cache.
const moduleCaches=new WeakMap<WasmCoreModule,ContentCache>();
export class WasmRuntime {
  private readonly cache:ContentCache;
  constructor(private readonly module:WasmCoreModule) {
    let cache=moduleCaches.get(module);
    if(!cache){cache={contents:new Map(),nextId:0};moduleCaches.set(module,cache);}
    this.cache=cache;
  }
  request(request:unknown):unknown {
    if(request&&typeof request==='object'&&'operation' in request&&['create','command','view','evaluate','inspect','replay'].includes(String(request.operation))&&'content' in request&&request.content&&typeof request.content==='object'&&immutableJson(request.content)){
      const content=request.content,cache=this.cache;
      let id=cache.contents.get(content);
      if(!id){
        if(cache.contents.size>=16){const oldest=cache.contents.entries().next().value!;this.rawRequest({operation:'releaseContent',contentId:oldest[1]});cache.contents.delete(oldest[0]);}
        id=`wasm-content-${++cache.nextId}`;
        const registered=this.rawRequest({operation:'registerContent',contentId:id,content}) as {ok:boolean};
        if(!registered.ok)return registered;
        cache.contents.set(content,id);
      }
      const {content:_content,...rest}=request as Record<string,unknown>;
      return this.rawRequest({...rest,contentId:id});
    }
    return this.rawRequest(request);
  }
  private rawRequest(request:unknown):unknown {
    const bytes=new TextEncoder().encode(JSON.stringify(request));
    if(bytes.byteLength>64*1024*1024)throw new Error('Native request exceeds 64 MiB.');
    const pointer=this.module._malloc(bytes.byteLength+1);
    if(!pointer)throw new Error('WebAssembly allocation failed.');
    try {
      this.module.HEAPU8.set(bytes,pointer);this.module.HEAPU8[pointer+bytes.byteLength]=0;
      return JSON.parse(this.module.UTF8ToString(this.module._raising_process(pointer)));
    } finally {this.module._free(pointer);}
  }
}
