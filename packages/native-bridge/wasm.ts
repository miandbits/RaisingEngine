/** Browser-safe adapter; module is generated from the same native/src/runtime.cpp. */
export interface WasmCoreModule {
  HEAPU8:Uint8Array;
  _malloc(size:number):number;
  _free(pointer:number):void;
  _raising_process(pointer:number):number;
  UTF8ToString(pointer:number):string;
}
export class WasmRuntime {
  constructor(private readonly module:WasmCoreModule) {}
  request(request:unknown):unknown {
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
