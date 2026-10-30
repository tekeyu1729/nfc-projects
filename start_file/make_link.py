import sys
import re
import os
import json

target_url = None
printed = False

# スタンプIDを読み込む関数
def load_stamps():
    asd = []
    # スクリプトがある場所を基準に絶対パスを算出する
    base_dir = os.path.dirname(os.path.abspath(__file__))
    stamp_path = os.path.join(base_dir, "..", "nfc-projects", "static", "stamp.json")
    
    with open(stamp_path, "r", encoding="utf-8") as f:
        id_list = json.load(f)
        if isinstance(id_list, list):
            for item in id_list:
                # stamp.json のキー名 "id" に合わせて修正
                if isinstance(item, dict) and "id" in item:
                    asd.append(item["id"])
    return asd

# HTMLファイルを更新する関数
def update_html(new_url, asd):
    base_dir = os.path.dirname(os.path.abspath(__file__))
    html_path_0 = os.path.join(base_dir, "..", "nfc-projects", "templates", "stump_all.html")
    html_path_1 = os.path.join(base_dir, "..", "nfc-projects", "templates", "admin_all.html")
    
    base_url = new_url.rstrip("/")

    if os.path.exists(html_path_0):
        with open(html_path_0, "r", encoding="utf-8") as f:
            content_0 = f.read()
        updated_content_0 = re.sub(r'https://[a-zA-Z0-9-]+\.trycloudflare\.com', base_url, content_0)
        with open(html_path_0, "w", encoding="utf-8") as f:
            f.write(updated_content_0)

    if os.path.exists(html_path_1):
        with open(html_path_1, "r", encoding="utf-8") as f:
            content_1 = f.read()
        updated_content_1 = re.sub(r'https://[a-zA-Z0-9-]+\.trycloudflare\.com', base_url, content_1)
        with open(html_path_1, "w", encoding="utf-8") as f:
            f.write(updated_content_1)

    print(f"\n[HTML更新完了] URLを '{base_url}' に書き換えました。")


def make_link(url, asd):
    a = url.rstrip("/") + "/"
    print("\n==========================================")
    print("=== 生成されたURL一覧 ===")
    print(a + "stamp/all")
    print(a + "stamp")
    
    if len(asd) == 0:
        print("[警告] スタンプIDが読み込まれていないため、個別URLはスキップします。")
    else:
        limit = min(5, len(asd))
        for i in range(limit):
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

    # 2. 接続完了ログの検知[cite: 1]
    line_lower = line.lower()
    if ("registered" in line_lower and "connection" in line_lower) or "precheck complete" in line_lower:
        if target_url and not printed:
            # 実行時に一度だけスタンプデータを読み込む
            asd = load_stamps()
            make_link(target_url, asd)
            update_html(target_url, asd)
            printed = True