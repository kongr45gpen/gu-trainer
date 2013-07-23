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

static const Rect base = {0.0, 0.0, 35.0, 35.0, TAU / 8.0};

float distanceFromBase(float x, float y) {
  assert(abs(base.hw - base.hh) < LARGE_EPS);
  x -= base.x;
  y -= base.y;
  float x_ = x * cosf(-base.rot) - y * sinf(-base.rot);
  float y_ = x * sinf(-base.rot) + y * cosf(-base.rot);
  
  // Exploit symmetry using abs() in every case.
  
  if (abs(x_) <= base.hw && abs(y_) <= base.hh)
    return 0.0f; // Inside base
  
  if (abs(x_) <= base.hw)
    return abs(y_) - base.hh;
  if (abs(y_) <= base.hh)
    return abs(x_) - base.hw;
  
  return sqrtf(SQR(abs(x_) - base.hw) + SQR(abs(y_) - base.hh));
}

#define MaxNumPlayers 256
// Slightly higher than the time for jumping and hitting the roof and falling quickly
#define ADVICE_PERIOD 2.5f
// Have at least this delay between hints even when they are appropriate
#define SHORT_ADVICE_DELAY 1.0f

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
#define TANK_LENGTH (2.0 * TANK_HALFLENGTH)
#define TANK_HEIGHT 2.05
#define TANK_ANG_VEL (PI / 4.0)

#define TANK_EFFECTIVE_RADIUS_SQR (SQR(TANK_HALFWIDTH) + SQR(TANK_HALFLENGTH))
#define TANK_EFFECTIVE_RADIUS sqrt(TANK_EFFECTIVE_RADIUS_SQR)

enum HintType {
  eNoHint,
  eTooLow,
  eTooClose,
  eTooFar,
  eTooHard, // Too hard or need multiple jumps
  eSingleJump,
  eLowFpsRequired // A modifier state; combine this HintType with a SingleJump
};

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
  float lastPos[MaxNumPlayers][3]; // Where the player was last, to determine if they moved.
  HintType lastHint[MaxNumPlayers];

  // Negative z if they don't want to spawn at the saved position (or there is no saved position)
  // Fourth element is rotation
  float savedPos[MaxNumPlayers][4];
  
  void GiveHintIfNecessary(const bz_BasePlayerRecord *b);
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
  bz_registerCustomSlashCommand("save", this);
  bz_registerCustomSlashCommand("clear", this);
  bz_registerCustomSlashCommand("advice", this);
  bz_registerCustomSlashCommand("commands", this);
  
  lastPollTime = 0.0f;
  for (int i = 0; i < MaxNumPlayers; i++) {
    lastHintTime[i] = 0.0f;
    lastPos[i][2] = lastPos[i][1] = lastPos[i][0] = -1.0f;
    savedPos[i][2] = savedPos[i][1] = savedPos[i][0] = -1.0f;
    lastHint[i] = eNoHint;
  }
  intersectionTest1();
  intersectionTest2();
}

void pyrJumpHelper::Cleanup() {
  bz_removeCustomSlashCommand("test");
  bz_removeCustomSlashCommand("state");
  bz_removeCustomSlashCommand("save");
  bz_removeCustomSlashCommand("clear");
  bz_removeCustomSlashCommand("advice");
  bz_removeCustomSlashCommand("commands");
  Flush();
}

bool isPlayerGrounded(int id) {
  bz_BasePlayerRecord *b = bz_getPlayerByIndex(id);
  assert(b);
  bool ret = !b->lastKnownState.falling;
  bz_freePlayerRecord(b);
  return ret;
}

// They could use a hint if they are anywhere reasonably placed on the pyramid.
// This includes needing to tell them that their jump will inevitably fail because they're too far from the base, etc.
bool isPlayerHintable(const bz_BasePlayerRecord *b) {
  const bz_PlayerUpdateState &s = b->lastKnownState;
  if (!b->spawned || bz_isPlayerPaused(b->playerID) || s.falling) return false;
  if (s.pos[2] < PYR_HEIGHT_LOWEST || s.pos[2] > PYR_HEIGHT_HIGHEST) return false;
  // Luckily there is no other way a tank can be grounded and not be on a pyramid.
  return true;
}

struct Hint {
  HintType type;
  string msg;
};

// Efficient hint calculations only
Hint calculateCheapHint(const bz_BasePlayerRecord *b) {
  Hint ret = {eNoHint, ""};
  const bz_PlayerUpdateState &s = b->lastKnownState;
  if (s.pos[2] < MIN_PYR_HEIGHT) {
    return {eTooLow, "Too low to make the climb"};
  }
  int lowFpsRequired = 0;
  if (s.pos[2] < TYP_PYR_HEIGHT) {
    lowFpsRequired = 2;
  }
  else if (s.pos[2] < MAX_PYR_HEIGHT) {
    lowFpsRequired = 1;
  }
  
  float dist = distanceFromBase(s.pos[0], s.pos[1]);
  if (dist < TANK_HALFWIDTH) {
    return {eTooClose, "Move out from under the base"};
  }
  else if (dist > TANK_EFFECTIVE_RADIUS) {
    return {eTooFar, "Move closer to the base"};
  }
  if (!lowFpsRequired)
    return ret;
  
  return {eLowFpsRequired, lowFpsRequired == 1 ? "Low-ish FPS required. " : "Low FPS required. " };
}

Hint calculateExpensiveHint(const bz_BasePlayerRecord *b, Hint hint) {
  bz_PlayerUpdateState s = b->lastKnownState; // modifiable
  
  assert(hint.type == eLowFpsRequired || hint.type == eNoHint);
  
  // Instead of doing nasty calculations specific to the required FPS, just approximate the required turn speeds by pretending that they're high enough.
  // The turn speed margins will either be too small to be detected (practically impossible jump) or small enough to dissuade them anyway!
  if (s.pos[2] < MAX_PYR_HEIGHT + LARGE_EPS)
    s.pos[2] = MAX_PYR_HEIGHT + LARGE_EPS;
  
    // Some troll bzflag developer decided tank length is along x-axis at zero rotation
  Rect tank = { s.pos[0], s.pos[1], TANK_HALFLENGTH, TANK_HALFWIDTH, s.rotation };

  float lower = BASE_HEIGHT - 4.0f - TANK_HEIGHT - s.pos[2];
  float upper = BASE_HEIGHT - s.pos[2];
  
  // Solve s = u.t + a.t^2 / 2
  float A = -0.5f * GRAVITY;
  float B = JUMP_VEL;
  // Must not collide between t1 and t2
  float t1 = (-B + sqrtf(B*B + 4*A*lower)) / 2/A;
  float t2 = (-B + sqrtf(B*B + 4*A*upper)) / 2/A;
  assert(!isnan(t1));
  assert(!isnan(t2));
  assert(t1 <= t2);  
  
  // Must collide at t3
  float t3 = (-B - sqrtf(B*B + 4*A*upper)) / 2/A;
  assert(!isnan(t3));  

  const float dt = 0.02f;

  const int divisions = 101;
  assert(divisions & 1); // Need this for the 0 speed value as delimiter!
  vector<bool> success(divisions + 1, true);
  success[divisions] = false;
  success[divisions / 2] = false;
  for (int i = 0; i < divisions; i++) {
    if (i == divisions / 2) continue;
    
    // Start from anti-clockwise because people tend to associate negative with anti-clockwise.
    float ratio = -(-1.0f + 2.0f * i / (divisions - 1));

    float t = t1;
    do {
      Rect pos = tank;
      pos.rot += (TANK_ANG_VEL * ratio * t);
      if (intersects(pos, base)) {
        success[i] = false;
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
  int start, end;
  char buf[20];
  string left;
  for (int i = 0; i <= divisions / 2; i++) {
    if (!on && success[i]) {
      on = true;
      end = -100 + 200 * i / (divisions - 1);
    }
    else if (on && !success[i]) {
      on = false;
      start = -100 + 200 * (i - 1) / (divisions - 1);
      if (start != end)
        sprintf(buf, "%d-%d", -start, -end); // Make positive values
      else
        sprintf(buf, "%d", -start);
      if (!left.empty())
        left = "," + left;
      left = buf + left;
    }
  }
  string right;
  assert(!on);
  for (int i = divisions / 2 + 1; i <= divisions; i++) {
    if (!on && success[i]) {
      on = true;
      start = -100 + 200 * i / (divisions - 1);
    }
    else if (on && !success[i]) {
      on = false;
      end = -100 + 200 * (i - 1) / (divisions - 1);
      if (start != end)
        sprintf(buf, "%d-%d", start, end);
      else
        sprintf(buf, "%d", start);
      if (!right.empty())
        right += ",";
      right += buf;
    }
  }

  if (left.empty() && right.empty()) {
    return {eTooHard, "Multiple jumps required or too difficult"};
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

    // Add the potential low FPS message
    return {eSingleJump, hint.msg + msg.c_str()};
  }
  assert(0);
}

void pyrJumpHelper::GiveHintIfNecessary(const bz_BasePlayerRecord *b) {
  const bz_PlayerUpdateState &s = b->lastKnownState;
  
  // Never give hints if they didn't move
  if (SQR(s.pos[0] - lastPos[b->playerID][0]) + SQR(s.pos[1] - lastPos[b->playerID][1]) + SQR(s.pos[2] - lastPos[b->playerID][2]) < LARGE_EPS)
    return;
  
  // Update the last position record
  for (int i = 0; i < 3; i++)
    lastPos[b->playerID][i] = s.pos[i];

  // Never give hints too often, even if new
  if (bz_getCurrentTime() - lastHintTime[b->playerID] < SHORT_ADVICE_DELAY)
    return;
    
  Hint hint = calculateCheapHint(b);
  
  // Don't repeat the same hint too often
  if (hint.type == lastHint[b->playerID] && bz_getCurrentTime() - lastHintTime[b->playerID] < ADVICE_PERIOD)
    return;
  
  if (hint.type == eNoHint || hint.type == eLowFpsRequired) {
    hint = calculateExpensiveHint(b, hint);
  }
  
  assert(hint.type != eNoHint);
  
  logMsg(b->playerID, hint.msg.c_str());
  lastHintTime[b->playerID] = bz_getCurrentTime();
  lastHint[b->playerID] = hint.type;
}

// Poll each player to determine whether they need advice
void pyrJumpHelper::pollPlayerHint() {
  float t = bz_getCurrentTime();
  bz_APIIntList *l = bz_getPlayerIndexList();
  for (int i = 0; i < l->size(); i++) {
    int id = l->get(i);
    
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(id);
    assert(b);
    if (isPlayerHintable(b)) {
      GiveHintIfNecessary(b);
    }
    bz_freePlayerRecord(b);
  }
  delete l;
}

void pyrJumpHelper::Event(bz_EventData *data) {
  if (data->eventType == bz_ePlayerJoinEvent) {
    auto d = static_cast<bz_PlayerJoinPartEventData_V1 *>(data);
    if (bz_getPlayerTeam(d->playerID) != eObservers) {
      bz_sendTextMessage(BZ_SERVER, d->playerID, "Use /commands to view the available commands");
      savedPos[d->playerID][2] = -1.0f; // In case an observer did /save to sabotage!
    }
  }
  else if (data->eventType == bz_ePlayerUpdateEvent) {
    // If they touch ground, can give them the next hint freely
    auto d = static_cast<bz_PlayerUpdateEventData_V1 *>(data);
    if (d->state.pos[2] < EPS)
      lastHintTime[d->playerID] -= ADVICE_PERIOD;
  }
  else if (data->eventType == bz_eGetPlayerSpawnPosEvent) {
    auto d = static_cast<bz_GetPlayerSpawnPosEventData_V1 *>(data);
    if (savedPos[d->playerID][2] >= 0.0f) {
      d->pos[0] = savedPos[d->playerID][0];
      d->pos[1] = savedPos[d->playerID][1];
      d->pos[2] = savedPos[d->playerID][2] + LARGE_EPS; // Don't spawn inside a building
      d->rot = savedPos[d->playerID][3];
      d->handled = true;
    }
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
    // Abandoned test was gonna test flying demonstration but got lazy
    /*float start = bz_getCurrentTime();
    const float delay = 0.05;
    float prev = 0;
    while (bz_getCurrentTime() - start < 1.0f) {
      if (bz_getCurrentTime() - prev > delay) {
        
      }
    }*/
    
    logMsgf(playerID, "Needing hint: %s", isPlayerHintable(b) ? "True" : "False");
    bz_freePlayerRecord(b);
    return true;
  }
  else if (command == "state") {
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(playerID);
    assert(b);
    bz_sendTextMessagef(BZ_SERVER, playerID, "Pos %.2f %.2f %.2f rot %.2f", b->lastKnownState.pos[0], b->lastKnownState.pos[1], b->lastKnownState.pos[2], b->lastKnownState.rotation);
    bz_freePlayerRecord(b);
    return true;
  }
  else if (command == "save") {
    // TODO: Extrapolate.
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(playerID);
    
    // Don't let the player /save when they are exploding or dead
    if (!b->spawned) {
      bz_sendTextMessage(BZ_SERVER, playerID, "You can only save your position when you are alive");
      bz_freePlayerRecord(b);
      return true;
    }
    
    for (int i = 0; i < 3; i++)
      savedPos[playerID][i] = b->lastKnownState.pos[i];
    savedPos[playerID][3] = b->lastKnownState.rotation;
    bz_sendTextMessage(BZ_SERVER, playerID, "Your current position was saved successfully");
    bz_freePlayerRecord(b);
    return true;
  }
  else if (command == "clear") {
    savedPos[playerID][2] = -1.0f;
    bz_sendTextMessage(BZ_SERVER, playerID, "Your saved position has been cleared");
    return true;
  }
  else if (command == "advice") {
    static float pads[][3] = {
      {-85.0f, -2.4f, 0.0f},
      {23.6f, 48.06f, 0.0f},
    };
    static string advice[] = {
      "Face 90 degrees. Drive and jump forward full speed with no turn. 2nd jump full speed to the right.",
      "Face 90 degrees. Drive and jump backward full speed with no turn. 2nd jump full speed to the right.",
    };
    bz_BasePlayerRecord *b = bz_getPlayerByIndex(playerID);
    const bz_PlayerUpdateState &s = b->lastKnownState;
    int i;
    for (i = 0; i < sizeof(pads) / sizeof(pads[0]); i++) {
      if (SQR(s.pos[0] - pads[i][0]) + SQR(s.pos[1] - pads[i][1]) + SQR(s.pos[2] - pads[i][2]) < SQR(2.0 * TANK_LENGTH)) {
        break;
      }
    }
    if (i < sizeof(pads) / sizeof(pads[0])) { // found
      bz_sendTextMessage(BZ_SERVER, playerID, advice[i].c_str());
    }
    bz_freePlayerRecord(b);
    return true;
  }
  else if (command == "commands") {
    bz_sendTextMessage(BZ_SERVER, playerID, "advice - Stand on a purple jump pad and reveal the jumping routine");
    bz_sendTextMessage(BZ_SERVER, playerID, "clear  - Clear a saved spawn; spawn normally");
    bz_sendTextMessage(BZ_SERVER, playerID, "save   - Save the current tank position and orientation for subsequent spawns");
    return true;
  }
  return false;
}
