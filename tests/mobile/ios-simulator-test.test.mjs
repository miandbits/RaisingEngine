import assert from 'node:assert/strict';
import test from 'node:test';
import {cleanUpIOSSimulator,selectIOSSimulator} from '../../scripts/lib/ios-simulator-test.mjs';

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

test('owned simulator deletion is attempted after shutdown failure and both errors are retained',async()=>{
  const attempts=[];
  const errors=await cleanUpIOSSimulator('owned-device',async(action,id)=>{
    attempts.push([action,id]);
    throw Error(action+' failed');
  });
  assert.deepEqual(attempts,[['shutdown','owned-device'],['delete','owned-device']]);
  assert.deepEqual(errors,{shutdown:'Error: shutdown failed',delete:'Error: delete failed'});
});
