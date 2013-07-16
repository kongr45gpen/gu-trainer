// pyrJumpHelper.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include "plugin_utils.h"

#include <cmath>
#include <string>
using namespace std;

#define assert(x) { if (!x) bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Failed assertion: %s", #x); }

#define EPS (1e-6)

struct Rect {
  float x; // center x
  float y; // center y
  float hw; // halfwidth
  float hh; // halfheight
  float rot; // angle of the rectangle, with 0 radians having width along x axis
};

// Intervals [a, b], [c, d]
bool overlap(float a, float b, float c, float d) {
  return (c < a && a < d) or (c < b && b < d) or (a < c && c < b) or (a < d && d < b);
}

bool projectionsOverlap(float x[4], float hw) {
  float xmin = x[0], xmax = x[0];
  for (int i = 1; i < 4; i++) {
    if (x[i] < xmin)
      xmin = x[i];
    else if (x[i] > xmax)
      xmax = x[i];
  }
  return overlap(xmin, xmax, -hw, hw);
}

// Apply Separating Axis Theorem in A's local space
bool intersectsHelper(Rect a, Rect b) {
// B center relative to A
  float b_x = b.x - a.x;
  float b_y = b.y - a.y;
  // B center relative to unrotated A
  float b__x = b_x * cosf(-a.rot) - b_y * sinf(-a.rot);
  float b__y = b_y * sinf(-a.rot) + b_y * cosf(-a.rot);
  // B corners relative to unrotated A
  float b_rot = b.rot - a.rot;
  float bx[4] = {
    b__x + b.hw * cosf(b_rot) - b.hh * sinf(b_rot),
    b__x + b.hw * cosf(b_rot) + b.hh * sinf(b_rot),
    b__x - b.hw * cosf(b_rot) - b.hh * sinf(b_rot),
    b__x - b.hw * cosf(b_rot) + b.hh * sinf(b_rot)
  };
  if (!projectionsOverlap(bx, a.hw)) return false;

  float by[4] = {
    b__y + b.hw * sinf(b_rot) + b.hh * cosf(b_rot),
    b__y + b.hw * sinf(b_rot) - b.hh * cosf(b_rot),
    b__y - b.hw * sinf(b_rot) + b.hh * cosf(b_rot),
    b__y - b.hw * sinf(b_rot) - b.hh * cosf(b_rot)
  };
  if (!projectionsOverlap(by, a.hh)) return false;
  return true;
}

// FIXME: Untested
bool intersects(Rect a, Rect b) {
  return intersectsHelper(a, b) && intersectsHelper(b, a);
}

#define MaxNumPlayers 256
// Slightly higher than the time for jumping and hitting the roof and falling quickly
#define ADVICE_PERIOD 2.5f

// Above and below this they probably know very well that they won't make the jump so don't pester them
#define PYR_HEIGHT_LOWEST 5
#define PYR_HEIGHT_HIGHEST 22
#define BASE_HEIGHT 30

#define GRAVITY 9.8
#define JUMP_VEL 19.0
#define MIN_JUMP_HEIGHT (JUMP_VEL * JUMP_VEL / 2.0 / GRAVITY)
// Empirical: at 50 FPS
#define TYP_JUMP_HEIGHT 18.608
// Empirical: at 30 FPS - any lower is cheeky
#define MAX_JUMP_HEIGHT 18.734
// TODO: Add constants for tank shooting and somehow quantify the quality of the jump shooting in terms of safety and range?
// Actually maybe best to leave this to the player completely...

#define MIN_PYR_HEIGHT (BASE_HEIGHT - MAX_JUMP_HEIGHT)
#define TYP_PYR_HEIGHT (BASE_HEIGHT - TYP_JUMP_HEIGHT)
#define MAX_PYR_HEIGHT (BASE_HEIGHT - MIN_JUMP_HEIGHT)

class pyrJumpHelper : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
  virtual const char* Name (){return "PyraJump Helper";}
  virtual void Init ( const char* config);

  void Cleanup();
  void Event ( bz_EventData *data);
  
  bool SlashCommand (int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);
  
private:
  float lastPollTime; // don't poll too often
  float lastHintTime[MaxNumPlayers];
  
  void pollPlayerHint();
};

BZ_PLUGIN(pyrJumpHelper)

void pyrJumpHelper::Init ( const char* /*commandLine*/ )
{
  // init events here with Register();
  MaxWaitTime = 0.1f;
  Register(bz_eTickEvent);
  Register(bz_ePlayerJoinEvent);
  Register(bz_ePlayerPartEvent);
  Register(bz_ePlayerUpdateEvent);
  Register(bz_ePlayerSpawnEvent);
  Register(bz_eGetPlayerSpawnPosEvent);
  bz_registerCustomSlashCommand("test", this);
  bz_registerCustomSlashCommand("height", this);
  
  for (int i = 0; i < MaxNumPlayers; i++)
    lastHintTime[i] = 0.0f;
}

void pyrJumpHelper::Cleanup() {
  bz_removeCustomSlashCommand("test");
  bz_removeCustomSlashCommand("height");
  Flush();
}

bool isPlayerGrounded(int id) {
  bz_BasePlayerRecord *b = bz_getPlayerByIndex(id);
  assert(b);
  bool ret = !b->lastKnownState.falling;
  bz_freePlayerRecord(b);
  return ret;
}

// They need a hint if they are anywhere reasonably placed on the pyramid.
// This includes needing to tell them that their jump will inevitably fail because they're too far from the base, etc.
bool isPlayerGroundedOnPyrNeedingHint(bz_BasePlayerRecord *b) {
  bz_PlayerUpdateState &s = b->lastKnownState;
  if (!b->spawned || bz_isPlayerPaused(b->playerID) || s.falling) return false;
  if (s.pos[2] < PYR_HEIGHT_LOWEST || s.pos[2] > PYR_HEIGHT_HIGHEST) return false;
  // Luckily there is no other way a tank can be grounded and not be on a pyramid.
  return true;
}

// Consider the necessary conditions sequentially and independently.
// 1. Suitable height.
// 2. Suitable position.
// 3. Suitable turn speed.
// E.g. if #1 not satisfied then ONLY tell them this even if their position is also bad.
void GiveHint(bz_BasePlayerRecord *b) {
  bz_PlayerUpdateState &s = b->lastKnownState;
  string fpsHint;
  if (s.pos[2] < MIN_PYR_HEIGHT) {
    bz_sendTextMessage(BZ_SERVER, b->playerID, "Too low to make the climb");
  }
  else if (s.pos[2] < TYP_PYR_HEIGHT) {
    fpsHint = "Need 30-50 FPS.";
  }
  else if (s.pos[2] < MAX_PYR_HEIGHT) {
    fpsHint = "Need ~50 FPS.";
  }
  
}

// Poll each player to determine whether they need advice
void pyrJumpHelper::pollPlayerHint() {
  float t = bz_getCurrentTime();
  bz_APIIntList *l = bz_getPlayerIndexList();
  for (int i = 0; i < l->size(); i++) {
    int id = l->get(i);
    if (t - lastHintTime[id] < ADVICE_PERIOD) // Don't spam them with advice
      continue;
    
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(id);
    assert(b);
    if (isPlayerGroundedOnPyrNeedingHint()) {
      GiveHint(b);
    }
    bz_freePlayerRecord(b);
  }
  delete l;
}

void pyrJumpHelper::Event(bz_EventData *data) {
  if (data->eventType == bz_ePlayerUpdateEvent) {
    // If they touch ground, can give them a new hint earlier than the normal period
    auto d = static_cast<bz_PlayerUpdateEventData_V1 *>(data);
    if (d->state.pos[2] < EPS)
      lastHintTime[d->playerID] -= ADVICE_PERIOD;
  }

  float t = bz_getCurrentTime();
  if (t - lastPollTime > MaxWaitTime) {2
    lastPollTime = t;
    pollPlayerHint();
  }
}

bool pyrJumpHelper::SlashCommand (int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params) {
  command.tolower();
  if (command == "test") {
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(playerID);
    assert(b);
    bz_sendTextMessagef(BZ_SERVER, playerID, "Your state is %d", isPlayerGroundedOnPyrNeedingHint(b));
    bz_freePlayerRecord(b);
    return true;
  }
  else if (command == "height") {
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(playerID);
    assert(b);
    bz_sendTextMessagef(BZ_SERVER, playerID, "%.2f", b->lastKnownState.pos[2]);
    bz_freePlayerRecord(b);
    return true;
  }
  return false;
}
