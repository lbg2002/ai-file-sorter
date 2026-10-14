"use strict";
const invoke = (cmd, args={}) => {
  if (!window.__TAURI__ || !window.__TAURI__.core) {
    return Promise.reject(new Error("请从 Tauri 桌面程序启动，普通浏览器无法访问本地文件。"));
  }
  return window.__TAURI__.core.invoke(cmd, args);
};
const el = id => document.getElementById(id);
const esc = value => String(value ?? "").replace(/[&<>"']/g, ch => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[ch]));
const state = {
  page:"work", mode:"ai", folder:"", scan:null, assignments:[],
  preview:null, modelIssues:[], busy:false, selected:new Set(),
  settings:null, apiKey:"", history:[], historyId:null, search:"", filter:"all"
};
let toastTimer;
function notify(message, error=false) {
  const box=el("toast"); box.textContent=message; box.className="toast show"+(error?" error":"");
  clearTimeout(toastTimer);toastTimer=setTimeout(()=>box.className="toast",4800);
}
function busy(on, label) {
  state.busy=on;
  el("top-status").classList.toggle("busy",on);
  el("top-status").querySelector("span").textContent=on ? label : "准备就绪";
  for (const id of ["btn-scan","choose-folder","btn-plan","btn-rescan","btn-apply","bulk-apply","fetch-models","save-config"]) {
    el(id).disabled=on;
  }
  renderActions();
}
async function run(job,label) {
  if(state.busy)return;
  busy(true,label);
  try { await job(); }
  catch(err){console.error(err);notify(String(err?.message||err),true);}
  finally {busy(false); }
}
function settingsFromUI(){
  return {
    endpoint:el("cfg-endpoint").value.trim(),
    model:el("cfg-model").value.trim(),
    prompt:el("cfg-prompt").value,
    rules:el("cfg-rules").value,
    includeFolders:el("cfg-folders").checked,
    theme:el("cfg-theme").value
  };
}
function populateSettings(){
  const s=state.settings;
  el("cfg-endpoint").value=s.endpoint;
  el("cfg-model").value=s.model;
  el("cfg-prompt").value=s.prompt;
  el("cfg-rules").value=s.rules;
  el("cfg-folders").checked=s.includeFolders;
  el("cfg-theme").value=s.theme;
  el("include-folders").checked=s.includeFolders;
  document.body.dataset.theme=s.theme;
}
function show(page) {
  state.page=page;
  for(const node of document.querySelectorAll(".page"))node.classList.toggle("active",node.id==="page-"+page);
  for(const node of document.querySelectorAll(".nav-item[data-page]"))node.classList.toggle("active",node.dataset.page===page);
  el("crumb").textContent={work:"整理工作台",history:"操作历史",settings:"设置"}[page];
  if(page==="history")loadHistory();
}
function invalidate() {
  state.scan=null;state.assignments=[];state.preview=null;state.modelIssues=[];state.selected.clear();
  renderAll();
}
async function chooseFolder(){
  let selected=false;
  await run(async()=>{
    const folder=await invoke("pick_folder");
    if(folder){el("folder-input").value=folder;invalidate();selected=true;}
  },"选择文件夹中");
  if(selected)await scanFolder();
}
async function scanFolder() {
  const folder=el("folder-input").value.trim();
  if(!folder){notify("请先选择文件夹",true);return;}
  await run(async()=>{
    const result=await invoke("scan_folder",{folder,includeFolders:el("include-folders").checked});
    state.scan=result;state.folder=result.root;state.assignments=[];state.preview=null;state.modelIssues=[];state.selected.clear();
    el("folder-input").value=result.root;
    renderAll();notify("扫描完成：共 "+result.items.length+" 项，跳过 "+result.ignored.length+" 项");
  },"正在扫描文件夹");
}
async function requestPlan() {
  const folder=el("folder-input").value.trim();
  if(!folder){notify("请先选择文件夹",true);return;}
  await run(async()=>{
    let result;
    if(state.mode==="ai"){
      const current=settingsFromUI();
      result=await invoke("analyze_folder",{
        folder,includeFolders:el("include-folders").checked,
        endpoint:current.endpoint,model:current.model,apiKey:state.apiKey,prompt:current.prompt
      });
    }else{
      result=await invoke("plan_by_rules",{
        folder,includeFolders:el("include-folders").checked,rules:el("cfg-rules").value
      });
    }
    state.scan=result.scan;state.folder=result.scan.root;
    state.assignments=result.assignments;state.modelIssues=result.issues||[];
    state.selected.clear();el("folder-input").value=result.scan.root;
    await validatePreview();renderAll();
    if(state.modelIssues.length){notify("模型结果存在 "+state.modelIssues.length+" 处异常，请使用“处理遗漏项”人工审查",true);}
    else notify("方案生成完成。请检查分类并确认移动。");
  },state.mode==="ai"?"正在整批请求 AI（只发送一次）":"正在按规则规划");
}
async function validatePreview() {
  if(!state.scan||!state.assignments.length){state.preview=null;return;}
  state.preview=await invoke("preview_plan",{folder:state.scan.root,items:state.scan.items,assignments:state.assignments});
}
function putAssignment(id,cat) {
  const i=state.assignments.findIndex(x=>x.id===id);
  if(i<0)state.assignments.push({id,category:cat});
  else state.assignments[i].category=cat;
}
async function editCategory(id,category) {
  if(!state.scan||state.busy)return;
  putAssignment(id,category.trim());
  try{await validatePreview();renderAll();}catch(e){notify(String(e),true);}
}
function repair() {
  if(!state.scan)return;
  ask("人工接管不完整方案","模型有重复、未知或遗漏项目。将丢弃模型的错误记录，所有未返回的项目明确设为“保留原位”，其余分类保留供你继续编辑。确认以后仍需通过全部本地校验。",async()=>{
    const map=new Map(state.assignments.map(x=>[x.id,x.category]));
    state.assignments=state.scan.items.map(i=>({id:i.id,category:map.get(i.id)||""}));
    state.modelIssues=[];
    await validatePreview();renderAll();notify("已修复清单结构。请继续审查预览。");
  });
}
async function applyPlan(){
  if(!state.scan||!state.preview||state.busy)return;
  if(state.modelIssues.length||state.preview.issues.length){notify("仍有未解决异常，不能执行移动",true);return;}
  if(el("folder-input").value.trim()!==state.scan.root){notify("目录已改变，请重新扫描",true);return;}
  const n=state.preview.movable;
  ask("确认移动 "+n+" 项","将按预览结果创建一级分类目录并移动文件。不会删除文件，也不会覆盖已有目标。\n所有成功移动都写入操作日志，可从操作历史撤销。",async()=>{
    await run(async()=>{
      const id=await invoke("apply_plan",{folder:state.scan.root,items:state.scan.items,assignments:state.assignments});
      notify("整理完成，已保存历史记录："+id);
      const previous=state.scan.root;
      invalidate();
      const scanned=await invoke("scan_folder",{folder:previous,includeFolders:el("include-folders").checked});
      state.scan=scanned;state.folder=scanned.root;
      renderAll();await loadHistory();
    },"移动中 · 写入操作日志");
  });
}
function renderActions(){
  const canMove=!!(state.preview && state.preview.movable>0 && state.preview.issues.length===0 && !state.modelIssues.length && !state.busy);
  el("btn-apply").disabled=!canMove;
  el("repair-plan").hidden=state.modelIssues.length===0;
}
function filteredItems(){
  if(!state.scan)return [];
  const query=state.search.trim().toLowerCase();
  const map=new Map(state.preview?.rows.map(x=>[x.id,x])||[]);
  return state.scan.items.filter(item=>{
    const row=map.get(item.id), cat=row?.category||"";
    if(query&&!((item.name+" "+cat).toLowerCase().includes(query)))return false;
    if(!row)return state.filter==="all";
    if(state.filter==="movable")return row.status==="可移动";
    if(state.filter==="kept")return row.status==="保留原位";
    if(state.filter==="error")return !["可移动","保留原位"].includes(row.status);
    return true;
  });
}
function renderTable(){
  const items=filteredItems();
  const body=el("file-rows");
  if(!state.scan){
    body.innerHTML='<tr><td colspan="5"><div class="empty"><div class="empty-mark">▤</div><strong>还没有文件清单</strong><span>请选择一个文件夹并开始扫描</span></div></td></tr>';
    el("table-foot").textContent="0 个项目";return;
  }
  if(!items.length){
    body.innerHTML='<tr><td colspan="5"><div class="empty"><div class="empty-mark">⌕</div><strong>没有符合条件的项目</strong><span>试试清除筛选或搜索</span></div></td></tr>';
  }else{
    const rows=new Map(state.preview?.rows.map(x=>[x.id,x])||[]);
    body.innerHTML=items.map(item=>{
      const row=rows.get(item.id);
      const stateText=row?.status||"待规划";
      const tone=stateText==="可移动"?"status-good":(stateText==="保留原位"||stateText==="待规划")?"status-idle":"status-danger";
      const initial=state.assignments.find(a=>a.id===item.id)?.category||"";
      return '<tr>'+
        '<td class="select-cell"><input type="checkbox" class="row-select" data-id="'+esc(item.id)+'" '+(state.selected.has(item.id)?"checked":"")+'></td>'+
        '<td><div class="filename"><span class="fileicon '+(item.kind==="folder"?"folder":"")+'">'+(item.kind==="folder"?"▰":"▤")+'</span><span title="'+esc(item.name)+'">'+esc(item.name)+'</span></div></td>'+
        '<td>'+(item.kind==="folder"?"文件夹":"文件")+'</td>'+
        '<td><input class="category-input" data-id="'+esc(item.id)+'" value="'+esc(initial)+'" placeholder="保留原位" list="category-options" '+(state.busy?"disabled":"")+'></td>'+
        '<td><span class="status-badge '+tone+'">'+esc(stateText)+'</span></td>'+
      '</tr>';
    }).join("");
  }
  el("table-foot").textContent=state.scan.items.length+" 个项目，当前显示 "+items.length+" 项";
  el("select-all").checked=items.length>0&&items.every(i=>state.selected.has(i.id));
  el("selected-count").textContent="已选择 "+state.selected.size+" 项";
  el("bulk-bar").hidden=state.selected.size===0;
}
function renderInsights(){
  const scan=state.scan,pr=state.preview;
  const categories=new Map();
  for(const row of pr?.rows||[])if(row.status==="可移动")categories.set(row.category,(categories.get(row.category)||0)+1);
  const sorted=Array.from(categories.entries()).sort((a,b)=>b[1]-a[1]);
  el("category-preview").innerHTML=sorted.length?sorted.map(([name,count])=>
    '<div class="category-row"><span class="category-dot"></span><span class="name" title="'+esc(name)+'">'+esc(name)+'</span><b>'+count+'</b></div>'
  ).join(""):'<div class="muted-message">生成方案后展示目标目录分布</div>';
  const issues=[...state.modelIssues,...(pr?.issues||[])];
  el("issue-counter").textContent=String(issues.length);
  el("issue-list").innerHTML=issues.length?issues.slice(0,35).map(issue=>'<div class="issue-row"><span>⚑</span><span>'+esc(issue)+'</span></div>').join("")+(issues.length>35?'<div class="muted-message">另有 '+(issues.length-35)+' 项异常</div>':'') :
    '<div class="success-line"><span>✓</span>'+(pr?'全部本地检查通过':'等待生成整理方案')+'</div>';
  const options=new Set(scan?.existingFolders||[]);
  for(const row of pr?.rows||[])if(row.category)options.add(row.category);
  el("category-options").innerHTML=Array.from(options).sort().map(s=>'<option value="'+esc(s)+'"></option>').join("");
}
function renderAll(){
  const scan=state.scan,pr=state.preview;
  el("stat-all").textContent=scan?scan.items.length:"—";
  el("stat-move").textContent=pr?pr.movable:"—";
  el("stat-keep").textContent=pr?pr.kept:"—";
  el("stat-issue").textContent=pr?pr.issues.length+state.modelIssues.length:"—";
  el("scan-note").textContent=scan?"跳过 "+scan.ignored.length+" 项 · "+scan.existingFolders.length+" 个现有文件夹":"尚未扫描";
  el("plan-subtitle").textContent=pr?"方案已生成，可修改后重新校验":(scan?"已扫描，等待生成分类方案":"先扫描文件夹，然后生成方案");
  el("validation-summary").textContent=pr?(pr.issues.length+state.modelIssues.length?"存在异常，禁止执行":"已通过安全校验"):"暂无整理方案";
  renderTable();renderInsights();renderActions();
}
function setMode(mode){
  state.mode=mode;
  for(const btn of document.querySelectorAll("[data-mode]"))btn.classList.toggle("active",btn.dataset.mode===mode);
  el("plan-mode-label").textContent=mode==="ai"?"整批 AI 智能规划":"本地规则整理";
  el("plan-mode-desc").textContent=mode==="ai"?"整份清单一次请求 API；优先复用现有目录":"按扩展名规则分类；不调用网络 API";
  el("btn-plan").textContent=mode==="ai"?"✦ 生成整理方案":"◇ 按规则规划";
}
async function loadHistory(){
  try{
    state.history=await invoke("list_history");
    el("hist-count").textContent=state.history.length;
    el("history-total").textContent="("+state.history.length+")";
    renderHistory();
  }catch(e){notify("读取历史失败："+e,true);}
}
function dateOf(seconds){const n=Number(seconds);return Number.isFinite(n)?new Date(n*1000).toLocaleString("zh-CN",{hour12:false}):"未知时间";}
function statusName(s){return {completed:"已完成",partial:"中断",undone:"已撤销",partial_undo:"部分撤销",in_progress:"待恢复"}[s]||s;}
function renderHistory(){
  const records=el("history-records");
  if(!state.history.length){
    records.innerHTML='<div class="empty"><div class="empty-mark">◷</div><strong>暂无整理记录</strong><span>完成第一次整理后会出现在这里</span></div>';
    el("history-detail").innerHTML='<div class="empty"><div class="empty-mark">▤</div><strong>还没有记录可查看</strong></div>';
    return;
  }
  if(!state.historyId||!state.history.some(h=>h.id===state.historyId))state.historyId=state.history[0].id;
  records.innerHTML=state.history.map(h=>
    '<div class="history-entry '+(state.historyId===h.id?'selected':'')+'" data-id="'+esc(h.id)+'"><strong>'+esc(dateOf(h.created))+'</strong><small title="'+esc(h.root)+'">'+esc(h.root)+'</small><span>'+esc(statusName(h.status))+' · '+h.count+' 项</span></div>'
  ).join("");
  const h=state.history.find(x=>x.id===state.historyId);
  el("history-detail").innerHTML='<div class="history-detail-head"><div><h2>整理详情</h2><p>'+esc(dateOf(h.created))+'</p></div><button id="btn-undo" class="primary-btn" '+(h.status==="undone"||state.busy?"disabled":"")+'>↶ 撤销本次整理</button></div>'+
    '<p><strong>目录：</strong>'+esc(h.root)+'</p><p><strong>状态：</strong>'+esc(statusName(h.status))+' · '+h.count+' 个计划动作</p>'+
    (h.error?'<p style="color:var(--red)"><strong>提示：</strong>'+esc(h.error)+'</p>':'')+
    '<div class="operation-list">'+h.operations.map(op=>'<div class="operation"><strong title="'+esc(op.name)+'">'+esc(op.name)+'</strong><span>→ '+esc(op.category)+' · '+esc(op.state)+'</span></div>').join("")+'</div>';
}
function doUndo(){
  const h=state.history.find(x=>x.id===state.historyId);
  if(!h||h.status==="undone")return;
  ask("撤销这次整理？","将按相反顺序恢复文件，逐项核对文件状态。若文件被修改或原位置已有同名文件，会跳过并提示，不会覆盖。",async()=>{
    await run(async()=>{
      const result=await invoke("undo_history",{id:h.id});
      await loadHistory();
      notify("已恢复 "+result.restored+" 项"+(result.errors.length?"；"+result.errors.length+" 项需人工处理":""));
      if(result.errors.length)notify(result.errors.slice(0,3).join("；"),true);
      if(state.scan&&state.scan.root===h.root)invalidate();
    },"逐项恢复文件");
  });
}
function ask(title,message,onConfirm){
  el("modal-title").textContent=title;el("modal-message").textContent=message;
  el("modal-backdrop").hidden=false;
  const close=()=>{el("modal-backdrop").hidden=true;el("modal-confirm").onclick=null;};
  el("modal-cancel").onclick=close;
  el("modal-confirm").onclick=async()=>{close();try{await onConfirm();}catch(e){notify(String(e),true);}};
}
async function start(){
  try {
    state.settings=await invoke("load_settings");
    populateSettings();
    await loadHistory();
  }catch(e){notify("初始化失败："+e,true);}
  renderAll();
}
document.querySelectorAll(".nav-item[data-page]").forEach(btn=>btn.addEventListener("click",()=>show(btn.dataset.page)));
document.querySelectorAll("[data-mode]").forEach(btn=>btn.addEventListener("click",()=>setMode(btn.dataset.mode)));
el("open-settings").onclick=()=>show("settings");
el("choose-folder").onclick=chooseFolder;
el("btn-scan").onclick=scanFolder;
el("btn-rescan").onclick=scanFolder;
el("btn-plan").onclick=requestPlan;
el("repair-plan").onclick=repair;
el("btn-apply").onclick=applyPlan;
el("folder-input").addEventListener("change",()=>{if(el("folder-input").value.trim()!==state.scan?.root)invalidate();});
el("include-folders").addEventListener("change",()=>{el("cfg-folders").checked=el("include-folders").checked;invalidate();});
el("search-file").addEventListener("input",event=>{state.search=event.target.value;renderTable();});
el("status-filter").addEventListener("change",event=>{state.filter=event.target.value;renderTable();});
el("file-rows").addEventListener("change",async event=>{
  if(event.target.classList.contains("category-input"))await editCategory(event.target.dataset.id,event.target.value);
  if(event.target.classList.contains("row-select")){
    if(event.target.checked)state.selected.add(event.target.dataset.id);else state.selected.delete(event.target.dataset.id);
    renderTable();
  }
});
el("select-all").onchange=event=>{
  for(const item of filteredItems()){if(event.target.checked)state.selected.add(item.id);else state.selected.delete(item.id);}
  renderTable();
};
el("clear-selection").onclick=()=>{state.selected.clear();renderTable();};
el("bulk-apply").onclick=async()=>{
  if(!state.scan)return;
  const cat=el("bulk-category").value.trim();
  for(const id of state.selected)putAssignment(id,cat);
  state.selected.clear();await validatePreview();renderAll();notify("批量分类已更新");
};
el("refresh-history").onclick=loadHistory;
el("history-records").addEventListener("click",event=>{
  const entry=event.target.closest(".history-entry");
  if(entry){state.historyId=entry.dataset.id;renderHistory();}
});
el("history-detail").addEventListener("click",event=>{if(event.target.closest("#btn-undo"))doUndo();});
el("save-config").onclick=()=>run(async()=>{
  state.settings=settingsFromUI();
  await invoke("save_settings",{settings:state.settings});
  document.body.dataset.theme=state.settings.theme;
  el("include-folders").checked=state.settings.includeFolders;
  notify("设置已保存（API Key 只在当前会话内有效）");
},"保存设置");
el("cfg-theme").onchange=()=>document.body.dataset.theme=el("cfg-theme").value;
el("cfg-folders").onchange=()=>{el("include-folders").checked=el("cfg-folders").checked;invalidate();};
el("cfg-key").addEventListener("input",event=>{state.apiKey=event.target.value;});
el("toggle-key").onclick=()=>{el("cfg-key").type=el("cfg-key").type==="password"?"text":"password";};
el("fetch-models").onclick=()=>run(async()=>{
  const models=await invoke("fetch_models",{endpoint:el("cfg-endpoint").value.trim(),apiKey:state.apiKey});
  el("model-options").innerHTML=models.map(name=>'<option value="'+esc(name)+'"></option>').join("");
  notify("已获取 "+models.length+" 个模型；在模型输入框中选择");
},"正在获取模型列表");
start();
