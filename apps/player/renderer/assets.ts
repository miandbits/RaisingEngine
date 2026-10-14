declare global { interface Window { raisingAssetUrl?: (id: string) => string } }
export const assetUrl = (id: string, generation: number) => window.raisingAssetUrl?.(id)
  ?? `raising-asset://project/${encodeURIComponent(id)}?g=${generation}`;
