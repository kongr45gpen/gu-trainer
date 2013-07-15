// pyrJumpHelper.cpp : Defines the entry point for the DLL application.
//

#include "bzfsAPI.h"
#include "plugin_utils.h"

#define assert(x) { if (!x) bz_sendTextMessagef(BZ_SERVER, BZ_ALLUSERS, "Failed assertion: %s", #x); }

class pyrJumpHelper : public bz_Plugin, public bz_CustomSlashCommandHandler
{
public:
  virtual const char* Name (){return "PyraJump Helper";}
  virtual void Init ( const char* config);

  void Cleanup();
  virtual void Event ( bz_EventData * /* eventData */ ){return;}
  
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

bool pyrJumpHelper::SlashCommand (int playerID, bz_ApiString command, bz_ApiString message, bz_APIStringList *params) {
  command.tolower();
  if (command == "test") {
    bz_sendTextMessagef(BZ_SERVER, playerID, "Your state is %d", isPlayerGrounded(playerID));
    return true;
  }
  return false;
}
