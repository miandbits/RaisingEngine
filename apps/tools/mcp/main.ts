import { realpath } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { serveStdio } from '@modelcontextprotocol/server/stdio';
import { createMcpServer } from '../../../packages/mcp-service/server.js';
import { recoverProject } from '../../../packages/project-service/transaction.js';

try {
  const args = process.argv.slice(2);
  if (args[0] !== '--project' || !args[1] || args.length > 3 || (args[2] !== undefined && args[2] !== '--read-only'))
    throw new Error('Usage: node dist/apps/tools/mcp/main.js --project <directory> [--read-only]');
  const root = await realpath(args[1]);
  const readOnly = args[2] === '--read-only';
  if (!readOnly) await recoverProject(root);
  const bundledWorker = new URL('./analysis-worker.js', import.meta.url);
  await serveStdio(() => createMcpServer(root, readOnly, existsSync(bundledWorker) ? bundledWorker : undefined));
} catch (error) {
  console.error(error instanceof Error ? error.message : String(error));
  process.exitCode = 1;
}
