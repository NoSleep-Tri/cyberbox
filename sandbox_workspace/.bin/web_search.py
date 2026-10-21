import sys, urllib.request, urllib.parse, html, re
if len(sys.argv) < 2:
    print('[Usage] web_search "<tu khoa>"')
    sys.exit(0)
q = ' '.join(sys.argv[1:]).strip()
print('[*] Dang tim kiem truc tuyen:', q, '\n')
try:
    pdata = urllib.parse.urlencode({'q': q}).encode('utf-8')
    req = urllib.request.Request('https://lite.duckduckgo.com/lite/', data=pdata, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=12) as resp:
        content = resp.read().decode('utf-8', 'ignore')
    raw_links = re.findall(r'<a[^>]+result-link[^>]*>.*?</a>', content, re.DOTALL)
    snippets = re.findall(r'<td class=['\"]result-snippet['\"]>(.*?)</td>', content, re.DOTALL)
    parsed = []
    for l in raw_links:
        mh = re.search(r'href=['\"]([^'\"]+)['\"]', l)
        mt = re.search(r'>([^<]+)<', l)
        u = mh.group(1) if mh else ''
        t = mt.group(1) if mt else 'Link'
        if u and not u.startswith('/'): parsed.append((u, t))
    total = min(len(parsed), len(snippets), 5)
    if total == 0:
        print('[-] Khong tim thay ket qua phu hop.')
    else:
        print(f'[+] Tim thay {total} ket qua hang dau:\n')
        for i in range(total):
            url, title = parsed[i]
            title = html.unescape(title).strip()
            snip = html.unescape(re.sub(r'<[^>]+>', '', snippets[i])).strip()
            print(f'[{i+1}] {title}\n    {snip}\n    Link: {url}\n')
except Exception as e:
    print('[Error] Loi khi tim kiem:', e)
