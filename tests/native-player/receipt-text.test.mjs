import test from 'node:test';
import assert from 'node:assert/strict';
import {nativeReceiptText} from './receipt-text.mjs';
const receipt = text => `<span class="reading-receipt-text" style="margin-top: 0px; margin-right: 0px; margin-bottom: 0px; margin-left: 0px;">${text}</span>`;
test('native scalar receipts unwrap only the known text span and decode entities once', () => {
  assert.equal(nativeReceiptText(receipt('22')), '22');
  assert.equal(nativeReceiptText('<span class="reading-receipt-text" style="margin:0;">1</span>'), '1');
  assert.equal(nativeReceiptText(receipt('&lt;name&gt; &amp; &quot;quote&quot; &apos; &#65; &#x1F600; &amp;lt;')), '<name> & "quote" \' A 😀 &lt;');
  assert.equal(nativeReceiptText('legacy &amp; text'), 'legacy & text');
  assert.equal(nativeReceiptText(undefined), undefined);
  assert.equal(nativeReceiptText(receipt('&#x110000; &#55296; &unknown;')), '&#x110000; &#55296; &unknown;');
});
test('unexpected or nested native markup is never stripped from scalar assertions', () => {
  for(const markup of ['<b>1</b>', receipt('<b>1</b>'), receipt('1')+receipt('2'), '<span class="different" style="margin:0;">1</span>', '<span class="reading-receipt-text" style="color:red;">1</span>']) assert.equal(nativeReceiptText(markup), markup);
});
