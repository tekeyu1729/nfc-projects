// ==============================================================================
// 1. データ定義 (Data Definitions)
// ==============================================================================

// 【スタンプラリー用データ】 Map上の座標 (mapX, mapY %)
const STAMP_DATA = [
  { id: "647899", name: "美術室前",         x: 35, y: 75, mapX: 80, mapY: 83, img: "/static/stump/stump_1.webp", galleryImg: "static/images/img-01.webp" },
  { id: "307616", name: "情報サイエンス棟 ", x: 65, y: 75, mapX: 55, mapY: 70, img: "/static/stump/stump_2.webp", galleryImg: "static/images/img-02.webp" },
  { id: "832904", name: "体育館前",         x: 20, y: 92, mapX: 92, mapY: 87, img: "/static/stump/stump_3.webp", galleryImg: "static/images/img-03.webp" },
  { id: "846782", name: "工業化実習棟 1F",   x: 50, y: 92, mapX: 18, mapY: 88, img: "/static/stump/stump_4.webp", galleryImg: "static/images/img-04.webp" },
  { id: "675845", name: "工業化実習棟 3F",   x: 80, y: 92, mapX: 7, mapY: 42, img: "/static/stump/stump_5.webp", galleryImg: "static/images/img-05.webp" }
];

// 景品受取所（情報科棟 P室）の座標定義
const PRIZE_LOCATION = {
  name: "景品受取所",
  mapX: 53,
  mapY: 72
};

let ALL_GROUPS = [];

const FLOOR_CONFIG = {
  "m-1F": { title: "普通教室棟_1階", mapImg: "/static/images/schoolMap_m-1.webp" },
  "m-2F": { title: "普通教室棟_2階", mapImg: "/static/images/schoolMap_m-2.webp" },
  "m-3F": { title: "普通教室棟_3階", mapImg: "/static/images/schoolMap_m-3.webp" },
  "m-4F": { title: "普通教室棟_4階", mapImg: "/static/images/schoolMap_m-4.webp" },
  "s-1F": { title: "工業科実習棟_1階", mapImg: "/static/images/schoolMap_s-1.webp" },
  "s-3F": { title: "工業科実習棟_3階", mapImg: "/static/images/schoolMap_s-3.webp" },
  "r-1F": { title: "ロボティクス棟_1階", mapImg: "/static/images/schoolMap_r-1.webp" },
  "h-2F": { title: "体育館", mapImg: "/static/images/schoolMap_h-2.webp" },
  "j-1F": { title: "情報サイエンス棟", mapImg: "/static/images/schoolMap_j-1.webp" },
};


// ==============================================================================
// 2. 状態管理変数 & DOM要素取得
// ==============================================================================

let acquiredStamps = [];                                              // 獲得済みスタンプID保持用
let userId = '';
let userRank = null;
let hasSeenTutorial = false;                                          // ユーザーのチュートリアル閲覧状態
let currentSelectedStamp = null;                                      // 現在選択中のスタンプ情報
let currentActiveId = null;                                           // フロアモーダルで現在アクティブな団体ID
let nextGoList = JSON.parse(localStorage.getItem('nextGoList') || '[]'); // 「次行く！」登録リスト

const mapArea = document.getElementById('mapArea');
const goalBtn = document.getElementById('goalBtn');
const stampModal = document.getElementById('stampModal');
const goalModal = document.getElementById('goalModal');


// ==============================================================================
// 3. 初期化 & API通信処理
// ==============================================================================

async function loadStampData() {
  // 1. まずサーバーからスタンプデータを最優先で取得する
  try {
    const stampRes = await fetch('/api/stamps');
    const data = await stampRes.json();

    if (data.userId) userId = data.userId;
    if (Array.isArray(data.stamps)) acquiredStamps = data.stamps;
    if (data.rank) userRank = data.rank;
    hasSeenTutorial = data.hasSeenTutorial || false;
  } catch (err) {
    console.error('スタンプデータの取得に失敗しました:', err);
  }

  // 2. 次にグループデータを非同期で取得する（失敗してもスタンプ描画は止めない）
  try {
    const groupRes = await fetch('/static/group_data/group_data.json');
    ALL_GROUPS = await groupRes.json();
  } catch (err) {
    console.error('グループデータの取得に失敗しました:', err);
    ALL_GROUPS = [];
  }

  // 3. 画面の更新とチュートリアル判定を確実に実行
  checkUrlParameter();
  refreshAllUI();
  checkTutorial();
}

// 画面UI全体の一括更新処理
function refreshAllUI() {
  renderMap();
  updateHomeIllustration();
  renderGallery();
  renderStaticMapPins();
  renderNextGoMapPins();
  updateGoalStatus();
}

function checkUrlParameter() {
  const urlParams = new URLSearchParams(window.location.search);
  const stampIdParam = urlParams.get('id');

  if (stampIdParam && STAMP_DATA.some(s => s.id === stampIdParam)) {
    if (!acquiredStamps.includes(stampIdParam)) {
      acquiredStamps.push(stampIdParam);
    }
    window.history.replaceState({}, document.title, window.location.pathname);
    openStampModal(stampIdParam);
  }
}

// ユーザーデータの閲覧状態（hasSeenTutorial）で初回表示判定
function checkTutorial() {
  if (!hasSeenTutorial) {
    const tutorialModal = document.getElementById('tutorialModal');
    if (tutorialModal) {
      tutorialModal.classList.add('show');
    }
  }
}

// ==============================================================================
// 4. 各機能のUIレンダリング処理
// ==============================================================================

//スタンプ獲得数に応じて画像を順番に重ねていく処理
function updateHomeIllustration() {
  const count = acquiredStamps.length; // 獲得したスタンプの数 (0〜5)
  const stepImages = document.querySelectorAll('.step-img');

  stepImages.forEach((img, index) => {
    // 獲得数以下のインデックス（例: 2個獲得なら index 0, 1, 2）に .show を付与して重ねる
    if (index <= count) {
      img.classList.add('show');
    } else {
      img.classList.remove('show');
    }
  });
}

function renderMap() {
  mapArea.innerHTML = '';
  STAMP_DATA.forEach(stamp => {
    const node = document.createElement('div');
    node.className = 'stamp-node';
    node.style.left = stamp.x + '%';
    node.style.top = stamp.y + '%';

    if (acquiredStamps.includes(stamp.id)) {
      node.classList.add('acquired');
    }

    node.innerHTML = `<img src="${stamp.img}" alt="スタンプ">`;
    node.addEventListener('click', () => openStampModal(stamp.id));

    mapArea.appendChild(node);
  });

  if (acquiredStamps.length === STAMP_DATA.length) {
    goalBtn.classList.add('active');
  } else {
    goalBtn.classList.remove('active');
  }
}

function renderStaticMapPins() {
  const container = document.querySelector('.map-container-box');
  if (!container) return;

  const existingPins = container.querySelectorAll('.static-stamp-pin');
  existingPins.forEach(pin => pin.remove());

  STAMP_DATA.forEach(stamp => {
    if (stamp.mapX !== undefined && stamp.mapY !== undefined) {
      const isAcquired = acquiredStamps.includes(stamp.id);
      const pin = document.createElement('div');
      
      pin.className = `static-stamp-pin ${isAcquired ? 'acquired' : ''}`;
      pin.style.left = stamp.mapX + '%';
      pin.style.top = stamp.mapY + '%';

      pin.innerHTML = `
        <div class="static-stamp-label">${isAcquired ? '獲得済' : 'スタンプ'}</div>
        <div class="static-stamp-dot"></div>
      `;

      container.appendChild(pin);
    }
  });

  if (acquiredStamps.length === STAMP_DATA.length) {
    const prizePin = document.createElement('div');
    prizePin.className = 'static-stamp-pin prize-pin';
    prizePin.id = 'prizeLocationPin';
    prizePin.style.left = PRIZE_LOCATION.mapX + '%';
    prizePin.style.top = PRIZE_LOCATION.mapY + '%';

    prizePin.innerHTML = `
      <div class="static-stamp-label">🎁 景品受取所</div>
      <div class="static-stamp-dot"></div>
    `;

    container.appendChild(prizePin);
  }
}

function renderGallery() {
  const UNACQUIRED_IMG = "static/images/img-00.webp"; // 未獲得時の画像
  const acquiredCount = acquiredStamps.length;       // 獲得したスタンプの数 (0〜5個)

  // ★ 最初から1枚目を表示し、獲得数に応じて増やすため、表示枚数は「獲得数 + 1」になります
  // (例: 0個獲得なら 0+1=1枚目まで表示。5個すべて獲得なら 5+1=6枚目まで全表示)
  const showCount = acquiredCount + 1;

  const sliderItems = document.querySelectorAll('.slider li');
  
  sliderItems.forEach((li, index) => {
    const img = li.querySelector('img');
    if (!img) return;

    // インデックスが「表示枚数 (showCount)」未満であればカラー画像を表示
    if (index < showCount) {
      const gallerySrc = STAMP_DATA[index] ? STAMP_DATA[index].galleryImg : UNACQUIRED_IMG;
      img.src = gallerySrc;
    } else {
      img.src = UNACQUIRED_IMG;
    }
  });
}

function renderNextGoMapPins() {
  const container = document.querySelector('.map-container-box');
  if (!container) return;

  const existingPins = container.querySelectorAll('.next-go-map-pin');
  existingPins.forEach(pin => pin.remove());

  ALL_GROUPS.forEach(group => {
    if (nextGoList.includes(String(group.id)) && group.mainMap) {
      const pin = document.createElement('div');
      pin.className = 'next-go-map-pin';
      pin.style.left = parseFloat(group.mainMap.x) + '%';
      pin.style.top = parseFloat(group.mainMap.y) + '%';

      pin.innerHTML = `
        <div class="next-go-pin-label">${group.groupName}</div>
        <div class="next-go-pin-marker">★</div>
      `;

      pin.addEventListener('click', (e) => {
        e.stopPropagation();
        openFloorModal(group.floor);
        setTimeout(() => highlightGroup(group.id, true), 200);
      });

      container.appendChild(pin);
    }
  });
}

function openStampModal(stampId) {
  const stamp = STAMP_DATA.find(s => s.id === stampId);
  if (!stamp) return;

  currentSelectedStamp = stamp;

  const isAcquired = acquiredStamps.includes(stampId);
  const imgContainer = document.getElementById('modalImgContainer');
  const statusText = document.getElementById('modalStatus');

  imgContainer.innerHTML = `<img src="${stamp.img}" alt="スタンプ">`;
  document.getElementById('modalLocation').innerText = `場所：${stamp.name}`;

  if (isAcquired) {
    imgContainer.classList.remove('unacquired');
    statusText.innerText = "獲得済み！";
    statusText.style.color = "#ff4b4b";
  } else {
    imgContainer.classList.add('unacquired');
    statusText.innerText = "未獲得（現地でタッチしてね）";
    statusText.style.color = "#888";
  }

  stampModal.classList.add('show');
}

function updateGoalStatus() {
  const remainingCount = STAMP_DATA.length - acquiredStamps.length;
  const lockIcon = document.getElementById('lock-icon');
  const goalBtn = document.getElementById('goalBtn');

  if (!lockIcon || !goalBtn) return;

  if (remainingCount === 0) {
    lockIcon.src = "static/images/unlock.webp";
    lockIcon.classList.add('is-unlocked');
    goalBtn.disabled = false;
  } else {
    lockIcon.src = "static/images/lock.webp";
    lockIcon.classList.remove('is-unlocked');
    goalBtn.disabled = true;
  }
}


// ==============================================================================
// 5. フロア詳細モーダル & イベント処理
// ==============================================================================

function openFloorModal(floorKey) {
  const config = FLOOR_CONFIG[floorKey];
  if (!config) return;

  document.getElementById('floorTitle').innerText = config.title;
  document.getElementById('floorMapImg').src = config.mapImg;
  
  const floorGroups = ALL_GROUPS.filter(g => g.floor === floorKey);
  
  renderFloorContent(floorGroups);
  updateBookmarkCountUI();
  
  document.getElementById('floorDetailModal').classList.add('show');
}

function renderFloorContent(groups) {
  const hotspotArea = document.getElementById('floorHotspotArea');
  const listContainer = document.getElementById('groupListContainer');
  
  hotspotArea.innerHTML = '';
  listContainer.innerHTML = '';

  groups.forEach(group => {
    const isRegistered = nextGoList.includes(String(group.id));

    const spot = document.createElement('div');
    spot.className = `room-hotspot ${isRegistered ? 'is-next-go' : ''}`;
    spot.id = `spot-${group.id}`;
    spot.style.left = parseFloat(group.modalArea.left) + '%';
    spot.style.top = parseFloat(group.modalArea.top) + '%';
    spot.style.width = parseFloat(group.modalArea.width) + '%';
    spot.style.height = parseFloat(group.modalArea.height) + '%';
    
    spot.addEventListener('click', () => highlightGroup(group.id, true));
    hotspotArea.appendChild(spot);

    const card = document.createElement('div');
    card.className = 'group-card';
    card.id = `card-${group.id}`;

    card.innerHTML = `
      <div class="group-card-title">${group.groupName} / ${group.title}</div>
      <div class="group-card-desc">${group.desc}</div>
      <button class="next-go-btn ${isRegistered ? 'registered' : ''}" data-id="${group.id}">
        ${isRegistered ? '★ 登録中' : '次行く！'}
      </button>
    `;

    card.addEventListener('click', (e) => {
      if (e.target.classList.contains('next-go-btn')) return;
      highlightGroup(group.id, false);
    });

    const btn = card.querySelector('.next-go-btn');
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      toggleNextGo(group.id);
    });

    listContainer.appendChild(card);
  });
}

function highlightGroup(id, scrollToList = false) {
  currentActiveId = id;

  document.querySelectorAll('.room-hotspot').forEach(el => el.classList.remove('active'));
  document.querySelectorAll('.group-card').forEach(el => el.classList.remove('active'));
  const existingMarker = document.getElementById('activeRoomMarker');
  if (existingMarker) existingMarker.remove();

  const group = ALL_GROUPS.find(g => String(g.id) === String(id));
  if (!group || !group.modalArea) return;

  const activeSpot = document.getElementById(`spot-${id}`);
  const activeCard = document.getElementById(`card-${id}`);

  if (activeSpot) activeSpot.classList.add('active');
  if (activeCard) {
    activeCard.classList.add('active');
    if (scrollToList) {
      activeCard.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }
  }

  const left = parseFloat(group.modalArea.left) || 0;
  const top = parseFloat(group.modalArea.top) || 0;
  const width = parseFloat(group.modalArea.width) || 0;
  const height = parseFloat(group.modalArea.height) || 0;

  const centerX = left + (width / 2);
  const centerY = top + (height / 2);

  const hotspotArea = document.getElementById('floorHotspotArea');
  if (!hotspotArea) return;

  const marker = document.createElement('div');
  marker.id = 'activeRoomMarker';
  marker.className = 'modal-room-marker';
  marker.style.left = centerX + '%';
  marker.style.top = centerY + '%';
  marker.innerHTML = `<div class="marker-icon"></div>`;
  
  hotspotArea.appendChild(marker);
}

function toggleNextGo(id) {
  const strId = String(id);
  const index = nextGoList.indexOf(strId);

  if (index > -1) {
    nextGoList.splice(index, 1);
  } else {
    nextGoList.push(strId);
  }

  localStorage.setItem('nextGoList', JSON.stringify(nextGoList));
  updateBookmarkCountUI();

  const isReg = nextGoList.includes(strId);
  const btn = document.querySelector(`#card-${id} .next-go-btn`);
  if (btn) {
    btn.classList.toggle('registered', isReg);
    btn.innerText = isReg ? '★ 登録中' : '次行く！';
  }

  const spot = document.getElementById(`spot-${id}`);
  if (spot) {
    spot.classList.toggle('is-next-go', isReg);
  }

  renderNextGoMapPins();
}

function updateBookmarkCountUI() {
  const countEl = document.getElementById('bookmarkCount');
  if (countEl) countEl.innerText = nextGoList.length;
}


// ==============================================================================
// 6. 画面切り替え & イベントリスナー設定
// ==============================================================================

function switchView(targetViewId, activeNavId) {
  document.querySelectorAll('.view-section').forEach(el => el.classList.remove('active'));
  document.querySelectorAll('.menu-group a').forEach(el => el.classList.remove('active'));

  const targetView = document.getElementById(targetViewId);
  const activeNav = document.getElementById(activeNavId);

  if (targetView) targetView.classList.add('active');
  if (activeNav) activeNav.classList.add('active');

  if (targetViewId === 'viewGallery') {
    $('.slider').slick('setPosition');
  }
}

// --- ボタン・タップイベント ---

goalBtn.addEventListener('click', () => {
  if (acquiredStamps.length === STAMP_DATA.length) {
    const uuidStr = userId || 'GOAL_COMPLETE';
    const hexStr = "0x" + uuidStr.replace(/-/g, "");
    const bigIntValue = BigInt(hexStr);
    const binaryStr = bigIntValue.toString(2).padStart(128, '0');

    document.getElementById('qrImage').src = `https://api.qrserver.com/v1/create-qr-code/?size=130x130&data=${encodeURIComponent(binaryStr)}`;
    
    const rankDisplay = document.getElementById('goalRankDisplay');
    if (rankDisplay) {
      rankDisplay.innerText = userRank ? `おめでとう！${userRank} 人目のゴールだよ！` : '';
    }
    goalModal.classList.add('show');
    triggerConfetti();
  }
});

// スタンプ詳細モーダルからマップへ移動
document.getElementById('goToMapBtn').addEventListener('click', () => {
  if (!currentSelectedStamp) return;

  stampModal.classList.remove('show');
  switchView('viewMap', 'navMap');

  const marker = document.getElementById('mapTargetMarker');
  if (marker && currentSelectedStamp.mapX !== undefined) {
    marker.style.left = currentSelectedStamp.mapX + '%';
    marker.style.top = currentSelectedStamp.mapY + '%';
    marker.classList.remove('hidden');

    setTimeout(() => {
      marker.classList.add('hidden');
    }, 5000);
  }
});

// ゴールモーダルから景品受取所（マップ）へ移動するボタン処理
document.getElementById('goalToMapBtn').addEventListener('click', () => {
  goalModal.classList.remove('show');
  switchView('viewMap', 'navMap');

  const marker = document.getElementById('mapTargetMarker');
  if (marker) {
    marker.style.left = PRIZE_LOCATION.mapX + '%';
    marker.style.top = PRIZE_LOCATION.mapY + '%';
    marker.classList.remove('hidden');

    setTimeout(() => {
      marker.classList.add('hidden');
    }, 5000);
  }
});

// チュートリアルモーダルを閉じ、サーバーへ閲覧完了通知をPOST送信する
const closeTutorialBtn = document.getElementById('closeTutorialBtn');
if (closeTutorialBtn) {
  closeTutorialBtn.addEventListener('click', async () => {
    const tutorialModal = document.getElementById('tutorialModal');
    if (tutorialModal) {
      tutorialModal.classList.remove('show');
    }

    hasSeenTutorial = true; // クライアント側の状態を更新

    // サーバーのDB側へ閲覧完了（hasSeenTutorial = true）を送信
    try {
      await fetch('/api/tutorial/complete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ userId: userId })
      });
    } catch (err) {
      console.error('チュートリアル完了状態の同期に失敗しました:', err);
    }
  });
}

document.getElementById('closeStampModal').addEventListener('click', () => stampModal.classList.remove('show'));
document.getElementById('closeGoalModal').addEventListener('click', () => goalModal.classList.remove('show'));
document.getElementById('closeFloorModal').addEventListener('click', () => document.getElementById('floorDetailModal').classList.remove('show'));

document.querySelectorAll('.main-map-hotspot').forEach(spot => {
  spot.addEventListener('click', (e) => {
    e.stopPropagation();
    const floorKey = spot.getAttribute('data-floor');
    if (floorKey && FLOOR_CONFIG[floorKey]) {
      openFloorModal(floorKey);
    }
  });
});

document.getElementById('logoBtn').addEventListener('click', () => window.location.reload());
document.getElementById('navHome').addEventListener('click', (e) => { e.preventDefault(); switchView('viewHome', 'navHome'); });
document.getElementById('navGallery').addEventListener('click', (e) => { e.preventDefault(); switchView('viewGallery', 'navGallery'); });
document.getElementById('navMap').addEventListener('click', (e) => { e.preventDefault(); switchView('viewMap', 'navMap'); });

$(document).ready(function() {$('.slider').slick({
    autoplay: false,
    infinite: false,
    speed: 500,
    slidesToShow: 3,
    slidesToScroll: 1,
    prevArrow: '<div class="slick-prev"></div>',
    nextArrow: '<div class="slick-next"></div>',
    centerMode: true,
    variableWidth: true,
    dots: true,
  });
});

function triggerConfetti() {
  if (typeof confetti === 'function') {
    const count = 200;
    const defaults = { origin: { y: 0.7 }, zIndex: 9999 };

    function fire(particleRatio, opts) {
      confetti({ ...defaults, ...opts, particleCount: Math.floor(count * particleRatio) });
    }

    fire(0.25, { spread: 26, startVelocity: 55 });
    fire(0.2,  { spread: 60 });
    fire(0.35, { spread: 100, decay: 0.91, scalar: 0.8 });
    fire(0.1,  { spread: 120, startVelocity: 25, decay: 0.92, scalar: 1.2 });
    fire(0.1,  { spread: 120, startVelocity: 45 });
  }
}

// 初期ロード実行
loadStampData();