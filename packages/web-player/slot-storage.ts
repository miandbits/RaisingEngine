/** A transaction owns read/compare/write so another tab cannot bypass confirmation. */
export interface StoredSlot {automatic:boolean;index:number;text:string}
export interface SlotStorage {
  read():Promise<StoredSlot[]>;
  update(change:(rows:readonly StoredSlot[])=>StoredSlot|null):Promise<void>;
}
export class IndexedSlotStorage implements SlotStorage {
  constructor(readonly namespace:string,private readonly factory:IDBFactory|undefined=globalThis.indexedDB){}
  private open():Promise<IDBDatabase>{return new Promise((resolve,reject)=>{
    if(!this.factory){reject(new Error('Browser save storage is unavailable. Export a backup file.'));return;}
    const request=this.factory.open('raising-save-slots-v1',1);let failed=false;
    request.onupgradeneeded=()=>{const db=request.result;if(!db.objectStoreNames.contains('slots'))db.createObjectStore('slots',{keyPath:'id'}).createIndex('namespace','namespace');};
    request.onerror=()=>reject(request.error??new Error('Browser save storage unavailable.'));
    request.onblocked=()=>{failed=true;reject(new Error('Close another game tab and retry browser storage.'));};
    request.onsuccess=()=>{if(failed)request.result.close();else resolve(request.result);};
  });}
  private async transaction(change?: (rows:readonly StoredSlot[])=>StoredSlot|null):Promise<StoredSlot[]>{
    const db=await this.open();return new Promise((resolve,reject)=>{
      const tx=db.transaction('slots',change?'readwrite':'readonly'),store=tx.objectStore('slots');let rows:StoredSlot[]=[],failure:unknown;
      tx.oncomplete=()=>{db.close();resolve(rows);};tx.onabort=()=>{db.close();reject(failure??tx.error??new Error('Browser save transaction failed.'));};tx.onerror=()=>{};
      const read=store.index('namespace').getAll(this.namespace);
      read.onsuccess=()=>{try{rows=read.result.map(({automatic,index,text})=>({automatic,index,text}));const next=change?.(rows);if(next)store.put({...next,namespace:this.namespace,id:this.namespace+'\0'+(next.automatic?'auto-':'manual-')+next.index});}catch(error){failure=error;tx.abort();}};
    });
  }
  read(){return this.transaction();}
  async update(change:(rows:readonly StoredSlot[])=>StoredSlot|null){await this.transaction(change);}
}
