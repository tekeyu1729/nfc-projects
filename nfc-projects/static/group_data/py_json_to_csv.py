
import json
import os
import sys

file_json = os.path.join(os.path.dirname(__file__), "group_data.json")
file_txt = os.path.join(os.path.dirname(__file__), "group_data.csv")

# JSONを読み込む
try:
    with open(file_json, 'r', encoding='utf-8') as f:
        data_list = json.load(f)
except json.JSONDecodeError as e:
    print(f"JSONの形式に問題があります: {e}")
    sys.exit()
except FileNotFoundError:
    print("group_data.json が見つかりません")
    sys.exit()

# JSONのデータ不足を確認
required_keys = [
    "id", "floor", "roomName", "groupName",
    "title", "desc", "mainMap", "modalArea"
]

has_error = False

for i, item in enumerate(data_list):
    # データが辞書形式か確認
    if not isinstance(item, dict):
        print(f"{i+1}個目のデータがオブジェクト形式ではありません")
        has_error = True
        continue

    # 必須キーを確認
    for key in required_keys:
        if key not in item:
            print(f"{i+1}個目のデータに「{key}」がありません")
            has_error = True

    # mainMapの存在と形式を確認
    if "mainMap" in item:
        if not isinstance(item["mainMap"], dict):
            print(f"{i+1}個目のデータのmainMapがオブジェクト形式ではありません")
            has_error = True
        else:
            for key in ["x", "y"]:
                if key not in item["mainMap"]:
                    print(f"{i+1}個目のデータのmainMapに「{key}」がありません")
                    has_error = True

    # modalAreaの存在と形式を確認
    if "modalArea" in item:
        if not isinstance(item["modalArea"], dict):
            print(f"{i+1}個目のデータのmodalAreaがオブジェクト形式ではありません")
            has_error = True
        else:
            for key in ["left", "top", "width", "height"]:
                if key not in item["modalArea"]:
                    print(f"{i+1}個目のデータのmodalAreaに「{key}」がありません")
                    has_error = True

    # 必須項目が空でないか確認
    for key in ["id", "floor", "roomName", "groupName", "title", "desc"]:
        if key in item and (
            item[key] is None or
            (isinstance(item[key], str) and item[key].strip() == "")
        ):
            print(f"{i+1}個目のデータの「{key}」が空です")
            has_error = True

    if isinstance(item.get("mainMap"), dict):
        for key in ["x", "y"]:
            value = item["mainMap"].get(key)
            if key in item["mainMap"] and (
                value is None or
                (isinstance(value, str) and value.strip() == "")
            ):
                print(f"{i+1}個目のデータのmainMapの「{key}」が空です")
                has_error = True

    if isinstance(item.get("modalArea"), dict):
        for key in ["left", "top", "width", "height"]:
            value = item["modalArea"].get(key)
            if key in item["modalArea"] and (
                value is None or
                (isinstance(value, str) and value.strip() == "")
            ):
                print(f"{i+1}個目のデータのmodalAreaの「{key}」が空です")
                has_error = True

if has_error:
    print("JSONのデータに問題があるため、CSVの更新を中止しました")
    sys.exit()

# CSVのデータ量を確認
existing_lines = []

if os.path.exists(file_txt):
    with open(file_txt, 'r', encoding='utf-8') as f:
        raw_lines = f.read().splitlines()

    # 空白行を除外
    existing_lines = [
        line.strip()
        for line in raw_lines
        if line.strip()
    ]

    has_error = False

    for i, line in enumerate(existing_lines):
        cols = line.split(",")
        le_da = len(cols)

        if le_da == 12:
            continue
        elif le_da == 1:
            print(f"{i+1}行目にデータが1つしかありません")
            has_error = True
        elif le_da < 12:
            print(f"{i+1}行目にデータが{le_da}つしかありません")
            has_error = True
        else:
            print(f"{i+1}行目にデータが{le_da}つあります")
            has_error = True

    if has_error:
        print("CSVのデータ数に問題があるため、更新を中止しました")
        sys.exit()

# JSONのデータをCSV形式に変換
update_dict = {}

for item in data_list:
    item_id = str(item["id"])

    line_str = (
        f"{item['mainMap']['x']},{item['mainMap']['y']},"
        f"{item['modalArea']['left']},{item['modalArea']['top']},"
        f"{item['modalArea']['width']},{item['modalArea']['height']},"
        f"{item_id},{item['floor'].replace('F', '')},"
        f"{item['roomName']},{item['groupName']},"
        f"{item['title']},{item['desc']}"
    )

    update_dict[item_id] = line_str

# 既存のCSVを更新
updated_lines = []
updated_ids = set()

for line in existing_lines:
    cols = line.split(",")
    line_id = cols[6] if len(cols) > 6 else None

    if line_id in update_dict:
        updated_lines.append(update_dict[line_id])
        updated_ids.add(line_id)
    else:
        updated_lines.append(line)

# CSVに存在しないJSONのデータを追加
for item_id, line_str in update_dict.items():
    if item_id not in updated_ids:
        updated_lines.append(line_str)

# CSVへ保存
with open(file_txt, 'w', encoding='utf-8') as f:
    f.write("\n".join(updated_lines))

print("csvへ保存しました")