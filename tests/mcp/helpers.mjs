import { cp, mkdir, mkdtemp } from 'node:fs/promises';
import { resolve } from 'node:path';
import { Client } from '@modelcontextprotocol/client';
import { StdioClientTransport } from '@modelcontextprotocol/client/stdio';

export async function fixture() {
  await mkdir('.cache/mcp-tests', { recursive: true });
  const root = await mkdtemp(resolve('.cache/mcp-tests/project-'));
  await cp(resolve('examples/academy-demo'), root, { recursive: true });
  return root;
}
export async function connect(t, root, options = {}) {
  const env = { ...process.env }; delete env.NODE_OPTIONS; delete env.ELECTRON_RUN_AS_NODE;
  const transport = new StdioClientTransport({ command: process.execPath,
    args: [resolve('dist/apps/tools/mcp/main.js'), '--project', root, ...(options.readOnly ? ['--read-only'] : [])],
    cwd: resolve('.'), env, stderr: 'pipe' });
  const client = new Client({ name: 'raising-tests', version: '1.0.0' }, options.modern ? { versionNegotiation: { mode: { pin: '2026-07-28' } } } : {});
  let stderr = ''; transport.stderr?.on('data', bytes => { stderr += bytes.toString(); });
  t.after(() => client.close());
  await client.connect(transport);
  return { client, transport, stderr: () => stderr,
    call: async (name, args = {}) => {
      const result = await client.callTool({ name, arguments: args });
      const data = result.structuredContent ?? JSON.parse(result.content[0].text);
      return { ...data, isError: !!result.isError };
    } };
}
