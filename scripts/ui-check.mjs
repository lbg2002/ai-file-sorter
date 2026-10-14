/* UI smoke checks: zero third-party runtime dependencies. */
import {readFileSync,existsSync} from "node:fs";
import {execFileSync} from "node:child_process";
const html=readFileSync("web/index.html","utf8");
const script=readFileSync("web/app.js","utf8");
execFileSync(process.execPath,["--check","web/app.js"],{stdio:"inherit"});
const ids=new Set([...html.matchAll(/\bid="([^"]+)"/g)].map(x=>x[1]));
const references=[...new Set([...script.matchAll(/\$\("([^"]+)"\)/g)].map(x=>x[1]))];
const missing=references.filter(id=>!ids.has(id));
if(missing.length)throw Error("UI elements missing: "+missing.join(", "));
if(ids.size!==[...html.matchAll(/\bid="([^"]+)"/g)].length)throw Error("Duplicate HTML IDs");
for(const file of ["web/style.css","web/parity.css"])if(!existsSync(file))throw Error("Stylesheet missing: "+file);
const required=[
 "review-overlay","review-table","review-search","review-status-filter","settings-overlay",
 "history-overlay","bulk-overlay","peek-overlay","explorer","explorer-grip","analyze",
 "include-folders","recursive","include-files","review-rows"
];
for(const id of required)if(!ids.has(id))throw Error("Missing critical original-version control: "+id);
const th=html.match(/<table id="review-table">[\s\S]*?<thead>([\s\S]*?)<\/thead>/);
if(!th||[...th[1].matchAll(/<th>/g)].length!==8)throw Error("Review must have exactly eight columns");
if(!html.includes('href="parity.css"'))throw Error("Original-theme polish stylesheet not linked");
console.log("UI smoke PASS:",ids.size,"unique controls,",references.length,"bound elements, eight review columns.");
