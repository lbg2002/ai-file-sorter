"""Compact, accessible Tk desktop shell (Linux/Windows, no third-party packages)."""
from __future__ import annotations

import json
import queue
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, simpledialog, ttk

from . import core

BG = "#151b27"
PANEL = "#202939"
INPUT = "#2a3546"
FG = "#f3f6fc"
MUTED = "#a8b6ca"
ACCENT = "#78aaff"


class Desktop(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("FileSort Lite · AI 文件整理")
        self.geometry("1140x730")
        self.minsize(850, 560)
        self.configure(bg=BG)
        self.items: list[dict] = []
        self.root_path: Path | None = None
        self.assignments: dict[str, str] = {}
        self.parse_issues: list[str] = []
        self.events: queue.Queue = queue.Queue()
        self.busy = False
        self.last_journal: Path | None = None
        self._style()
        self._build()
        self._load_settings()
        self._refresh_history()
        self.after(100, self._drain)

    def _style(self):
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure(".", font=("Noto Sans CJK SC" if self.tk.call("tk", "windowingsystem") == "x11" else "Segoe UI", 10))
        style.configure("Treeview", background=PANEL, foreground=FG, fieldbackground=PANEL, borderwidth=0, rowheight=29)
        style.configure("Treeview.Heading", background=INPUT, foreground=FG, relief="flat", font=("Segoe UI", 10, "bold"))
        style.map("Treeview", background=[("selected", "#355b94")], foreground=[("selected", "#ffffff")])
        style.configure("Vertical.TScrollbar", background=INPUT, troughcolor=PANEL)

    def _label(self, parent, text, size=10, color=FG, bold=False):
        return tk.Label(parent, text=text, bg=parent["bg"], fg=color, font=("Segoe UI", size, "bold" if bold else "normal"), anchor="w")

    def _entry(self, parent, variable, show=None, width=17):
        return tk.Entry(parent, textvariable=variable, show=show, width=width, bg=INPUT, fg=FG,
                        insertbackground=FG, relief="flat", highlightthickness=1, highlightbackground="#35445a", highlightcolor=ACCENT)

    def _button(self, parent, text, command, primary=False):
        bg = "#3c76d2" if primary else INPUT
        return tk.Button(parent, text=text, command=command, bg=bg, fg="#ffffff", activebackground="#508cdc",
                         activeforeground="#ffffff", relief="flat", borderwidth=0, padx=14, pady=9, cursor="hand2")

    def _build(self):
        body = tk.Frame(self, bg=BG)
        body.pack(fill="both", expand=True, padx=20, pady=16)
        head = tk.Frame(body, bg=BG)
        head.pack(fill="x", pady=(0, 16))
        self._label(head, "FileSort Lite", 22, FG, True).pack(side="left")
        self._label(head, "  ·  先预览，再移动，可撤销", 10, MUTED).pack(side="left", padx=10, pady=(11, 0))
        left = tk.Frame(body, bg=PANEL, width=246)
        left.pack(side="left", fill="y", padx=(0, 14))
        left.pack_propagate(False)
        self._label(left, "整理记录", 13, FG, True).pack(anchor="w", padx=15, pady=(18, 9))
        hist_frame = tk.Frame(left, bg=PANEL)
        hist_frame.pack(fill="both", expand=True, padx=9)
        self.history_list = tk.Listbox(hist_frame, bg=PANEL, fg=FG, selectbackground="#355b94", relief="flat", borderwidth=0,
                                      activestyle="none", highlightthickness=0, font=("Segoe UI", 10))
        self.history_list.pack(fill="both", expand=True)
        self.history_list.bind("<<ListboxSelect>>", self._show_history)
        self._button(left, "撤销所选记录", self._undo).pack(fill="x", padx=14, pady=(12, 16))
        main = tk.Frame(body, bg=BG)
        main.pack(side="left", fill="both", expand=True)

        config = tk.Frame(main, bg=PANEL, padx=14, pady=12)
        config.pack(fill="x")
        self.folder = tk.StringVar(value=str(Path.home() / "Downloads"))
        self.endpoint = tk.StringVar(value="https://api.openai.com/v1")
        self.model = tk.StringVar(value="gpt-4.1-mini")
        self.key = tk.StringVar()
        self.include_dirs = tk.BooleanVar(value=False)
        folderrow = tk.Frame(config, bg=PANEL)
        folderrow.pack(fill="x", pady=(0, 10))
        self._label(folderrow, "目标文件夹", 10, MUTED).pack(side="left", padx=(0, 10))
        self._entry(folderrow, self.folder, width=42).pack(side="left", fill="x", expand=True, ipady=7)
        self._button(folderrow, "浏览…", self._browse).pack(side="left", padx=(8, 0))
        apir = tk.Frame(config, bg=PANEL)
        apir.pack(fill="x", pady=(0, 8))
        self._label(apir, "API", color=MUTED).pack(side="left", padx=(0, 6))
        self._entry(apir, self.endpoint, width=32).pack(side="left", fill="x", expand=True, ipady=7, padx=(0, 8))
        self._label(apir, "模型", color=MUTED).pack(side="left", padx=(0, 6))
        self._entry(apir, self.model, width=16).pack(side="left", ipady=7)
        self._label(apir, "密钥", color=MUTED).pack(side="left", padx=(8, 6))
        self._entry(apir, self.key, show="●", width=16).pack(side="left", ipady=7)
        action = tk.Frame(config, bg=PANEL)
        action.pack(fill="x")
        tk.Checkbutton(action, text="包含顶层文件夹（默认不移动）", variable=self.include_dirs, bg=PANEL, fg=FG,
                       selectcolor=INPUT, activeforeground=FG, activebackground=PANEL).pack(side="left")
        self._button(action, "扫描", self._scan).pack(side="right", padx=(6, 0))
        self._button(action, "AI 一次规划", self._analyze, True).pack(side="right", padx=(6, 0))

        self._label(main, "预览与校验", 14, FG, True).pack(anchor="w", pady=(19, 8))
        tableframe = tk.Frame(main, bg=PANEL)
        tableframe.pack(fill="both", expand=True)
        self.table = ttk.Treeview(tableframe, columns=("name", "kind", "category", "status"), show="headings", selectmode="browse")
        for col, title, width, stretch in [("name", "项目", 285, True), ("kind", "类型", 67, False),
                                           ("category", "目标分类（双击编辑）", 240, True), ("status", "校验", 110, False)]:
            self.table.heading(col, text=title)
            self.table.column(col, width=width, minwidth=width // 2, stretch=stretch)
        self.table.tag_configure("bad", foreground="#ffa5a5")
        self.table.tag_configure("good", foreground="#ace4c5")
        self.table.tag_configure("skip", foreground=MUTED)
        scrollbar = ttk.Scrollbar(tableframe, orient="vertical", command=self.table.yview)
        self.table.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side="right", fill="y")
        self.table.pack(side="left", fill="both", expand=True)
        self.table.bind("<Double-1>", self._edit_category)
        foot = tk.Frame(main, bg=BG)
        foot.pack(fill="x", pady=(12, 0))
        self.state_label = self._label(foot, "选择文件夹并扫描。密钥只保存在当前窗口内。", 10, MUTED)
        self.state_label.pack(side="left", fill="x", expand=True)
        self.apply_btn = self._button(foot, "确认移动", self._apply, True)
        self.apply_btn.pack(side="right")
        self.apply_btn.configure(state="disabled")

    def _load_settings(self):
        try:
            prefs = json.loads((core.data_dir() / "settings.json").read_text(encoding="utf-8"))
            self.endpoint.set(prefs.get("endpoint", self.endpoint.get()))
            self.model.set(prefs.get("model", self.model.get()))
        except (OSError, ValueError, TypeError):
            pass

    def _save_settings(self):
        try:
            core._atomic_json(core.data_dir() / "settings.json", {"endpoint": self.endpoint.get(), "model": self.model.get()})
        except OSError:
            pass  # API configuration is convenient but optional; never persist the key

    def _browse(self):
        path = filedialog.askdirectory(initialdir=self.folder.get())
        if path:
            self.folder.set(path)
            self._scan()
    def _notify(self, msg, error=False):
        self.state_label.configure(text=msg[:150], fg="#ffb3b3" if error else MUTED)

    def _scan(self):
        if self.busy:
            return False
        try:
            self.root_path, self.items, ignored = core.scan(self.folder.get(), self.include_dirs.get())
            self.assignments = {}
            self.parse_issues = []
            self._render()
            self._notify(f"已扫描 {len(self.items)} 项；跳过 {len(ignored)} 项。未生成方案。")
            return True
        except Exception as ex:
            self.root_path, self.items, self.assignments = None, [], {}
            self._render()
            self._notify(str(ex), True)
            return False

    def _render(self):
        for row in self.table.get_children():
            self.table.delete(row)
        if self.root_path is None:
            return
        status, issues = core.validate(self.root_path, self.items, self.assignments, self.parse_issues)
        for item in self.items:
            id_ = item["id"]
            value = self.assignments.get(id_)
            state = status[id_]
            tag = "good" if state == "可移动" else ("skip" if state == "保留原位" else "bad")
            self.table.insert("", "end", iid=id_, values=(item["name"], "文件夹" if item["kind"] == "folder" else "文件", value if value is not None and value != "" else ("不移动" if value == "" else "—"), state), tags=(tag,))
        n = sum(v == "可移动" for v in status.values())
        self.apply_btn.configure(state="normal" if not issues and n and not self.busy else "disabled")
        if self.assignments:
            self._notify(f"待移动 {n} 项 · 保留 {sum(v == '保留原位' for v in status.values())} 项 · 异常 {len(issues)} 项" + (" · " + "; ".join(issues[:2]) if issues else ""), bool(issues))

    def _edit_category(self, event):
        row_id = self.table.identify_row(event.y)
        if not row_id:
            return
        current = self.assignments.get(row_id, "")
        name = self.table.item(row_id)["values"][0]
        new = simpledialog.askstring("修改分类", f"{name}\n请输入单层目标文件夹名；留空表示不移动：", initialvalue=current, parent=self)
        if new is None:
            return
        self.assignments[row_id] = new.strip()
        self.parse_issues = []  # stale model issues are replaced by a fully visible, locally reviewed plan
        self._render()

    def _run(self, work, on_success):
        if self.busy:
            return
        self.busy = True
        self.apply_btn.configure(state="disabled")
        def task():
            try:
                self.events.put(("ok", work(), on_success))
            except Exception as ex:
                self.events.put(("error", str(ex), None))
        threading.Thread(target=task, daemon=True).start()

    def _drain(self):
        try:
            while True:
                status, result, callback = self.events.get_nowait()
                self.busy = False
                if status == "ok":
                    callback(result)
                else:
                    self._notify(result, True)
                    messagebox.showerror("操作失败", result)
                self._render()
        except queue.Empty:
            pass
        self.after(100, self._drain)

    def _analyze(self):
        if not self._scan():
            return
        root, items = self.root_path, list(self.items)
        endpoint, model, key = self.endpoint.get(), self.model.get(), self.key.get()
        self._save_settings()
        self._run(lambda: core.parse_plan(core.request_plan(root, items, endpoint, model, key), items), self._plan_ready)

    def _plan_ready(self, result):
        self.assignments, self.parse_issues = result
        self._render()
        if self.parse_issues:
            messagebox.showwarning("模型输出校验", "\n".join(self.parse_issues[:12]) + "\n请检查后重新生成方案")

    def _apply(self):
        if not self.root_path or self.busy:
            return
        if not messagebox.askyesno("确认移动", "即将按预览结果移动文件。原文件夹默认保持原位。\n将保存可撤销操作日志。继续？"):
            return
        root, items, assignments = self.root_path, list(self.items), dict(self.assignments)
        self._notify("正在逐项安全移动并记录日志…")
        self._run(lambda: core.apply_plan(root, items, assignments), self._applied)

    def _applied(self, journal):
        self.last_journal = journal
        self._refresh_history()
        self._scan()
        messagebox.showinfo("完成", f"整理完成，已记录 {journal.name}\n如需撤销，请在左侧选中该记录。")

    def _refresh_history(self):
        self.history_files = core.history()
        self.history_list.delete(0, "end")
        for p in self.history_files[:100]:
            try:
                j = json.loads(p.read_text(encoding="utf-8"))
                count = sum(x["state"] == "done" for x in j["operations"])
                self.history_list.insert("end", f"{j['created'][:16]} · {count}项 · {j['status']}")
            except (ValueError, OSError, KeyError):
                self.history_list.insert("end", p.stem)

    def _selected_history(self):
        idx = self.history_list.curselection()
        if not idx:
            return None
        return self.history_files[idx[0]]

    def _show_history(self, _event):
        path = self._selected_history()
        if path:
            try:
                job = json.loads(path.read_text(encoding="utf-8"))
                self._notify(f"记录：{job['root']} | {len(job['operations'])} 个移动 | {job['status']}")
            except (ValueError, OSError, KeyError):
                pass

    def _undo(self):
        path = self._selected_history()
        if path is None:
            messagebox.showinfo("撤销", "请先选择一条整理记录")
            return
        if not messagebox.askyesno("确认撤销", "将按相反顺序恢复文件；已被修改或占用的文件会跳过，不会覆盖任何现有内容。继续？"):
            return
        self._run(lambda: core.undo(path), self._undone)

    def _undone(self, result):
        count, issues = result
        self._refresh_history()
        self._scan()
        messagebox.showinfo("撤销结果", f"已恢复 {count} 项" + ("\n" + "\n".join(issues[:12]) if issues else ""))


def main():
    Desktop().mainloop()


if __name__ == "__main__":
    main()