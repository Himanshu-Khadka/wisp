const fs=require('fs'),vm=require('vm'),assert=require('assert');
const {JSDOM}=require('jsdom');const dom=new JSDOM('');const fflate=require('../vendor/fflate.cjs');
const src=fs.readFileSync(require('path').join(__dirname,'../firmware/Wisp/portal.js'),'utf8');
const code=src.slice(src.indexOf('function safeName('),src.indexOf('function uploadFile('));
const context={fflate,DOMParser:dom.window.DOMParser,TextDecoder,URL};vm.createContext(context);vm.runInContext(code,context);
const encode=s=>new TextEncoder().encode(s);
function book(extra={}){return fflate.zipSync({'META-INF/container.xml':encode('<container><rootfiles><rootfile full-path="OPS/book.opf"/></rootfiles></container>'),'OPS/book.opf':encode('<package><manifest><item id="a" href="a.xhtml"/><item id="b" href="b.xhtml"/></manifest><spine><itemref idref="b"/><itemref idref="a"/></spine></package>'),'OPS/a.xhtml':encode('<html><body><p>Second chapter.</p><script>bad content</script></body></html>'),'OPS/b.xhtml':encode('<html><body><h1>First chapter.</h1><p>Hello world.</p></body></html>'),...extra})}
let text=context.epubToText(book());assert.equal(text,'First chapter. Hello world. Second chapter.');
assert.throws(()=>context.epubToText(book({'META-INF/encryption.xml':encode('<encryption/>')})),/Encrypted/);
assert.throws(()=>context.epubToText(fflate.zipSync({'bad.txt':encode('bad')})),/missing/);
assert.throws(()=>context.epubToText(book({'OPS/a.xhtml':encode('<bad')})),/Malformed/);
assert.throws(()=>context.epubToText(book({'big.xhtml':encode('x'.repeat(5*1024*1024+1))})),/limit/);
assert.equal(context.readingText('Café — hello\n world'),'Cafe - hello world');assert(!context.safeName('../../bad.txt').includes('..'));
console.log('PASS: EPUB spine order, paragraph boundaries, script removal, encrypted/missing/malformed/oversized books, ASCII conversion, filename sanitization');
