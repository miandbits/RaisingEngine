import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,writeFile,readFile} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';

test('portable staged-file leases clean only their own files; desktop remains unavailable',{timeout:90000},async()=>{
 await mkdir('.cache/portable-save-files',{recursive:true});const out=await mkdtemp(resolve('.cache/portable-save-files/run-')),source=join(out,'lifecycle.cpp'),exe=join(out,process.platform==='win32'?'lifecycle.exe':'lifecycle');
 await writeFile(source,String.raw`
#define RAISING_PORTABLE_SAVE_TESTING 1
#include "portable-save-files.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace raising_player;
namespace fs=std::filesystem;
static void write(const fs::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);f<<s;assert(f.good());}
class Fake final:public PortableSaveFilesBackend {
 public:
  bool alive=true,pending=false;std::uint64_t id=0;int shutdowns=0;
  std::optional<PortableSaveResult> result;
  bool available()const override{return alive;}
  bool busy()const override{return pending;}
  bool begin_import(std::uint64_t next)override{if(!alive||pending)return false;pending=true;id=next;return true;}
  bool begin_export(std::uint64_t next,const fs::path&)override{return begin_import(next);}
  std::optional<PortableSaveResult> poll()override{auto out=std::move(result);result.reset();if(out)pending=false;return out;}
  void cancel(std::uint64_t expected)override{if(pending&&id==expected)result=PortableSaveResult{id,PortableSaveKind::import_file,PortableSaveStatus::cancelled,nullptr,""};}
  void shutdown()override{if(alive){alive=false;pending=false;result.reset();++shutdowns;}}
};
int main(int argc,char** argv){
 assert(argc==2);const auto root=fs::u8path(argv[1]);fs::create_directories(root);
 const auto unrelated=root/"keep.txt";write(unrelated,"user-owned");
 PortableSaveFiles production(nullptr,root/"production");assert(!production.available());assert(!production.busy());assert(!production.begin_import(1));assert(!production.begin_export(1,unrelated));assert(!production.poll());production.cancel(1);production.shutdown();assert(!fs::exists(root/"production"));
 fs::path staged;
 {auto first=PortableSaveStagedFile::create(root,"lease-one");staged=first->path;write(staged,"{}");write(staged.parent_path()/"raising-save.json.tmp","failed write");write(staged.parent_path()/"raising-save.json.bak","old snapshot");auto second=first;first.reset();assert(fs::exists(staged));second.reset();assert(!fs::exists(staged));assert(!fs::exists(staged.parent_path()));}
 {auto first=PortableSaveStagedFile::create(root,"collision");write(first->path,"original");bool rejected=false;try{auto invalid=PortableSaveStagedFile::create(root,"collision");}catch(const std::runtime_error& e){rejected=std::string(e.what())=="write-failed";}assert(rejected&&fs::exists(first->path));std::ifstream f(first->path);std::string bytes;f>>bytes;assert(bytes=="original");}
 for(auto name:{std::string(""),std::string("../outside"),std::string("sub/file"),std::string(81,'a')}){bool rejected=false;try{auto file=PortableSaveStagedFile::create(root,name);}catch(const std::runtime_error& e){rejected=std::string(e.what())=="write-failed";}assert(rejected);}
 fs::path extra;
 {auto file=PortableSaveStagedFile::create(root,"extra");extra=file->path.parent_path()/"not-ours.txt";write(extra,"keep");write(file->path,"{}");}
 assert(fs::exists(extra));assert(!fs::exists(extra.parent_path()/"raising-save.json"));
 const auto abandoned=root/"request-abandoned",unknown=root/"request-unknown",wrong=root/"request-wrong-marker";
 for(const auto& dir:{abandoned,unknown,wrong}){fs::create_directory(dir);write(dir/"raising-save.json","{}");}
 write(abandoned/".raising-portable-stage-v1","RaisingEngine portable save staging v1\n");write(abandoned/"raising-save.json.tmp","interrupted write");write(abandoned/"raising-save.json.bak","old snapshot");write(wrong/".raising-portable-stage-v1","not our marker\n");
 PortableSaveStagedFile::cleanup_abandoned(root);assert(!fs::exists(abandoned));assert(fs::exists(unknown/"raising-save.json"));assert(fs::exists(wrong/"raising-save.json"));assert(fs::exists(extra));assert(fs::exists(unrelated));
 auto backend=std::make_shared<Fake>();
 {PortableSaveFiles injected(backend);assert(injected.available());assert(injected.begin_import(7));assert(!injected.begin_import(8));assert(injected.busy());injected.cancel(6);assert(!injected.poll());injected.cancel(7);auto cancelled=injected.poll();assert(cancelled&&cancelled->requestId==7&&cancelled->status==PortableSaveStatus::cancelled);assert(!injected.poll());assert(!injected.busy());assert(injected.begin_export(9,unrelated));injected.shutdown();assert(!injected.available());assert(!injected.poll());}
 assert(backend->shutdowns==1);assert(fs::exists(unrelated));std::cout<<"{\"ok\":true,\"scope\":\"portable ownership and interface only; no UIKit acceptance\",\"checks\":9}\n";
}
`);
 const mingw=join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe'),compiler=process.env.CXX??(process.platform==='win32'?(existsSync(mingw)?mingw:'g++'):'c++');
 const built=spawnSync(compiler,['-std=c++17','-O1','-Wall','-Wextra','-I',resolve('native/player'),source,'-o',exe],{encoding:'utf8',timeout:60000});assert.equal(built.status,0,built.stderr||String(built.error));
 const run=spawnSync(exe,[join(out,'owned-root')],{encoding:'utf8',timeout:30000});assert.equal(run.status,0,run.stderr||run.stdout);const report=JSON.parse(run.stdout);assert.equal(report.ok,true);assert.equal(await readFile(join(out,'owned-root/keep.txt'),'utf8'),'user-owned');await writeFile(join(out,'result.json'),JSON.stringify(report,null,2)+'\n');
});
