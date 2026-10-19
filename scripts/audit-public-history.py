"""Read-only all-ref Git history audit. Never prints credential contents."""
import argparse, hashlib, json, pathlib, re, subprocess

parser=argparse.ArgumentParser()
parser.add_argument('--repository',required=True)
parser.add_argument('--policy',required=True)
parser.add_argument('--output',required=True)
args=parser.parse_args()
repo=pathlib.Path(args.repository).resolve(); output=pathlib.Path(args.output).resolve()
policy=json.loads(pathlib.Path(args.policy).read_text(encoding='utf8'))
def git(*values):
    return subprocess.check_output(['git','-C',str(repo),*values])

prefixes=['projects/','internal/','.cache/','dist/','release/','docs/evidence/','node_modules/','output/','.playwright-cli/']
private=set(policy['privateTools'])
names=git('log','--all','--format=','--name-only','-z').decode('utf8','replace').split('\0')
paths=sorted({p.strip('\n') for p in names if p.strip('\n') and (p.strip('\n') in private or any(p.strip('\n').startswith(x) for x in prefixes))})
objects={}
for line in git('rev-list','--objects','--all').decode('utf8','replace').splitlines():
    item=line.split(' ',1);objects[item[0]]=item[1] if len(item)>1 else ''
ids='\n'.join(objects)+'\n'
metadata=subprocess.check_output(['git','-C',str(repo),'cat-file','--batch-check'],input=ids.encode()).decode().splitlines()
blobs=[line.split()[0] for line in metadata if len(line.split())>=3 and line.split()[1]=='blob']
excluded=set(policy['excludedAssetSha256']); matches=[]; suspects=[]; total=0
patterns={
    'github-token':re.compile(rb'(?<![A-Za-z0-9])gh[pousr]_[A-Za-z0-9]{36,255}(?![A-Za-z0-9])'),
    'github-fine-grained-token':re.compile(rb'github_pat_[A-Za-z0-9_]{80,255}'),
    'aws-access-id':re.compile(rb'(?<![A-Z0-9])(?:AKIA|ASIA)[A-Z0-9]{16}(?![A-Z0-9])'),
    'private-key-block':re.compile(rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----\s+[A-Za-z0-9+/=\r\n]{80,}')
}
process=subprocess.Popen(['git','-C',str(repo),'cat-file','--batch'],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
for oid in blobs:
    process.stdin.write((oid+'\n').encode());process.stdin.flush()
    header=process.stdout.readline().decode().split();size=int(header[2]);data=process.stdout.read(size);process.stdout.read(1);total+=size
    if hashlib.sha256(data).hexdigest() in excluded:matches.append({'object':oid,'examplePath':objects[oid]})
    if b'\0' not in data:
        for kind,pattern in patterns.items():
            if pattern.search(data):suspects.append({'object':oid,'examplePath':objects[oid],'kind':kind})
process.stdin.close();process.wait()
if process.returncode:raise RuntimeError('git cat-file failed')
refs=git('for-each-ref','--format=%(refname) %(objectname)').decode().splitlines()
report={'repository':str(repo),'refs':refs,'blobCount':len(blobs),'uncompressedBlobBytes':total,'forbiddenPaths':paths,'excludedAssetBlobs':matches,'credentialPatternSuspects':suspects,'limitation':'Known paths/hashes and selected credential signatures only; not a complete provenance or secret clearance.'}
output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
print(json.dumps({'report':str(output),'refs':len(refs),'blobs':len(blobs),'forbiddenPaths':len(paths),'excludedAssetBlobs':len(matches),'credentialPatternSuspects':len(suspects)}))
