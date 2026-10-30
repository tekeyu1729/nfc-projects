import json
import os
import uuid
from functools import wraps
from flask import Flask, render_template, request, redirect, jsonify, make_response, Response

app = Flask(__name__)
# 定数・ファイルパス設定
TOTAL_STAMPS = []
STAMPS_FILE = 'static/stamp.json'
DB_FILE = 'user_db.json'

# --- 外部ファイル（JSON）の読み込み・保存処理 ---
with open(STAMPS_FILE, 'r', encoding='utf-8') as f:
  data = json.load(f)
  for i in range(len(data)):
    TOTAL_STAMPS.append(data[i]['id'])

def load_db():
    """起動時に JSON ファイルから user_db を読み込む"""
    if not os.path.exists(DB_FILE):
        return {}
    try:
        with open(DB_FILE, 'r', encoding='utf-8') as f:
            data = json.load(f)
            if not isinstance(data, dict):
                return {}
            # JSONの配列データをPythonのset型に復元＆各種キーの安全な補完
            for uid in list(data.keys()):
                if not isinstance(data[uid], dict):
                    data[uid] = {}
                stamps_list = data[uid].get("stamps", [])
                if not isinstance(stamps_list, list):
                    stamps_list = []
                data[uid]["stamps"] = set(stamps_list)
                if "completed" not in data[uid]:
                    data[uid]["completed"] = False
                if "rank" not in data[uid]:
                    data[uid]["rank"] = None
                if "has_seen_tutorial" not in data[uid]:
                    data[uid]["has_seen_tutorial"] = False
                if "checked" not in data[uid]:
                    data[uid]["checked"] = False
            return data
    except Exception as e:
        print(f"DB読み込みエラー: {e}")
        return {}

def save_db():
    """user_db の変更内容を JSON ファイルへ書き込む"""
    try:
        serializable_db = {}
        for uid, data in user_db.items():
            serializable_db[uid] = {
                "stamps": list(data["stamps"]),
                "completed": data["completed"],
                "rank": data.get("rank"),
                "has_seen_tutorial": data.get("has_seen_tutorial", False),
                "checked": data.get("checked", False)
            }
        with open(DB_FILE, 'w', encoding='utf-8') as f:
            json.dump(serializable_db, f, ensure_ascii=False, indent=2)
    except Exception as e:
        print(f"DB保存エラー: {e}")

# サーバー起動時にデータを読み込む
user_db = load_db()

# --- 管理者認証用設定 ---
ADMIN_USER = 'q1w2e3r4'
ADMIN_PASS = 'zaxscdvf'

def check_auth(username, password):
    return username == ADMIN_USER and password == ADMIN_PASS

def authenticate():
    return Response(
        '管理者ログインが必要です。', 401,
        {'WWW-Authenticate': 'Basic realm="Login Required"'}
    )

def requires_auth(f):
    @wraps(f)
    def decorated(*args, **kwargs):
        auth = request.authorization
        if not auth or not check_auth(auth.username, auth.password):
            return authenticate()
        return f(*args, **kwargs)
    return decorated

# --- ユーザー用機能 ---

def get_or_create_user(user_id):
    if not user_id or user_id not in user_db:
        user_id = str(uuid.uuid4())
        user_db[user_id] = {
            "stamps": set(),
            "completed": False,
            "rank": None,
            "has_seen_tutorial": False,
            "checked": False
        }
        save_db()
        return user_id, True
    return user_id, False

@app.route('/stamp')
def handle_stamp():
    user_id = request.cookies.get('user_id')
    user_id, is_new = get_or_create_user(user_id)

    # 複数の 'id' パラメータをリストとして取得
    stamp_ids = request.args.getlist('id')
    
    updated = False
    for s_id in stamp_ids:
        if s_id in TOTAL_STAMPS:
            user_db[user_id]["stamps"].add(s_id)
            updated = True
            
            if user_db[user_id]["stamps"] == set(TOTAL_STAMPS):
                user_db[user_id]["completed"] = True
                
                if user_db[user_id].get("rank") is None:
                    existing_ranks = [u["rank"] for u in user_db.values() if u.get("rank") is not None]
                    user_db[user_id]["rank"] = (max(existing_ranks) + 1) if existing_ranks else 1
                
    if updated:
        save_db()

    response = make_response(render_template('stamp.html'))
    if is_new:
        response.set_cookie('user_id', user_id, max_age=365*24*60*60, httponly=True)

    return response

@app.route('/api/stamps')
def get_stamps():
    user_id = request.cookies.get('user_id')

    if not user_id or user_id not in user_db:
        return jsonify({
            'userId': None, 
            'stamps': [], 
            'completed': False, 
            'rank': None,
            'hasSeenTutorial': False
        })

    user_data = user_db[user_id]
    return jsonify({
        'userId': user_id,
        'stamps': list(user_data["stamps"]),
        'completed': user_data["completed"],
        'rank': user_data.get("rank"),
        'hasSeenTutorial': user_data.get("has_seen_tutorial", False)
    })

@app.route('/api/tutorial/complete', methods=['POST'])
def complete_tutorial():
    user_id = request.cookies.get('user_id')
    
    if not user_id:
        data = request.get_json() or {}
        user_id = data.get('userId')

    if user_id and user_id in user_db:
        user_db[user_id]["has_seen_tutorial"] = True
        save_db()
        return jsonify({'success': True, 'message': 'Tutorial marked as completed'})

    return jsonify({'success': False, 'message': 'User not found'}), 404

@app.route('/stamp/all')
def show_stamp_all_page():
    return render_template('stamp_all.html')


# --- 管理者用機能 ---

@app.route('/admin')
@requires_auth
def admin_page():
    global user_db
    user_db = load_db()

    completed_ranks = [data["rank"] for data in user_db.values() if data.get("completed") and data.get("rank") is not None]
    max_rank = max(completed_ranks) if completed_ranks else 0

    incomplete_count = sum(1 for data in user_db.values() if not data.get("completed"))
    total_participants = max_rank + incomplete_count
    
    # ゴール者数を計算する処理を追加
    completed_participants = sum(1 for data in user_db.values() if data.get("completed"))

    formatted_db = {}
    for uid, data in user_db.items():
        formatted_db[uid] = {
            "stamps": list(data["stamps"]),
            "completed": data["completed"],
            "rank": data.get("rank"),
            "has_seen_tutorial": data.get("has_seen_tutorial", False),
            "checked": data.get("checked", False)
        }

    return render_template(
        'admin.html', 
        user_db=formatted_db, 
        total_participants=total_participants,
        completed_participants=completed_participants  # テンプレートへ渡す
    )

@app.route('/admin/update', methods=['POST'])
@requires_auth
def admin_update():
    user_id = request.form.get('user_id')
    stamp_id = request.form.get('stamp_id')
    action = request.form.get('action')

    if user_id in user_db and stamp_id:
        if action == 'add' and stamp_id in TOTAL_STAMPS:
            user_db[user_id]["stamps"].add(stamp_id)
        elif action == 'remove':
            user_db[user_id]["stamps"].discard(stamp_id)
        
        is_completed = (user_db[user_id]["stamps"] == set(TOTAL_STAMPS))
        user_db[user_id]["completed"] = is_completed
        if is_completed and user_db[user_id].get("rank") is None:
            existing_ranks = [u["rank"] for u in user_db.values() if u.get("rank") is not None]
            user_db[user_id]["rank"] = (max(existing_ranks) + 1) if existing_ranks else 1
        elif not is_completed:
            user_db[user_id]["rank"] = None

        save_db()

    return redirect('/admin')

@app.route('/admin/clear', methods=['POST'])
@requires_auth
def admin_clear():
    user_id = request.form.get('user_id')
    if user_id:
        if user_id == 'ALL':
            user_db.clear()
        elif user_id in user_db:
            del user_db[user_id]
        save_db()
            
    return redirect('/admin')

@app.route('/admin/all')
@requires_auth
def all_page():
    return render_template('admin_all.html')

# --- QRコード照会用API ---

@app.route('/admin/checker')
@requires_auth
def qr_reader_page():
    return render_template('checker.html')

@app.route('/api/check_user', methods=['POST'])
@requires_auth
def check_user():
    data = request.get_json()
    user_id = data.get('user_id') if data else None

    if not user_id or user_id not in user_db:
        return jsonify({
            'success': False,
            'message': '該当するユーザーが見つかりません。（削除済みまたは未登録）',
            'completed': False
        }), 404

    user_data = user_db[user_id]
    completed = user_data["completed"]
    stamp_count = len(user_data["stamps"])
    rank = user_data.get("rank")
    
    already_checked = user_data.get("checked", False)
    user_data["checked"] = True
    save_db()

    return jsonify({
        'success': True,
        'userId': user_id,
        'completed': completed,
        'stampCount': stamp_count,
        'rank': rank,
        'checked': True,
        'alreadyChecked': already_checked,
        'message': 'ユーザーデータを照会しました。'
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
