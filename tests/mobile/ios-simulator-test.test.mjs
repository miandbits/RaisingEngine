import assert from 'node:assert/strict';
import test from 'node:test';
import {cleanUpIOSSimulator,runIOSSimulatorCases,selectIOSSimulator} from '../../scripts/lib/ios-simulator-test.mjs';

const runtimeId=version=>'com.apple.CoreSimulator.SimRuntime.iOS-'+version.replaceAll('.','-');
const deviceId=name=>'com.apple.CoreSimulator.SimDeviceType.'+name;
const runtime=(version,names,{identifier=runtimeId(version),isAvailable=true}={})=>({
  version,identifier,isAvailable,supportedDeviceTypes:names.map(name=>({identifier:deviceId(name)}))
});
const inventory=[
  runtime('27.2',['iPhone-18-Pro','iPhone-17e']),
  runtime('27.0',['iPad-Pro-11-inch-M5-12GB','iPhone-17e']),
  runtime('27.2',['iPhone-Duo'],{identifier:runtimeId('27.2')+'-Duo'}),
  runtime('28.0',['iPhone-17e'],{isAvailable:false}),
  runtime('27.1',['iPhone-Duo'])
];

test('iOS simulator selection uses runtime and device support instead of inventory order',()=>{
  const shuffled=[...inventory].reverse().map(item=>({...item,supportedDeviceTypes:[...item.supportedDeviceTypes].reverse()}));
  for(const runtimes of [inventory,shuffled]){
    assert.deepEqual(selectIOSSimulator(runtimes),{runtime:runtimeId('27.2'),type:deviceId('iPhone-17e')});
    assert.deepEqual(selectIOSSimulator(runtimes,{family:'ipad'}),{
      runtime:runtimeId('27.0'),type:deviceId('iPad-Pro-11-inch-M5-12GB')
    });
  }
  assert.equal(selectIOSSimulator([runtime('27.9',['iPhone-17e']),runtime('27.10',['iPhone-17e'])]).runtime,
    runtimeId('27.10'));
});

test('explicit simulator overrides require an available, supported pair',()=>{
  const id=runtimeId('27.0');
  assert.deepEqual(selectIOSSimulator(inventory,{runtimeId:id,deviceType:deviceId('iPhone-17e')}),
    {runtime:id,type:deviceId('iPhone-17e')});
  assert.throws(()=>selectIOSSimulator(inventory,{runtimeId:id,deviceType:deviceId('iPhone-18-Pro')}),
    /27-0.*iPhone-18-Pro/);
  assert.throws(()=>selectIOSSimulator(inventory,{runtimeId:runtimeId('27.1')}),/27-1/);
  assert.deepEqual(selectIOSSimulator(inventory,{runtimeId:runtimeId('27.1'),deviceType:deviceId('iPhone-Duo')}),
    {runtime:runtimeId('27.1'),type:deviceId('iPhone-Duo')});
  assert.throws(()=>selectIOSSimulator(inventory,{runtimeId:runtimeId('28.0')}),/28-0/);
});

test('Simulator cases terminate only to admit the next launch',async()=>{
  const actions=[];
  try {
    await runIOSSimulatorCases(['export','import'],{
      runCase:async name=>actions.push(name),
      terminatePrevious:async()=>actions.push('terminate'),
    });
  } finally {
    assert.deepEqual(await cleanUpIOSSimulator('owned-device',async(action,id)=>{
      assert.equal(id,'owned-device');
      actions.push(action);
    }),{});
  }
  assert.deepEqual(actions,['export','terminate','import','shutdown','delete']);
});

test('Simulator case or between-case termination errors stop the next launch',async()=>{
  for(const [phase,expected] of [['export',['export']],['terminate',['export','terminate']]]) {
    const actions=[],failure=Error(phase+' failed');
    const record=async action=>{
      actions.push(action);
      if(action===phase)throw failure;
    };
    await assert.rejects(runIOSSimulatorCases(['export','import'],{
      runCase:record,terminatePrevious:()=>record('terminate'),
    }),error=>error===failure);
    assert.deepEqual(actions,expected);
  }
});

test('owned simulator deletion is attempted after shutdown failure and both errors are retained',async()=>{
  const attempts=[];
  const errors=await cleanUpIOSSimulator('owned-device',async(action,id)=>{
    attempts.push([action,id]);
    throw Error(action+' failed');
  });
  assert.deepEqual(attempts,[['shutdown','owned-device'],['delete','owned-device']]);
  assert.deepEqual(errors,{shutdown:'Error: shutdown failed',delete:'Error: delete failed'});
});

test('sandbox receipt binds launch identity and rejects path escapes',async()=>{
  const {mkdtemp,mkdir,realpath,symlink,rm}=await import('node:fs/promises');
  const os=await import('node:os'), path=await import('node:path');
  const {validateIOSSandboxReceipt,readIOSPortableStaging}=await import('../../scripts/lib/ios-simulator-test.mjs');
  const directoryLinkType=process.platform==='win32'?'junction':'dir';
  const temp=await mkdtemp(path.join(os.tmpdir(),'ios-receipt-'));
  try {
    const deviceDataPath=await realpath(temp),home=path.join(deviceDataPath,'Containers','Data','Application','12345678-1234-1234-1234-123456789abc');
    const dir=path.join(home,'Library','Application Support','RaisingEngine','probe');
    await mkdir(path.join(dir,'portable-staging'),{recursive:true});
    const expected={nonce:'nonce',pid:42,contentFingerprint:'sha256:fixture',gameId:'probe',deviceDataPath};
    const receipt={event:'sandbox-receipt',nonce:expected.nonce,pid:expected.pid,contentFingerprint:expected.contentFingerprint,home,saveFile:path.join(dir,'slot-1.json')};
    assert.equal((await validateIOSSandboxReceipt(receipt,expected)).directory,dir);
    assert.deepEqual(await readIOSPortableStaging(dir),[]);
    for(const changed of [{nonce:'other'},{pid:43},{contentFingerprint:'other'},{saveFile:'/tmp/outside/save.json'},{home:'relative'},{home:home+'/../'+path.basename(home)},{saveFile:dir+'//slot-1.json'}])
      await assert.rejects(validateIOSSandboxReceipt({...receipt,...changed},expected));
    await rm(path.join(dir,'portable-staging'),{recursive:true});
    await symlink(home,path.join(dir,'portable-staging'),directoryLinkType);
    await assert.rejects(readIOSPortableStaging(dir),/owned directory/);
    const outside=path.join(home,'outside');await mkdir(outside);
    await assert.rejects(validateIOSSandboxReceipt(receipt,{...expected,deviceDataPath:outside}),/escapes owned simulator/);
    await rm(path.join(home,'Library'),{recursive:true});
    await symlink(outside,path.join(home,'Library'),directoryLinkType);
    await assert.rejects(validateIOSSandboxReceipt(receipt,expected));
  } finally {await rm(temp,{recursive:true,force:true});}
});

test('sandbox receipt wait is bounded and rejects duplicate receipts',async()=>{
  const {waitForIOSSandboxReceipt}=await import('../../scripts/lib/ios-simulator-test.mjs');
  const value={event:'sandbox-receipt',nonce:'n'};
  assert.deepEqual(await waitForIOSSandboxReceipt('unused',{read:async()=>JSON.stringify(value)+'\n'}),value);
  let reads=0;
  assert.deepEqual(await waitForIOSSandboxReceipt('unused',{interval:1,read:async()=>++reads===1?JSON.stringify(value):JSON.stringify(value)+'\n'}),value);
  assert.equal(reads,2);
  await assert.rejects(waitForIOSSandboxReceipt('unused',{timeout:1,interval:1,read:async()=>''}),/Timed out/);
  await assert.rejects(waitForIOSSandboxReceipt('unused',{read:async()=>JSON.stringify(value)+'\n'+JSON.stringify(value)+'\n'}),/Multiple/);
});

test('owned simulator data path comes from its unique inventory entry',async()=>{
  const {ownedIOSSimulatorDataPath}=await import('../../scripts/lib/ios-simulator-test.mjs');
  const entry={udid:'owned',dataPath:'/simulator/owned/data'};
  assert.equal(ownedIOSSimulatorDataPath({devices:{runtime:[entry]}},'owned'),entry.dataPath);
  assert.throws(()=>ownedIOSSimulatorDataPath({devices:{runtime:[entry]}},'other'));
  assert.throws(()=>ownedIOSSimulatorDataPath({devices:{runtime:[entry,entry]}},'owned'));
  assert.throws(()=>ownedIOSSimulatorDataPath({devices:{runtime:[{...entry,dataPath:'relative'}]}},'owned'));
});
