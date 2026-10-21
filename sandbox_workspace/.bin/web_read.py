import sys, urllib.request, re
if len(sys.argv) < 2:
    print('[Usage] web_read "<url>"')
    sys.exit(0)
u = sys.argv[1].strip()
if not u.startswith('http://') and not u.startswith('https://'): u = 'https://' + u
print('[*] Dang doc URL:', u)
try:
    req = urllib.request.Request(u, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=12) as resp:
        html = resp.read().decode('utf-8', 'ignore')
    html = re.sub(r'<(script|style|svg|noscript)[^>]*>.*?</\1>', '', html, flags=re.DOTALL|re.IGNORECASE)
    text = re.sub(r'<[^>]+>', ' ', html)
    text = re.sub(r'\s+', ' ', text).strip()
    if len(text) > 3000: text = text[:3000] + '\n... [Noi dung dai da rut gon]'
    print('\n--- [NOI DUNG TRANG WEB] ---\n' + text + '\n--- [HET TRANG WEB] ---\n')
except Exception as e:
    print('[Error] Khong the doc URL:', e)
