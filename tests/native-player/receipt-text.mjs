// ui.texts is serialized RML. Only unwrap the plain-text receipt introduced by
// recent reading; widget markup and unexpected wrappers must remain observable.
export function nativeReceiptText(value) {
  if (typeof value !== 'string') return value;
  const match = /^<span class="reading-receipt-text" style="(?:margin:0;|margin-top: 0px; margin-right: 0px; margin-bottom: 0px; margin-left: 0px;)">([^<]*)<\/span>$/.exec(value);
  const text = match ? match[1] : value;
  if (text.includes('<')) return value;
  const named = {amp:'&',lt:'<',gt:'>',quot:'"',apos:"'"};
  return text.replace(/&(#(?:[0-9]+|x[0-9a-f]+)|amp|lt|gt|quot|apos);/gi, (entity, key) => {
    if (key[0] !== '#') return named[key.toLowerCase()];
    const point = key[1].toLowerCase() === 'x' ? parseInt(key.slice(2),16) : Number(key.slice(1));
    return Number.isInteger(point) && point > 0 && point <= 0x10ffff && !(point >= 0xd800 && point <= 0xdfff) ? String.fromCodePoint(point) : entity;
  });
}
