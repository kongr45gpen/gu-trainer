// pyrJumpHelper.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include "plugin_utils.h"

#include <cmath>
#include <string>
using namespace std;

#define assert(x) { \
 if (!(x)) { \
   bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Failed assertion line %d: %s", __LINE__, #x); \
   bz_debugMessagef(0, "Failed assertion line %d: %s", __LINE__, #x); \
 } \
}

#define logMsg(playerID, format) { \
  bz_sendTextMessage(BZ_SERVER, playerID, format); \
  bz_debugMessage(3, format); \
}
                  
#define logMsgf(playerID, format, ...) { \
  bz_sendTextMessagef(BZ_SERVER, playerID, format, __VA_ARGS__); \
  bz_debugMessagef(3, format, __VA_ARGS__); \
}

#define logf(format, ...) { \
  bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, format, __VA_ARGS__); \
  bz_debugMessagef(3, format, __VA_ARGS__); \
}
                  
#define SQR(x) ((x) * (x))

#define EPS (1e-6)
#define LARGE_EPS (1e-3)

#define PI acos(-1.0)
#define TAU (2.0 * PI)

struct Rect {
  float x; // center x
  float y; // center y
  float hw; // halfwidth
  float hh; // halfheight
  float rot; // angle of the rectangle, with 0 radians having width along x axis
};

// Intervals [a, b], [c, d]
bool overlap(float a, float b, float c, float d) {
  bz_debugMessagef(5, "Regions %.2f,%.2f, %.2f,%.2f", a, b, c, d);
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
  bz_debugMessagef(5, "B relative to A is (%.2f, %.2f)", b_x, b_y);
  // B center relative to unrotated A
  float b__x = b_x * cosf(-a.rot) - b_y * sinf(-a.rot);
  float b__y = b_x * sinf(-a.rot) + b_y * cosf(-a.rot);
  bz_debugMessagef(5, "B relative to A unrotated by %.2f is (%.2f, %.2f)", a.rot, b__x, b__y);
  bz_debugMessagef(5, "Brot %.2f", b.rot - a.rot);
  // B corners relative to unrotated A
  float b_rot = b.rot - a.rot;
  float bx[4] = {
    b__x + b.hw * cosf(b_rot) - b.hh * sinf(b_rot),
    b__x + b.hw * cosf(b_rot) + b.hh * sinf(b_rot),
    b__x - b.hw * cosf(b_rot) - b.hh * sinf(b_rot),
    b__x - b.hw * cosf(b_rot) + b.hh * sinf(b_rot)
  };
  bz_debugMessagef(5, "Projections %.2f %.2f %.2f %.2f", bx[0], bx[1], bx[2], bx[3]);
  if (!projectionsOverlap(bx, a.hw)) return false;

  float by[4] = {
    b__y + b.hw * sinf(b_rot) + b.hh * cosf(b_rot),
    b__y + b.hw * sinf(b_rot) - b.hh * cosf(b_rot),
    b__y - b.hw * sinf(b_rot) + b.hh * cosf(b_rot),
    b__y - b.hw * sinf(b_rot) - b.hh * cosf(b_rot)
  };
  bz_debugMessagef(5, "Projections %.2f %.2f %.2f %.2f", by[0], by[1], by[2], by[3]);
  if (!projectionsOverlap(by, a.hh)) return false;
  return true;
}

// FIXME: Untested
bool intersects(Rect a, Rect b) {
  return intersectsHelper(a, b) && intersectsHelper(b, a);
}

bool intersectionTest1() {
  Rect a = { 4.5, 1.0, 5.0, 2.5, PI / 180.0 * -36.869897645844021296855612559093 };
  Rect b = { 3.0, 11.0, sqrtf(200.0) / 2.0f, sqrtf(32.0) / 2.0f, -TAU / 8.0f };
  bool ret = !intersects(a, b);
  assert(ret);
  bz_debugMessagef(0, "IntersectionTest1 %s", ret ? "passed" : "failed");
  return ret;
}

bool intersectionTest2() {
  Rect a = { 0.0, 0.0, 1.0, 2.0, 0.0 };
  Rect b = { 1.5, 1.5, sqrtf(2.0), sqrtf(2.0) / 2.0f, -TAU / 8.0f };
  bool ret = intersects(a, b);
  assert(ret);
  bz_debugMessagef(0, "IntersectionTest2 %s", ret ? "passed" : "failed");
  return ret;
}

static Rect base = {0.0, 0.0, 35.0, 35.0, TAU / 8.0};

#define MaxNumPlayers 256
// Slightly higher than the time for jumping and hitting the roof and falling quickly
#define ADVICE_PERIOD 2.5f

// Above and below this they probably know very well that they won't make the jump so don't pester them
#define PYR_HEIGHT_LOWEST 5
// Actually you can't even land this high even if you jumped from a block (which is disabled on the map)
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

#define TANK_HALFWIDTH 1.4
#define TANK_HALFLENGTH 3.0
#define TANK_HEIGHT 2.05
#define TANK_ANG_VEL (PI / 4.0)

#define TANK_EFFECTIVE_RADIUS_SQR (SQR(TANK_HALFWIDTH) + SQR(TANK_HALFLENGTH))

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
  // FIXME: Record last hint so as not to tell the player twice that they're too low.
  
  void GiveHint(bz_BasePlayerRecord *b);
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
  bz_registerCustomSlashCommand("state", this);
  
  for (int i = 0; i < MaxNumPlayers; i++)
    lastHintTime[i] = 0.0f;
  intersectionTest1();
  intersectionTest2();
}

void pyrJumpHelper::Cleanup() {
  bz_removeCustomSlashCommand("test");
  bz_removeCustomSlashCommand("state");
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
void pyrJumpHelper::GiveHint(bz_BasePlayerRecord *b) {
  bz_PlayerUpdateState &s = b->lastKnownState;
  // Too much work to simulate for several FPSes.
  // Only use the infinite FPS calculation.
  if (s.pos[2] < MIN_PYR_HEIGHT) {
    bz_sendTextMessage(BZ_SERVER, b->playerID, "Too low to make the climb");
    lastHintTime[b->playerID] = bz_getCurrentTime();
    return;
  }
  else if (s.pos[2] < TYP_PYR_HEIGHT) {
    logMsg(b->playerID, "You need a low framerate to jump high enough");
  }
  else if (s.pos[2] < MAX_PYR_HEIGHT) {
    logMsg(b->playerID, "You need a low-ish framerate to jump high enough");
  }
  // Instead of doing nasty calculations specific to the required FPS, just approximate the required turn speeds by pretending that they're high enough.
  // The turn speed margins will either be too small to be detected (practically impossible jump) or small enough to dissuade them anyway!
  if (s.pos[2] < MAX_PYR_HEIGHT + LARGE_EPS)
    s.pos[2] = MAX_PYR_HEIGHT + LARGE_EPS;
  
  // Tell them "Too far from platform" if they are 1) not within the base by a radius 2) their distance from the four parametric finite lines of the base
  // This is a bit difficult and maybe shouldn't bother.
  
  // Some troll decided length is along x-axis at zero rotation
  Rect tank = { s.pos[0], s.pos[1], TANK_HALFLENGTH, TANK_HALFWIDTH, s.rotation };
  //logf("tankpos %.2f, %.2f, %.2f hw %.2f hh %.2f rot %.2f", tank.x, tank.y, s.pos[2], tank.hw, tank.hh, tank.rot); 

  float lower = BASE_HEIGHT - 4.0f - TANK_HEIGHT - s.pos[2];
  float upper = BASE_HEIGHT - s.pos[2];
  
  // Solve s = u.t + a.t^2 / 2
  float A = -0.5f * GRAVITY;
  float B = JUMP_VEL;
  // Must not collide between t1 and t2
  float t1 = (-B + sqrtf(B*B + 4*A*lower)) / 2/A;
  float t2 = (-B + sqrtf(B*B + 4*A*upper)) / 2/A;
  //logf("t1 %.2f t2 %.2f", t1, t2);
  assert(!isnan(t1));
  assert(!isnan(t2));
  assert(t1 <= t2);  
  
  // Must collide at t3
  float t3 = (-B - sqrtf(B*B + 4*A*upper)) / 2/A;
  assert(!isnan(t3));  

  const float dt = 0.01f;

  const int divisions = 201;
  assert(divisions & 1); // Need this for the 0 speed value as delimiter!
  vector<bool> success(divisions + 1, true);
  success[divisions] = false;
  success[divisions / 2] = false;
  for (int i = 0; i < divisions; i++) {
    if (i == divisions / 2) continue;
    
    // Start from anti-clockwise because people tend to associate negative with anti-clockwise.
    float ratio = -(-1.0f + 2.0f * i / (divisions - 1));

    float t = t1;
    //logf("Div %d, t1 %.2f t2 %.2f", i, t1, t2);
    do {
      Rect pos = tank;
      pos.rot += (TANK_ANG_VEL * ratio * t);
      //bz_debugMessagef(0, "Rot %.2f t %.2f", pos.rot, t);
      //bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Rot %.2f t %.2f", pos.rot, t);
      if (intersects(pos, base)) {
        success[i] = false;
        //logf("%d intersects", i);
        break;
      }
      
      t = min(t + dt, t2);
    } while (t < t2);
    
    if (!success[i]) continue;
    
    // Must collide at t3
    Rect pos = tank;
    pos.rot += (TANK_ANG_VEL * ratio * t3);
    success[i] = intersects(pos, base);
  }
  bool on = false;
  int end;
  char buf[20];
  string left;
  for (int i = 0; i <= divisions / 2; i++) {
    if (!on && success[i]) {
      on = true;
      end = -100 + 200 * i / (divisions - 1);
    }
    else if (on && !success[i]) {
      on = false;
      sprintf(buf, "%d-%d", -(-100 + 200 * (i - 1) / (divisions - 1)), -end); // Make positive values
      if (!left.empty())
        left = "," + left;
      left = buf + left;
    }
  }
  int start;
  string right;
  assert(!on);
  for (int i = divisions / 2 + 1; i <= divisions; i++) {
    if (!on && success[i]) {
      on = true;
      start = -100 + 200 * i / (divisions - 1);
    }
    else if (on && !success[i]) {
      on = false;
      sprintf(buf, "%d-%d", start, -100 + 200 * (i - 1) / (divisions - 1));
      if (!right.empty())
        right += ",";
      right += buf;
    }
  }

  if (left.empty() && right.empty()) {
    logMsg(b->playerID, "Can't climb from here");
  }
  else {
    string msg = "% turn speed to climb up: ";
    if (!left.empty()) {
      msg += "Left: ";
      msg += left;
      msg += " ";
    }
    if (!right.empty()) {
      msg += "Right: ";
      msg += right;
    }
    // FIXME: Never send the exact same "% turn speed" message twice.
    logMsg(b->playerID, msg.c_str());
  }
  lastHintTime[b->playerID] = bz_getCurrentTime();
}

// Poll each player to determine whether they need advice
void pyrJumpHelper::pollPlayerHint() {
  float t = bz_getCurrentTime();
  bz_APIIntList *l = bz_getPlayerIndexList();
  for (int i = 0; i < l->size(); i++) {
    int id = l->get(i);

    // FIXME: If they adjust their position from too high to OK, should tell them
    if (t - lastHintTime[id] < ADVICE_PERIOD) // Don't spam them with advice
      continue;
    
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(id);
    assert(b);
    if (isPlayerGroundedOnPyrNeedingHint(b)) {
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
  if (t - lastPollTime > MaxWaitTime) {
    lastPollTime = t;
    pollPlayerHint();
  }
}

bool pyrJumpHelper::SlashCommand (int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params) {
  command.tolower();
  if (command == "test") {
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(playerID);
    assert(b);
    logf("Needing hint: %s", isPlayerGroundedOnPyrNeedingHint(b) ? "True" : "False");
    bz_freePlayerRecord(b);
    return true;
  }
  else if (command == "state") {
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(playerID);
    assert(b);
    bz_sendTextMessagef(BZ_SERVER, playerID, "z %.2f r %.2f", b->lastKnownState.pos[2], b->lastKnownState.rotation);
    bz_freePlayerRecord(b);
    return true;
  }
  return false;
}
