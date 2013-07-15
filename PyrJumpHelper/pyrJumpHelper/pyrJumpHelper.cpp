// pyrJumpHelper.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include "plugin_utils.h"

#include <cmath>
using namespace std;

#define assert(x) { if (!x) bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Failed assertion: %s", #x); }

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

class pyrJumpHelper : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
  virtual const char* Name (){return "PyraJump Helper";}
  virtual void Init ( const char* config);

  void Cleanup();
  void Event ( bz_EventData *data);
  
  bool SlashCommand (int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params);
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
}

void pyrJumpHelper::Cleanup() {
  bz_removeCustomSlashCommand("test");
  Flush();
}

bool isPlayerGrounded(int id) {
  bz_BasePlayerRecord *b = bz_getPlayerByIndex(id);
  assert(b);
  bool ret = !b->lastKnownState.falling;
  bz_freePlayerRecord(b);
  return ret;
}

void pyrJumpHelper::Event(bz_EventData *data) {
  if (data->eventType == bz_eTickEvent) {
    // Poll each player and if they are on a pyramid and they haven't been notified within 2.5 seconds then give them a hint
  }
}

bool pyrJumpHelper::SlashCommand (int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params) {
  command.tolower();
  if (command == "test") {
    bz_sendTextMessagef(BZ_SERVER, playerID, "Your state is %d", isPlayerGrounded(playerID));
    return true;
  }
  return false;
}
