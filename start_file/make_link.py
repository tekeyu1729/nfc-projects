import sys
import re
import os

target_url = None
printed = False

# HTMLファイルを更新する関数
def update_html(new_url):
    html_path_0 = os.path.join(os.path.dirname(__file__), "..", "nfc-projects", "templates", "stump_all.html")
    html_path_1 = os.path.join(os.path.dirname(__file__), "..", "nfc-projects", "templates", "admin_all.html")
    if not os.path.exists(html_path_0):
        return
    if not os.path.exists(html_path_1):
            return

    # 新しいドメイン（末尾の/を除去）
    base_url = new_url.rstrip("/")

    with open(html_path_0, "r", encoding="utf-8") as f:
        content_0 = f.read()
    with open(html_path_1, "r", encoding="utf-8") as f:
        content_1 = f.read()

    # https://xxxx.trycloudflare.com 部分を新しいドメインに置換
    updated_content_0 = re.sub(
        r'https://[a-zA-Z0-9-]+\.trycloudflare\.com',
        base_url,
        content_0
    )
    updated_content_1 = re.sub(
        r'https://[a-zA-Z0-9-]+\.trycloudflare\.com',
        base_url,
        content_1
    )

    with open(html_path_0, "w", encoding="utf-8") as f:
        f.write(updated_content_0)
    with open(html_path_1, "w", encoding="utf-8") as f:
        f.write(updated_content_1)

    print(f"\n[HTML更新完了] {html_path_0} , {html_path_1} のURLを '{base_url}' に書き換えました。")

asd = [647899,307616,832904,846782,675845]
def make_link(url):
    a = url.rstrip("/") + "/"
    print("\n==========================================")
    print("=== 生成されたURL一覧 ===")
    print(a + "stamp/all")
    print(a + "stamp")
    for i in range(5):
        print(f"{a}stamp?id={asd[i]}")
    print(f"{a}stamp?id={asd[0]}&id={asd[1]}&id={asd[2]}&id={asd[3]}&id={asd[4]}")
    print(a + "admin/all")
    print(a + "admin")
    print(a + "admin/checkr")
    print("==========================================\n")

for line in sys.stdin:
    # 1. URLの検出
    match = re.search(r'https://[a-zA-Z0-9-]+\.trycloudflare\.com', line)
    if match:
        target_url = match.group(0)

    # ログ行をそのまま標準出力（リアルタイム表示）
    print(line, end="", flush=True)

    # 2. 接続完了ログの検知
    line_lower = line.lower()
    if ("registered" in line_lower and "connection" in line_lower) or "precheck complete" in line_lower:
        if target_url and not printed:
            make_link(target_url)
            update_html(target_url)  # HTML書き換えの実行
            printed = True