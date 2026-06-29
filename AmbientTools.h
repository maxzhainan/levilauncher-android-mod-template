#pragma once

#include <cstddef>
#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup AmbientTools Ambient Tools API
 *
 * Call @ref At_IsBound() before other APIs. On failure, `AtResult::error` is
 * usually @ref AT_ERR_NOT_LOADED.
 *
 * @{
 */

#define AT_ERR_NOT_LOADED "Ambient Tools Lib Not Loaded"

#define AT_STR_SHORT  64
#define AT_STR_MED    128
#define AT_STR_LONG   256

typedef struct AtResult {
    int  ok;
    char error[96];
} AtResult;

typedef struct AtVec2 {
    float x;
    float y;
} AtVec2;

typedef struct AtVec3 {
    float x;
    float y;
    float z;
} AtVec3;

typedef struct AtDVec2 {
    double x;
    double z;
} AtDVec2;

typedef struct AtDVec3 {
    double x;
    double y;
    double z;
} AtDVec3;

typedef struct AtPlayerInfo {
    int64_t entity_id;
    char    name[AT_STR_SHORT];
    char    uuid[AT_STR_MED];
    char    xuid[AT_STR_SHORT];
    int     build_platform;
    int     is_host;
    int     is_teacher;
} AtPlayerInfo;

typedef struct AtEntityInfo {
    int64_t rid;
    int64_t uid;
    char    type[AT_STR_MED];
    char    name[AT_STR_SHORT];
    char    uuid[AT_STR_MED];
    AtVec3  pos;
    AtVec3  vel;
    float   pitch;
    float   yaw;
    float   head_yaw;
    int     is_player;
    int     is_local;
} AtEntityInfo;

/**
 * @brief Whether Ambient Tools is linked and ready.
 * @return 1 if ready, 0 otherwise.
 */
int At_IsBound(void);

/**
 * @brief Whether the local player is in a world.
 * @return 1 in world, 0 otherwise.
 */
int At_SessionInGame(void);

/**
 * @brief Whether the UI is in a menu.
 * @return 1 in menu, 0 otherwise.
 */
int At_SessionInMenu(void);

/**
 * @brief Whether the current session is ready for gameplay queries.
 * @return 1 if ready, 0 otherwise.
 */
int At_SessionPlayerReady(void);

/**
 * @brief World seed from when the world was joined.
 */
AtResult At_GetSeedI64(int64_t *out);

/**
 * @brief Runtime entity id of the local player.
 */
AtResult At_GetLocalPlayerRuntimeEntityId(int64_t *out);

/**
 * @brief World tick at join (not live-updated).
 */
AtResult At_GetCurrentTick(int64_t *out);

/**
 * @brief World display name.
 * @param buf Caller-owned UTF-8 buffer.
 * @param cap Buffer size in bytes.
 */
AtResult At_GetWorldName(char *buf, size_t cap);

/**
 * @brief Server engine name.
 * @param buf Caller-owned UTF-8 buffer.
 * @param cap Buffer size in bytes.
 */
AtResult At_GetEngine(char *buf, size_t cap);

/**
 * @brief World spawn position from join.
 */
AtResult At_GetSpawnPosition(AtDVec3 *out);

/**
 * @brief Join-time look rotation (pitch in `x`, yaw in `z`).
 */
AtResult At_GetStartGameRotation(AtDVec2 *out);

/**
 * @brief Default gamemode name (e.g. survival).
 * @param buf Caller-owned UTF-8 buffer.
 * @param cap Buffer size in bytes.
 */
AtResult At_GetPlayerGamemode(char *buf, size_t cap);

/**
 * @brief Live local player pitch and yaw in degrees.
 */
AtResult At_GetLocalRotation(AtVec2 *out);

/**
 * @brief Live local player world position (feet / body origin).
 */
AtResult At_GetLocalPosition(AtVec3 *out);

/**
 * @brief Live local player block coordinates (floored x, y, z).
 */
AtResult At_GetLocalBlockPos(AtVec3 *out);

/**
 * @brief Number of players in the session roster.
 */
AtResult At_GdmPlayerCount(size_t *out);

/**
 * @brief Roster player by entity id.
 */
AtResult At_GdmPlayerByEntityId(int64_t entity_id, AtPlayerInfo *out);

/**
 * @brief Roster player by name (case-insensitive).
 */
AtResult At_GdmPlayerByName(const char *name, AtPlayerInfo *out);

/**
 * @brief Whether an entity with this runtime id exists.
 */
AtResult At_EntityHasRid(int64_t rid, int *out);

/**
 * @brief Entity snapshot by runtime id.
 */
AtResult At_EntityByRid(int64_t rid, AtEntityInfo *out);

/**
 * @brief Entity snapshot by unique id.
 */
AtResult At_EntityByUid(int64_t uid, AtEntityInfo *out);

/**
 * @brief Total tracked entity count.
 */
AtResult At_EntityCount(size_t *out);

/**
 * @brief Entity snapshot by index (0 .. count-1).
 */
AtResult At_EntityAt(size_t index, AtEntityInfo *out);

/**
 * @brief Number of tracked players.
 */
AtResult At_TrackedPlayerCount(size_t *out);

/**
 * @brief Tracked player by index (0 .. count-1).
 */
AtResult At_TrackedPlayerAt(size_t index, AtEntityInfo *out);

/**
 * @brief Number of tracked mobs.
 */
AtResult At_MobCount(size_t *out);

/**
 * @brief Tracked mob by index (0 .. count-1).
 */
AtResult At_MobAt(size_t index, AtEntityInfo *out);

/**
 * @brief Tracked snapshot of the local player entity (network-synced `pos`, not live engine).
 * @see At_GetLocalPosition for live position.
 */
AtResult At_LocalTrackedPlayer(AtEntityInfo *out);

/**
 * @brief Local player runtime entity id in the entity tracker.
 */
AtResult At_LocalEntityRid(int64_t *out);

/**
 * @brief Send Chat Message or execute commands
 */
AtResult At_SendChatMessage(const char *msg);


typedef struct AtChatEvent {
    char    message[AT_STR_LONG];
    char    sender_name[AT_STR_SHORT];
    char    xuid[AT_STR_SHORT];
    int     is_mine;       /* 1 if sent by local player */
    int     block;         /* set to 1 in callback to intercept */
} AtChatEvent;

typedef void (*AtChatCallback)(AtChatEvent *ev);

void At_SetChatCallback(AtChatCallback cb);
void At_ClearChatCallback(void);

/** @} */

#ifdef __cplusplus
}
#endif
