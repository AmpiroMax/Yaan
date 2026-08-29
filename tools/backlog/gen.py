# -*- coding: utf-8 -*-
#
# Module: tools/backlog
# File: tools/backlog/gen.py
#
# Responsibility:
# - Генератор docs/plans/BACKLOG.html из data.py: сортировка задач по приоритету, нумерация ключей, секция «В работе», проверка дублей (--check).
#
# Dependencies:
# - Uses: python3 stdlib. Used by: координатор; `python3 tools/backlog/gen.py docs/plans/BACKLOG.html`.
#
# AI Agents Notice (must follow):
# - Follow docs/ARCHITECTURE.md strictly. Доска — правило 18a: задачи берутся отсюда и возвращаются сюда.
#
import sys, re, html, itertools, collections
sys.path.insert(0, __file__.rsplit('/',1)[0])
from data import EPICS, WIP
RANK={'B':0,'H':1,'M':2,'L':3}
EPICS=[(ek,et,w,sorted(t,key=lambda x:RANK[x[1]])) for ek,et,w,t in EPICS]
EPICS=[e for e in EPICS if e[0]=='E13']+[e for e in EPICS if e[0]!='E13']
PR={'B':('Блокер','#b22'),'H':('Высокий','#c60'),'M':('Средний','#27a'),'L':('Низкий','#777')}
ST={'':'к работе','part':'в работе','done':'сделано'}
def words(s): return set(w for w in re.findall(r'[а-яёa-z0-9]{4,}', s.lower()))
def check():
    items=[]
    for ek,et,_,tasks in EPICS:
        for ti,(tt,pr,st,subs,note) in enumerate(tasks,1):
            items.append((f'{ek}-{ti}',tt))
            for si,s in enumerate(subs,1): items.append((f'{ek}-{ti}.{si}',s))
    dups=[]
    for (a,ta),(b,tb) in itertools.combinations(items,2):
        wa,wb=words(ta),words(tb)
        if len(wa)>=3 and len(wb)>=3:
            j=len(wa&wb)/len(wa|wb)
            if j>=0.28: dups.append((round(j,2),a,ta,b,tb))
    return items,sorted(dups,reverse=True)
def render():
    items,dups=check()
    ne=len(EPICS); nt=sum(len(t) for *_,t in EPICS); ns=sum(len(s) for *_,t in EPICS for _,_,_,s,_ in t)
    out=[]
    out.append(f'''<!doctype html>
<!--
Module: docs/plans
File: docs/plans/BACKLOG.html

Responsibility:
- Полный бэклог проекта в форме Jira: эпики → задачи → подзадачи, порядок —
  сначала блокеры, потом крупное и важное. Источник — scratchpad генератор
  координатора (data.py); правится данными, не руками в html.
-->
<html lang="ru"><head><meta charset="utf-8"><title>Бэклог Daggerfall N — эпики, задачи, подзадачи</title>
<style>body{{font:15px/1.45 -apple-system,Segoe UI,sans-serif;max-width:64em;margin:2em auto;padding:0 1em;color:#222;background:#fff}}
h1{{font-size:1.5em}}h2{{margin-top:2em;padding:.3em .5em;background:#f1f1f1;border-left:5px solid #444}}
.why{{color:#555;margin:.2em 0 1em;font-style:italic}}
.task{{margin:.8em 0 .8em .5em;border:1px solid #ddd;border-radius:4px;padding:.5em .8em}}
.task h3{{margin:0;font-size:1.05em}} .key{{font-family:Menlo,monospace;color:#666;font-size:.85em;margin-right:.5em}}
.badge{{display:inline-block;color:#fff;border-radius:3px;padding:0 .4em;font-size:.8em;margin-left:.4em}}
.st{{display:inline-block;border:1px solid #999;border-radius:3px;padding:0 .4em;font-size:.8em;margin-left:.4em;color:#444}}
ol{{margin:.4em 0 .2em 1.2em}} li{{margin:.15em 0}} .note{{color:#555;font-size:.9em;margin-top:.3em}}
table{{border-collapse:collapse;margin:1em 0}} td,th{{border:1px solid #ccc;padding:.2em .6em}}
</style></head><body>
<h1>Бэклог проекта — эпики, задачи, подзадачи</h1>
<p>Состояние на 29.08.2026. Порядок эпиков и задач: сначала блокеры, затем по важности и размеру. Ключи задач — по образцу Jira: <code>E4-2</code> — задача, <code>E4-2.3</code> — подзадача.</p>
<p><b>Итого: {ne} эпиков, {nt} задач, {ns} подзадач — {nt+ns} тикетов.</b></p>
<table><tr><th>Эпик</th><th>Задач</th><th>Подзадач</th><th>Блокеров</th></tr>''')
    for ek,et,_,tasks in EPICS:
        out.append(f'<tr><td><a href="#{ek}">{ek}. {html.escape(et)}</a></td><td>{len(tasks)}</td><td>{sum(len(s) for _,_,_,s,_ in tasks)}</td><td>{sum(1 for _,p,*_ in tasks if p=="B")}</td></tr>')
    out.append('</table>')
    out.append('<h2 id="wip">В работе сейчас</h2><table><tr><th>Ключ</th><th>Задача</th><th>Кто</th><th>С</th><th>Состояние / обновление</th></tr>')
    tt={f'{ek}-{ti}':t[0] for ek,_,_,tasks in EPICS for ti,t in enumerate(tasks,1)}
    for k,who,since,upd in WIP:
        out.append(f'<tr><td><a href="#{k}">{k}</a></td><td>{html.escape(tt.get(k,"?"))}</td><td>{who}</td><td>{since}</td><td>{html.escape(upd)}</td></tr>')
    if not WIP: out.append('<tr><td colspan=5>ничего — разработка остановлена</td></tr>')
    out.append('</table>')
    for ek,et,why,tasks in EPICS:
        out.append(f'<h2 id="{ek}">{ek}. {html.escape(et)}</h2><p class="why">{html.escape(why)}</p>')
        for ti,(tt,pr,st,subs,note) in enumerate(tasks,1):
            pn,pc=PR[pr]
            out.append(f'<div class="task" id="{ek}-{ti}"><h3><span class="key">{ek}-{ti}</span>{html.escape(tt)}<span class="badge" style="background:{pc}">{pn}</span><span class="st">{ST[st]}</span></h3><ol>')
            for si,s in enumerate(subs,1):
                out.append(f'<li><span class="key">{ek}-{ti}.{si}</span>{html.escape(s)}</li>')
            out.append('</ol>'+(f'<div class="note">{html.escape(note)}</div>' if note else '')+'</div>')
    out.append('</body></html>')
    return '\n'.join(out), items, dups
if __name__=='__main__':
    h,items,dups=render()
    if '--check' in sys.argv:
        print(len(items),'items;',len(dups),'possible duplicates')
        for d in dups[:40]: print(d)
    else:
        open(sys.argv[1],'w').write(h); print('written', len(items))
