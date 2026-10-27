#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <3ds.h>
#include <citro2d.h>
#include "quirc.h"

#define WIDTH 400
#define HEIGHT 240
#define TEX_WIDTH 512
#define TEX_HEIGHT 256
#define SCREEN_SIZE (WIDTH * HEIGHT * 2)
#define WAIT_TIMEOUT 1000000000ULL

// CAMUのRGB565は通常のRGB565(R=上位5bit)。色が青っぽい/赤っぽい場合のみ1にする。
#define CAM_SWAP_RB 0

#define API_BASE_URL "https://selective-donate-apart-optional.trycloudflare.com"   // 起動時の接続先(QRで変更可)
#define API_URL_MAX 256
#define API_PATH "/api/check_user"
#define AUTH_HEADER "Basic cTF3MmUzcjQ6emF4c2NkdmY="

// 状態管理用構造体
typedef struct {
    bool frozen;
    char lastUuid[37];
    char rankVal[32];
    char userIdVal[65];
    char stampCountVal[16];
    char completedVal[16];
    char alreadyCheckedVal[16];
    char statusMsg[128];
    bool isSuccess;
    bool isNotFound;
    int httpStatus;
} AppState;

// 受信(1フレーム分)を予約する。キャプチャ自体は起動時から走らせっぱなしにする。
static Result armReceive(Handle *event, u8 *buffer, u32 transferBytes)
{
    return CAMU_SetReceiving(event, buffer, PORT_CAM1, SCREEN_SIZE, (s16)transferBytes);
}

// 3DSのテクスチャは8x8タイル内がMorton(Z-order)配置。
// 毎フレーム96000画素を変換するので、オフセットは事前にテーブル化しておく。
static u32 xOffsetTable[WIDTH];
static u32 yOffsetTable[HEIGHT];

static void initTextureTables(void)
{
    for (unsigned int x = 0; x < WIDTH; x++) {
        unsigned int lx = x & 7;
        unsigned int morton = (lx & 1) | ((lx & 2) << 1) | ((lx & 4) << 2);
        xOffsetTable[x] = (x >> 3) * 64 + morton;
    }
    for (unsigned int y = 0; y < HEIGHT; y++) {
        unsigned int ly = y & 7;
        unsigned int morton = ((ly & 1) << 1) | ((ly & 2) << 2) | ((ly & 4) << 3);
        yOffsetTable[y] = (y >> 3) * (TEX_WIDTH / 8) * 64 + morton;
    }
}

static void uploadCameraFrame(C3D_Tex *texture, u16 *texturePixels, const u16 *cameraPixels)
{
    for (unsigned int y = 0; y < HEIGHT; y++) {
        const u16 *src = cameraPixels + y * WIDTH;
        u16 *dstRow = texturePixels + yOffsetTable[y];
        for (unsigned int x = 0; x < WIDTH; x++) {
            u16 pixel = src[x];
#if CAM_SWAP_RB
            pixel = (u16)(((pixel & 0x001F) << 11) |
                          (pixel & 0x07E0) |
                          ((pixel >> 11) & 0x001F));
#endif
            dstRow[xOffsetTable[x]] = pixel;
        }
    }

    C3D_TexUpload(texture, texturePixels);
    C3D_TexFlush(texture);
}

// QRコード解析
static bool scanQr(struct quirc *qr, const u16 *img, char *payload, size_t payloadSize)
{
    int imageWidth = 0;
    int imageHeight = 0;

    u8 *gray = quirc_begin(qr, &imageWidth, &imageHeight);
    if (gray == NULL || imageWidth != WIDTH || imageHeight != HEIGHT)
        return false;

    for (int i = 0; i < WIDTH * HEIGHT; i++)
    {
        u16 pixel = img[i];
#if CAM_SWAP_RB
        unsigned int r = (pixel & 0x1F) * 255 / 31;
        unsigned int b = ((pixel >> 11) & 0x1F) * 255 / 31;
#else
        unsigned int r = ((pixel >> 11) & 0x1F) * 255 / 31;
        unsigned int b = (pixel & 0x1F) * 255 / 31;
#endif
        unsigned int g = ((pixel >> 5) & 0x3F) * 255 / 63;
        gray[i] = (u8)((77 * r + 150 * g + 29 * b) >> 8);
    }
    quirc_end(qr);

    int count = quirc_count(qr);
    if (count == 0) return false;

    for (int i = 0; i < count; i++)
    {
        struct quirc_code code;
        struct quirc_data data;
        quirc_extract(qr, i, &code);
        if (quirc_decode(&code, &data) != QUIRC_SUCCESS) continue;

        size_t len = data.payload_len;
        if (len >= payloadSize) len = payloadSize - 1;
        memcpy(payload, data.payload, len);
        payload[len] = '\0';
        return true;
    }
    return false;
}

// 128桁バイナリをUUIDに変換
static bool binaryToUuid(const char *binary, char *uuid, size_t uuidSize)
{
    if (strlen(binary) != 128 || uuidSize < 37) return false;
    char hex[33];
    const char *digits = "0123456789abcdef";

    for (int i = 0; i < 128; i++) {
        if (binary[i] != '0' && binary[i] != '1') return false;
    }

    for (int i = 0; i < 32; i++) {
        int value = 0;
        for (int j = 0; j < 4; j++)
            value = value * 2 + (binary[i * 4 + j] - '0');
        hex[i] = digits[value];
    }
    hex[32] = '\0';

    snprintf(uuid, uuidSize, "%.8s-%.4s-%.4s-%.4s-%.12s", hex, hex + 8, hex + 12, hex + 16, hex + 20);
    return true;
}

// 接続先API。起動時は既定値で、URLのQRを読み取ると変更できる(アプリ終了で既定値に戻る)
static char apiUrl[API_URL_MAX] = API_BASE_URL API_PATH;

// QRの内容がAPI URLとして使えるか検査し、前後の空白/改行を除去して整形する
static bool normalizeApiUrl(char *str)
{
    char *start = str;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != str) memmove(str, start, strlen(start) + 1);

    size_t len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) str[--len] = '\0';

    if (strncmp(str, "http://", 7) != 0 && strncmp(str, "https://", 8) != 0) return false;
    if (len <= 8 || len >= API_URL_MAX - (int)sizeof(API_PATH)) return false;
    // QRには /api/check_user より前だけを入れる。パスを含むものは受け付けない。
    if (strstr(str, API_PATH) != NULL) return false;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c <= 0x20 || c >= 0x7F) return false;
    }
    return true;
}

// QRのURL(例: https://example.com)に API_PATH を付けて完全なURLにする。
// 末尾の / は取り除いてから付ける。
static void buildApiUrl(const char *base, char *out, size_t outSize)
{
    snprintf(out, outSize, "%s", base);
    size_t len = strlen(out);
    while (len > 0 && out[len - 1] == '/') out[--len] = '\0';

    snprintf(out + len, outSize - len, "%s", API_PATH);
}

// 簡易JSONパーサ
static bool getJsonValue(const char *json, const char *key, char *out, size_t outSize)
{
    if (!json || !key || !out || outSize == 0) return false;
    out[0] = '\0';

    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;

    p = strchr(p + strlen(search), ':');
    if (!p) return false;
    p++;

    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;

    if (*p == '"') {
        p++;
        const char *end = strchr(p, '"');
        if (!end) return false;
        size_t len = (size_t)(end - p);
        if (len >= outSize) len = outSize - 1;
        memcpy(out, p, len);
        out[len] = '\0';
        return true;
    }

    const char *end = p;
    while (*end && *end != ',' && *end != '}' && *end != ' ' && *end != '\r' && *end != '\n') end++;
    size_t len = (size_t)(end - p);
    if (len == 0 || len >= outSize) return false;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

// Flask API通信
static Result checkUser(const char *uuid, u32 *statusCode, char *response, size_t responseSize)
{
    Result rc;
    httpcContext context;
    bool contextOpened = false;
    bool httpInitialized = false;
    char json[80];

    response[0] = '\0';
    *statusCode = 0;
    snprintf(json, sizeof(json), "{\"user_id\":\"%s\"}", uuid);

    rc = httpcInit(0x1000);
    if (R_FAILED(rc)) return rc;
    httpInitialized = true;

    rc = httpcOpenContext(&context, HTTPC_METHOD_POST, apiUrl, 1);
    if (R_FAILED(rc)) goto cleanup;
    contextOpened = true;

    // 3DSに内蔵されたルート証明書は古く、Cloudflare(trycloudflare.com)などの
    // 現行の証明書チェーンを検証できず HTTPS が失敗するため、https のときは検証を無効にする。
    if (strncmp(apiUrl, "https://", 8) == 0) {
        httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
    }

    httpcAddRequestHeaderField(&context, "Content-Type", "application/json");
    httpcAddRequestHeaderField(&context, "Authorization", AUTH_HEADER);
    httpcAddRequestHeaderField(&context, "Accept", "application/json");
    httpcAddPostDataRaw(&context, (const u32 *)json, (u32)strlen(json));

    rc = httpcBeginRequest(&context);
    if (R_FAILED(rc)) goto cleanup;

    rc = httpcGetResponseStatusCode(&context, statusCode);
    if (R_FAILED(rc)) goto cleanup;

    {
        u32 downloaded = 0;
        rc = httpcDownloadData(&context, (u8 *)response, (u32)(responseSize - 1), &downloaded);
        if (downloaded >= responseSize) downloaded = (u32)(responseSize - 1);
        response[downloaded] = '\0';
    }

    rc = 0;
cleanup:
    if (contextOpened) httpcCloseContext(&context);
    if (httpInitialized) httpcExit();
    return rc;
}

int main(int argc, char* argv[])
{
    gfxInitDefault();

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* topScreen = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bottomScreen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    u32 clrBg         = C2D_Color32(24, 36, 60, 255);
    u32 clrHeader     = C2D_Color32(18, 90, 170, 255);
    u32 clrWhite      = C2D_Color32(255, 255, 255, 255);
    u32 clrGreen      = C2D_Color32(46, 204, 113, 255);
    u32 clrRed        = C2D_Color32(231, 76, 60, 255);
    u32 clrPanel      = C2D_Color32(32, 48, 80, 255);
    u32 clrLine       = C2D_Color32(50, 75, 115, 255);
    u32 clrSubText    = C2D_Color32(180, 200, 220, 255);

    C2D_TextBuf staticBuf = C2D_TextBufNew(16384);

    u8 *buffer = malloc(SCREEN_SIZE);    // 受信用バッファA
    u8 *bufferB = malloc(SCREEN_SIZE);   // 受信用バッファB(処理中にも次のフレームを受け取れるよう交互に使う)
    u16 *texturePixels = calloc((size_t)TEX_WIDTH * TEX_HEIGHT, sizeof(u16));
    struct quirc *qr = quirc_new();
    if (!buffer || !bufferB || !texturePixels || !qr || quirc_resize(qr, WIDTH, HEIGHT) < 0) {
        if (buffer) free(buffer);
        if (bufferB) free(bufferB);
        if (texturePixels) free(texturePixels);
        if (qr) quirc_destroy(qr);
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return -1;
    }

    // カメラ用テクスチャ (512x256の2のべき乗で初期化)
    C3D_Tex cameraTex;
    if (!C3D_TexInit(&cameraTex, TEX_WIDTH, TEX_HEIGHT, GPU_RGB565)) {
        return -1;
    }
    C3D_TexSetFilter(&cameraTex, GPU_LINEAR, GPU_LINEAR);

    // 注意: top > bottom にすること。
    // top < bottom だとCitro2Dが「90度回転済みサブテクスチャ」と判断して
    // 画像を横倒し・サイズ違いで描画してしまう。
    // アップロード時に上下反転して格納しているので、
    // 画像の上端(top) = v 240/256、下端(bottom) = v 0 になる。
    static Tex3DS_SubTexture cameraSubTex = {
        WIDTH, HEIGHT,
        0.0f, (float)HEIGHT / TEX_HEIGHT, (float)WIDTH / TEX_WIDTH, 0.0f
    };
    C2D_Image cameraImg = {
        &cameraTex,
        &cameraSubTex
    };

    if (R_FAILED(camInit())) {
        return -1;
    }

    u32 transferBytes = 0;
    CAMU_GetMaxBytes(&transferBytes, WIDTH, HEIGHT);
    CAMU_SetTransferBytes(PORT_CAM1, transferBytes, WIDTH, HEIGHT);
    CAMU_SetSize(SELECT_OUT1, SIZE_CTR_TOP_LCD, CONTEXT_A);
    CAMU_SetOutputFormat(SELECT_OUT1, OUTPUT_RGB_565, CONTEXT_A);
    CAMU_SetFrameRate(SELECT_OUT1, FRAME_RATE_15);
    CAMU_SetNoiseFilter(SELECT_OUT1, true);
    CAMU_SetAutoExposure(SELECT_OUT1, true);
    CAMU_SetAutoWhiteBalance(SELECT_OUT1, true);
    CAMU_SetTrimming(PORT_CAM1, false);
    CAMU_Activate(SELECT_OUT1);

    initTextureTables();

    // キャプチャは一度だけ開始し、あとは受信を予約し直すだけにする(毎回Start/Stopすると非常に重い)
    Handle recvEvent = 0;
    Handle bufErrEvent = 0;
    u8 *recvTarget = buffer;   // いま受信を予約しているバッファ
    u8 *procBuf = buffer;      // 処理(表示・QR解析)に使うバッファ
    u64 lastFrameTime = osGetTime();
    // バッファエラー(受信の予約が間に合わず撮影が止まる状態)を即座に検知して復旧する
    CAMU_GetBufferErrorInterruptEvent(&bufErrEvent, PORT_CAM1);
    CAMU_StartCapture(PORT_CAM1);
    armReceive(&recvEvent, recvTarget, transferBytes);

    AppState state = {0};
    strcpy(state.rankVal, "-");
    strcpy(state.userIdVal, "Waiting for QR scan...");
    strcpy(state.stampCountVal, "-");
    strcpy(state.completedVal, "-");
    strcpy(state.alreadyCheckedVal, "-");
    strcpy(state.statusMsg, "待機中");
    state.isSuccess = false;

    // 静的テキストはループ前に1回だけ生成(毎フレームのパース/最適化は重い)
    C2D_TextBuf dynBuf = C2D_TextBufNew(2048);
    C2D_Text txtTitleTop, txtLive, txtMsgDefault, txtMsgWait;
    C2D_Text txtTitleBottom, txtFooter;
    C2D_Text dynRankLbl, dynStampLbl, dynUserLbl;
    C2D_TextParse(&txtTitleTop,     staticBuf, "Camera + Server Checker");
    C2D_TextParse(&txtLive,         staticBuf, "Live Preview");
    C2D_TextParse(&txtMsgDefault,   staticBuf, "QRコードをカメラに向けてください。(A: 再スキャン)");
    C2D_TextParse(&txtMsgWait,      staticBuf, "Aボタンを押して次のスキャンへ");
    C2D_TextParse(&txtTitleBottom,  staticBuf, "ユーザー照会");
    C2D_TextParse(&txtFooter,       staticBuf, "START : Exit  |  A : Next Scan");
    C2D_TextParse(&dynRankLbl,      staticBuf, "RANK");
    C2D_TextParse(&dynStampLbl,     staticBuf, "STAMPS");
    C2D_TextParse(&dynUserLbl,      staticBuf, "USER ID");
    C2D_TextOptimize(&txtTitleTop);
    C2D_TextOptimize(&txtLive);
    C2D_TextOptimize(&txtMsgDefault);
    C2D_TextOptimize(&txtMsgWait);
    C2D_TextOptimize(&txtTitleBottom);
    C2D_TextOptimize(&txtFooter);
    C2D_TextOptimize(&dynRankLbl);
    C2D_TextOptimize(&dynStampLbl);
    C2D_TextOptimize(&dynUserLbl);

    int frameCounter = 0;
    char payload[512];

    while (aptMainLoop())
    {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;

        // カメラフレームが届いているかをノンブロッキングで確認
        bool newFrame = false;
        bool needRestart = false;

        // バッファエラー: 受信の予約が間に合わなかった。放置すると撮影が止まるので即復旧する
        if (bufErrEvent != 0 && svcWaitSynchronization(bufErrEvent, 0) == 0) {
            needRestart = true;
        }
        // 保険: 3秒間フレームが全く来ない場合も復旧する
        if (osGetTime() - lastFrameTime > 3000) {
            needRestart = true;
        }

        if (needRestart) {
            if (recvEvent != 0) { svcCloseHandle(recvEvent); recvEvent = 0; }
            CAMU_ClearBuffer(PORT_CAM1);
            recvTarget = buffer;
            armReceive(&recvEvent, recvTarget, transferBytes);
            CAMU_StartCapture(PORT_CAM1);
            lastFrameTime = osGetTime();
        } else if (recvEvent != 0 && svcWaitSynchronization(recvEvent, 0) == 0) {
            svcCloseHandle(recvEvent);
            recvEvent = 0;
            lastFrameTime = osGetTime();

            // 届いたバッファを処理用にし、次のフレームは反対側のバッファへすぐ予約する
            procBuf = recvTarget;
            GSPGPU_InvalidateDataCache(procBuf, SCREEN_SIZE);
            recvTarget = (recvTarget == buffer) ? bufferB : buffer;
            armReceive(&recvEvent, recvTarget, transferBytes);
            newFrame = true;
        }

        if (state.frozen) {
            if (kDown & KEY_A) {
                state.frozen = false;
                state.lastUuid[0] = '\0';
                strcpy(state.rankVal, "-");
                strcpy(state.userIdVal, "Ready for next scan");
                strcpy(state.stampCountVal, "-");
                strcpy(state.completedVal, "-");
                strcpy(state.alreadyCheckedVal, "-");
                strcpy(state.statusMsg, "待機中");
                state.isSuccess = false;
                state.isNotFound = false;
            }
        } else if (newFrame) {
            // カメラ映像を3DSのタイル形式に変換してテクスチャへ転送する。
            uploadCameraFrame(&cameraTex, texturePixels, (const u16 *)procBuf);

            frameCounter++;
            if (frameCounter >= 5) {
                frameCounter = 0;
                if (scanQr(qr, (const u16 *)procBuf, payload, sizeof(payload))) {
                    char uuid[37];
                    if (normalizeApiUrl(payload)) {
                        // URLのQR: API接続先を変更する
                        buildApiUrl(payload, apiUrl, sizeof(apiUrl));

                        state.frozen = true;
                        state.lastUuid[0] = '\0';
                        state.isSuccess = true;
                        state.isNotFound = false;
                        state.httpStatus = 0;
                        strcpy(state.rankVal, "-");
                        strcpy(state.stampCountVal, "-");
                        strcpy(state.completedVal, "-");
                        strcpy(state.alreadyCheckedVal, "-");
                        snprintf(state.userIdVal, sizeof(state.userIdVal), "%s", payload);
                        strcpy(state.statusMsg, "API URLを変更しました");
                    } else if (binaryToUuid(payload, uuid, sizeof(uuid))) {
                        if (strcmp(uuid, state.lastUuid) != 0) {
                            strncpy(state.lastUuid, uuid, sizeof(state.lastUuid));
                            state.frozen = true;
                            strcpy(state.statusMsg, "照会中...");

                            u32 status = 0;
                            char response[512];
                            Result apiRc = checkUser(uuid, &status, response, sizeof(response));

                            if (R_FAILED(apiRc)) {
                                state.isSuccess = false;
                                state.isNotFound = false;
                                state.httpStatus = 0;
                                strcpy(state.rankVal, "Error");
                                strncpy(state.userIdVal, uuid, sizeof(state.userIdVal));
                                snprintf(state.statusMsg, sizeof(state.statusMsg), "通信エラー %08lX", (unsigned long)apiRc);
                            } else {
                                state.httpStatus = status;
                                char success[16] = "";
                                char rank[24] = "";
                                char userId[64] = "";
                                char completed[16] = "";
                                char stampCount[16] = "";
                                char alreadyChecked[16] = "";
                                char message[128] = "";

                                getJsonValue(response, "success", success, sizeof(success));
                                getJsonValue(response, "rank", rank, sizeof(rank));
                                getJsonValue(response, "userId", userId, sizeof(userId));
                                getJsonValue(response, "completed", completed, sizeof(completed));
                                getJsonValue(response, "stampCount", stampCount, sizeof(stampCount));
                                getJsonValue(response, "alreadyChecked", alreadyChecked, sizeof(alreadyChecked));
                                getJsonValue(response, "message", message, sizeof(message));

                                if (status == 200 && strcmp(success, "true") == 0) {
                                    state.isSuccess = true;
                                    state.isNotFound = false;
                                    
                                    snprintf(state.rankVal, sizeof(state.rankVal), "%s", (rank[0] == '\0' || strcmp(rank, "null") == 0) ? "Not ranked" : rank);
                                    snprintf(state.userIdVal, sizeof(state.userIdVal), "%s", userId[0] == '\0' ? uuid : userId);
                                    snprintf(state.stampCountVal, sizeof(state.stampCountVal), "%s", stampCount[0] == '\0' ? "0" : stampCount);
                                    snprintf(state.completedVal, sizeof(state.completedVal), "%s", completed);
                                    snprintf(state.alreadyCheckedVal, sizeof(state.alreadyCheckedVal), "%s", alreadyChecked);

                                    if (strcmp(alreadyChecked, "true") == 0) {
                                        strcpy(state.statusMsg, "照会成功 (既チェック)");
                                    } else {
                                        strcpy(state.statusMsg, "照会成功 (新規)");
                                    }

                                } else if (status == 404) {
                                    state.isSuccess = false;
                                    state.isNotFound = true;
                                    strcpy(state.rankVal, "N/A");
                                    snprintf(state.userIdVal, sizeof(state.userIdVal), "%s", uuid);
                                    strcpy(state.statusMsg, "ユーザーが未登録です");
                                } else {
                                    state.isSuccess = false;
                                    state.isNotFound = false;
                                    strcpy(state.rankVal, "Error");
                                    snprintf(state.userIdVal, sizeof(state.userIdVal), "%s", uuid);
                                    snprintf(state.statusMsg, sizeof(state.statusMsg), "HTTPエラー: %lu", (unsigned long)status);
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 描画処理 (Citro2D フレーム開始)
        // ==========================================
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(dynBuf);

        C2D_Text txtDynamicStatus, txtDynamicRank, txtDynamicStamp, txtDynamicUser;
        C2D_TextParse(&txtDynamicStatus, dynBuf, state.statusMsg);
        C2D_TextParse(&txtDynamicRank,   dynBuf, state.rankVal);
        C2D_TextParse(&txtDynamicStamp,  dynBuf, state.stampCountVal);
        C2D_TextParse(&txtDynamicUser,   dynBuf, state.userIdVal);
        C2D_TextOptimize(&txtDynamicStatus);
        C2D_TextOptimize(&txtDynamicRank);
        C2D_TextOptimize(&txtDynamicStamp);
        C2D_TextOptimize(&txtDynamicUser);

        // --- 上画面の描画 (カメラ背景テクスチャ + UI) ---
        // 色と深度バッファを毎フレームクリアする(しないと前フレームの文字が残って重なる)
        C2D_TargetClear(topScreen, clrBg);
        C2D_SceneBegin(topScreen);
        C2D_DrawImageAt(cameraImg, 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);

        // ヘッダーバー (高さ 26px)
        C2D_DrawRectSolid(0, 0, 0, 400, 26, clrHeader);
        C2D_DrawRectSolid(10, 7, 0, 14, 10, clrWhite);
        C2D_DrawCircleSolid(17, 12, 0, 3, clrHeader);
        C2D_DrawRectSolid(13, 5, 0, 6, 2, clrWhite);
        C2D_DrawText(&txtTitleTop, C2D_AlignLeft | C2D_WithColor, 30, 7, 0.5f, 0.45f, 0.45f, clrWhite);

        // 右側アイコン
        C2D_DrawRectSolid(348, 16, 0, 3, 5, clrWhite);
        C2D_DrawRectSolid(353, 13, 0, 3, 8, clrWhite);
        C2D_DrawRectSolid(358, 10, 0, 3, 11, clrWhite);
        C2D_DrawRectSolid(368, 9, 0, 22, 12, clrLine);
        C2D_DrawRectSolid(370, 11, 0, 18, 8, clrHeader);
        C2D_DrawRectSolid(372, 13, 0, 12, 4, clrWhite);
        C2D_DrawRectSolid(390, 12, 0, 2, 6, clrWhite);

        // Live Preview インジケーター
        {
            // ●(丸) + 文字 を、パネルの中で上下左右とも均等な余白になるように配置する
            const float panelX = 8.0f, panelY = 34.0f, panelH = 20.0f;
            const float padX = 10.0f, dotD = 8.0f, gap = 6.0f, liveScale = 0.38f;
            float textW, textH;
            C2D_TextGetDimensions(&txtLive, liveScale, liveScale, &textW, &textH);

            float panelW = padX + dotD + gap + textW + padX;
            C2D_DrawRectSolid(panelX, panelY, 0, panelW, panelH, clrPanel);
            C2D_DrawCircleSolid(panelX + padX + dotD / 2.0f, panelY + panelH / 2.0f, 0,
                                dotD / 2.0f, state.frozen ? clrRed : clrGreen);
            C2D_DrawText(&txtLive, C2D_AlignLeft | C2D_WithColor,
                         panelX + padX + dotD + gap, panelY + (panelH - textH) / 2.0f,
                         0.5f, liveScale, liveScale, clrWhite);
        }

        // QRコードのファインダー枠
        float fx = 125, fy = 43, fw = 150, fh = 150, l = 22, t = 3;   // 枠(80→150px)。カメラ領域 y:26-210 の中央
        u32 frameColor = state.frozen ? clrRed : clrGreen;
        C2D_DrawRectSolid(fx, fy, 0, l, t, frameColor);
        C2D_DrawRectSolid(fx, fy, 0, t, l, frameColor);
        C2D_DrawRectSolid(fx + fw - l, fy, 0, l, t, frameColor);
        C2D_DrawRectSolid(fx + fw - t, fy, 0, t, l, frameColor);
        C2D_DrawRectSolid(fx, fy + fh - t, 0, l, t, frameColor);
        C2D_DrawRectSolid(fx, fy + fh - l, 0, t, l, frameColor);
        C2D_DrawRectSolid(fx + fw - l, fy + fh - t, 0, l, t, frameColor);
        C2D_DrawRectSolid(fx + fw - t, fy + fh - l, 0, t, l, frameColor);

        // 下部メッセージバー
        C2D_DrawRectSolid(0, 210, 0, 400, 30, clrPanel);
        {
            // バー(x:0-400, y:210-240)の中央に配置する
            C2D_Text *msg = state.frozen ? &txtMsgWait : &txtMsgDefault;
            float msgScale = state.frozen ? 0.42f : 0.40f;
            float msgW, msgH;
            C2D_TextGetDimensions(msg, msgScale, msgScale, &msgW, &msgH);
            C2D_DrawText(msg, C2D_WithColor,
                         (400.0f - msgW) / 2.0f, 210.0f + (30.0f - msgH) / 2.0f,
                         0.5f, msgScale, msgScale, clrWhite);
        }

        // --- 下画面の描画 ---
        C2D_TargetClear(bottomScreen, clrBg);
        C2D_SceneBegin(bottomScreen);

        C2D_DrawRectSolid(0, 0, 0, 320, 26, clrHeader);
        C2D_DrawText(&txtTitleBottom, C2D_AlignLeft | C2D_WithColor, 10, 4, 0.5f, 0.45f, 0.45f, clrWhite);

        C2D_DrawRectSolid(0, 26, 0, 320, 32, clrPanel);
        C2D_DrawLine(0, 58, clrLine, 320, 58, clrLine, 1.0f, 0.0f);
        C2D_DrawCircleSolid(22, 42, 0, 6, state.isSuccess ? clrGreen : (state.frozen ? clrRed : clrSubText));

        C2D_DrawText(&txtDynamicStatus, C2D_AlignLeft | C2D_WithColor, 35, 36, 0.5f, 0.42f, 0.42f, clrWhite);

        C2D_DrawText(&dynRankLbl, C2D_AlignLeft | C2D_WithColor, 15, 66, 0.5f, 0.35f, 0.35f, clrSubText);
        C2D_DrawText(&txtDynamicRank, C2D_AlignLeft | C2D_WithColor, 15, 84, 0.5f, 0.50f, 0.50f, clrWhite);

        C2D_DrawText(&dynStampLbl, C2D_AlignLeft | C2D_WithColor, 170, 66, 0.5f, 0.35f, 0.35f, clrSubText);
        C2D_DrawText(&txtDynamicStamp, C2D_AlignLeft | C2D_WithColor, 170, 84, 0.5f, 0.50f, 0.50f, clrWhite);

        C2D_DrawLine(0, 115, clrLine, 320, 115, clrLine, 1.0f, 0.0f);

        C2D_DrawText(&dynUserLbl, C2D_AlignLeft | C2D_WithColor, 15, 123, 0.5f, 0.35f, 0.35f, clrSubText);
        C2D_DrawText(&txtDynamicUser, C2D_AlignLeft | C2D_WithColor, 15, 142, 0.5f, 0.38f, 0.38f, clrWhite);
        C2D_DrawLine(0, 175, clrLine, 320, 175, clrLine, 1.0f, 0.0f);

        C2D_DrawText(&txtFooter, C2D_AlignLeft | C2D_WithColor, 15, 195, 0.5f, 0.38f, 0.38f, clrSubText);

        C3D_FrameEnd(0);
    }

    CAMU_StopCapture(PORT_CAM1);
    if (recvEvent != 0) svcCloseHandle(recvEvent);
    if (bufErrEvent != 0) svcCloseHandle(bufErrEvent);
    CAMU_Activate(SELECT_NONE);
    camExit();
    C3D_TexDelete(&cameraTex);
    if (qr) quirc_destroy(qr);
    if (buffer) free(buffer);
    if (bufferB) free(bufferB);
    if (texturePixels) free(texturePixels);
    C2D_TextBufDelete(dynBuf);
    C2D_TextBufDelete(staticBuf);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
