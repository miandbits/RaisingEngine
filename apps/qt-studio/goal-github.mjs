import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {randomUUID} from 'node:crypto';
import yauzl from 'yauzl';

const exec = promisify(execFile), archiveLimit = 64 * 1024 * 1024, reportLimit = 16 * 1024 * 1024;
const repoPattern = /^[A-Za-z0-9][A-Za-z0-9_.-]*\/[A-Za-z0-9][A-Za-z0-9_.-]*$/;
function repository(value) {
  if (typeof value !== 'string' || value.length > 200 || !repoPattern.test(value)) throw Error('저장소를 owner/repository 형식으로 입력하세요.');
  return value;
}
export async function githubCommand(args, binary = false) {
  try {
    const {stdout} = await exec('gh', args, {encoding: binary ? 'buffer' : 'utf8', timeout: 90000,
      maxBuffer: binary ? archiveLimit : 2 * 1024 * 1024, windowsHide: true,
      env: {...process.env, GH_PROMPT_DISABLED: '1', GH_HOST: 'github.com', NO_COLOR: '1'}});
    return binary ? stdout : JSON.parse(stdout);
  } catch (e) {
    // Never echo CLI output: authentication helpers may include sensitive information.
    const stderr = String(e.stderr || '');
    if (e.code === 'ENOENT') throw Error('GitHub CLI(gh)가 없습니다. 설치 후 gh auth login으로 로그인하세요.');
    if (/401|403|authentication|gh auth login/i.test(stderr)) throw Error('GitHub 인증 또는 권한이 필요합니다. gh auth login과 저장소 Actions 읽기 권한을 확인하세요.');
    if (/404|410|expired/i.test(stderr)) throw Error('GitHub 실행 또는 보고서를 찾을 수 없거나 보관 기간이 만료되었습니다. 목록을 새로고침하세요.');
    if (e.killed || e.code === 'ERR_CHILD_PROCESS_STDIO_MAXBUFFER') throw Error('GitHub 요청 시간 또는 크기 제한을 초과했습니다. 다시 시도하세요.');
    throw Error('GitHub 조회에 실패했습니다. 네트워크·인증·요청 제한을 확인한 뒤 다시 시도하세요.');
  }
}

// Inspect in memory. Archive paths are labels only and are never written to disk.
export function readReportArchive(bytes) {
  if (!Buffer.isBuffer(bytes) || bytes.length > archiveLimit) return Promise.reject(Error('보고서 압축 파일은 64 MiB 이하이어야 합니다.'));
  return new Promise((resolve, reject) => yauzl.fromBuffer(bytes, {lazyEntries: true, strictFileNames: true}, (err, zip) => {
    if (err) return reject(Error('보고서 ZIP이 손상되었습니다.'));
    let count = 0, total = 0, settled = false;
    const reports = [], names = new Set();
    const fail = message => {if (!settled) {settled = true; zip.close(); reject(Error(message));}};
    zip.on('error', () => fail('보고서 ZIP 경로 또는 압축 데이터가 유효하지 않습니다.'));
    zip.on('end', () => {if (!settled) {settled = true; resolve(reports.sort((a,b) => Number(b.primary) - Number(a.primary) || a.entry.localeCompare(b.entry)));}});
    zip.on('entry', entry => {
      if (++count > 2048) return fail('보고서 ZIP 항목 수 제한을 초과했습니다.');
      const name = entry.fileName;
      if (!/(^|\/)report\.json$/.test(name)) return zip.readEntry();
      if (names.has(name) || reports.length >= 32 || entry.uncompressedSize > reportLimit || total + entry.uncompressedSize > archiveLimit) return fail('보고서 중복 또는 압축 해제 크기 제한을 초과했습니다.');
      names.add(name);
      zip.openReadStream(entry, (error, stream) => {
        if (error) return fail('보고서 압축 해제에 실패했습니다.');
        const chunks = []; let size = 0;
        stream.on('error', () => fail('보고서 압축 데이터가 손상되었습니다.'));
        stream.on('data', chunk => {
          size += chunk.length; total += chunk.length;
          if (size > reportLimit || total > archiveLimit) {stream.destroy(); fail('보고서 압축 해제 크기 제한을 초과했습니다.');} else chunks.push(chunk);
        });
        stream.on('end', () => {
          if (settled) return;
          reports.push({id: randomUUID(), entry: name, primary: /(^|\/)ci-goal-regression\/report\.json$/.test(name), bytes: Buffer.concat(chunks)});
          zip.readEntry();
        });
      });
    });
    zip.readEntry();
  }));
}

export class GitHubGoalReports {
  listing = null;
  bundle = null;
  constructor(command = githubCommand) {this.command = command;}
  async defaults(root) {
    try {
      const {stdout} = await exec('git', ['-C', root, 'remote', 'get-url', 'origin'], {encoding: 'utf8', timeout: 5000, windowsHide: true});
      const match = stdout.trim().match(/^(?:https:\/\/github\.com\/|git@github\.com:)([^/]+\/[^/]+?)(?:\.git)?$/);
      return {repository: match && repoPattern.test(match[1]) ? match[1] : ''};
    } catch {return {repository: ''};}
  }
  async list(root, params) {
    this.listing = null; this.bundle = null;
    const repo = repository(params.repository), page = params.page ?? 1, branch = params.branch ?? '';
    if (!Number.isInteger(page) || page < 1 || page > 100 || typeof branch !== 'string' || branch.length > 256) throw Error('실행 목록 페이지 또는 브랜치가 유효하지 않습니다.');
    const query = new URLSearchParams({per_page: '20', page: String(page)});
    if (branch) query.set('branch', branch);
    const result = await this.command(['api', '--hostname', 'github.com', `repos/${repo}/actions/workflows/goal-regression.yml/runs?${query}`]);
    if (!Array.isArray(result.workflow_runs)) throw Error('GitHub 실행 목록 응답이 유효하지 않습니다.');
    const runs = result.workflow_runs.slice(0,20).map(r => ({id: r.id, number: r.run_number, attempt: r.run_attempt, title: r.display_title,
      branch: r.head_branch, sha: r.head_sha, status: r.status, conclusion: r.conclusion, createdAt: r.created_at,
      url: `https://github.com/${repo}/actions/runs/${r.id}`})).filter(r => Number.isSafeInteger(r.id) && r.id > 0);
    this.listing = {id: randomUUID(), root, repository: repo, page, branch, runs, hasNext: result.total_count > page * 20};
    return this.listing;
  }
  async reports(root, listingId, runId) {
    this.bundle = null;
    const listing = this.listing;
    if (!listing || listing.root !== root || listing.id !== listingId) throw Error('실행 목록이 만료되었습니다. 다시 조회하세요.');
    const run = listing.runs.find(r => r.id === runId);
    if (!run || run.status !== 'completed') throw Error('완료된 실행을 선택하세요. 진행 중인 실행은 새로고침 후 확인할 수 있습니다.');
    const repo = listing.repository;
    const result = await this.command(['api', '--hostname', 'github.com', `repos/${repo}/actions/runs/${run.id}/artifacts?per_page=100`]);
    const artifacts = (result.artifacts || []).filter(a => a.name === 'goal-regression-reports');
    const artifact = artifacts.find(a => !a.expired);
    if (!artifact) throw Error(artifacts.length ? '보고서 보관 기간이 만료되었습니다. CI를 다시 실행하세요.' : '이 실행에는 경로 보고서가 없습니다. 보고서 생성 전 실패했거나 업로드하지 않은 실행입니다.');
    if (!Number.isSafeInteger(artifact.id) || artifact.id <= 0 || artifact.size_in_bytes > archiveLimit) throw Error('보고서 아티팩트 ID 또는 크기가 유효하지 않습니다 (최대 64 MiB).');
    const bytes = await this.command(['api', '--hostname', 'github.com', `repos/${repo}/actions/artifacts/${artifact.id}/zip`], true);
    const entries = await readReportArchive(bytes);
    if (!entries.length) throw Error('압축 파일에 report.json이 없습니다.');
    if (this.listing !== listing) throw Error('실행 목록이 변경되었습니다. 다시 선택하세요.');
    this.bundle = {id: randomUUID(), root, repository: repo, run, artifactId: artifact.id, entries};
    return {id: this.bundle.id, repository: repo, run, artifactId: artifact.id, reports: entries.map(({id,entry,primary,bytes}) => {
      try {const r=JSON.parse(bytes.toString('utf8'));return {id,entry,primary,ok:r.ok,failedRoutes:Array.isArray(r.rows)?r.rows.filter(x=>x.status!=='unchanged').length:0};}
      catch {return {id,entry,primary,error:'JSON 손상'};}
    })};
  }
  open(editor, ci, revision, bundleId, reportId) {
    const bundle = this.bundle;
    if (!bundle || bundle.id !== bundleId || bundle.root !== editor.root) throw Error('GitHub 보고서 세션이 만료되었습니다. 다시 가져오세요.');
    const entry = bundle.entries.find(e => e.id === reportId);
    if (!entry) throw Error('선택한 GitHub 보고서가 없습니다.');
    const origin = {repository: bundle.repository, run: bundle.run, artifactId: bundle.artifactId, entry: entry.entry, primary: entry.primary};
    return ci.openBytes(editor, entry.bytes, revision, entry.entry, origin);
  }
}
